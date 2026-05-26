#include "render/scene_renderer.h"
#include <cassert>
#include "common/constants.h"
#include "material/material.h"
#include "object/light.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "render/device_context.h"
#include "render/mesh_renderer.h"
#include "render/render_target.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include <spdlog/spdlog.h>
#include <vector>

namespace SJH
{
	void SceneRenderer::Render(RenderTarget & /*target*/)
	{
		// Phase B: target 미사용 — 모든 Camera 가 자기 FBO 를 보유. IRenderStage 계약 파라미터 유지.
		// 1. SceneContext 에서 Camera 컬렉션 직접 조회 — DFS 폐기 (Cocos2D `Scene::_cameras` 정통).
		const auto &cameras = Scene::Director::Get().GetContext().GetCameras();
		if (cameras.empty())
		{
			spdlog::warn("SceneRenderer::Render — SceneContext 에 Camera 0 — 프레임 skip.");
			return;
		}

		// 2. addCamera 호출 순서 그대로 렌더 (Cocos2D `addChild` 정통). std::sort 폐기.
		//    render-time IsEnabled filter (Component::SetEnabled(false) 가 SceneContext 에 영향 X).
		for (auto *cam : cameras)
		{
			if (cam->IsEnabled())
				RenderWithCamera(*cam);
		}
	}

	void SceneRenderer::RenderWithCamera(Scene::Camera &cam)
	{
		// Phase B: 모든 Camera 는 자기 RenderTarget 을 보유 (Camera::SetTargetRenderTarget assert 보장).
		assert(cam.GetTargetRenderTarget() != nullptr);
		auto &rc = DeviceContext::Get();

		RenderTarget &target = *cam.GetTargetRenderTarget();
		rc.BeginFrame(target);

		const auto viewMat = cam.GetViewMatrix();
		const auto projMat = cam.GetProjectionMatrix();
		const auto cullingMask = cam.CullingMask;
		DirLight *dir;
		std::vector<PointLight *> points;
		std::vector<SpotLight *> spots;

		// 카메라 뷰 계산을 위해 position 획득
		vmath::vec3 viewPos(0.0f, 0.0f, 0.0f);
		if (auto *camOwner = cam.GetOwner())
		{
			const auto camWorld = camOwner->GetWorldMatrix();
			viewPos = vmath::vec3(camWorld[3][0], camWorld[3][1], camWorld[3][2]);
		}
		
		// 라이트 수집
		auto &ctx = Scene::Director::Get().GetContext();
		dir = (ctx.GetDirLight() && ctx.GetDirLight()->IsEnabled())
		          ? ctx.GetDirLight()
		          : nullptr;
		points.reserve(ctx.GetPointLights().size());
		for (auto *l : ctx.GetPointLights())
			if (l->IsEnabled())
				points.push_back(l);

		spots.reserve(ctx.GetSpotLights().size());
		for (auto *l : ctx.GetSpotLights())
			if (l->IsEnabled())
				spots.push_back(l);

		auto programs = ResourceRegistry::Get().GetAllPrograms();

		SendLightUniforms(programs, dir, points, spots, viewPos);

		mProcessor.Clear();
		CollectFromActor(Scene::Director::Get().Root(), viewMat, cullingMask);
		mProcessor.SortMultiStage();
		mProcessor.Process(rc, viewMat, projMat);
	}

	void SceneRenderer::SendLightUniforms(const std::vector<Program *> &programs,
	                                      DirLight *dir,
	                                      const std::vector<PointLight *> &points,
	                                      const std::vector<SpotLight *> &spots,
	                                      const vmath::vec3 &viewPos)
	{

		if (static_cast<int>(points.size()) > Const::MAX_POINT_LIGHTS)
			spdlog::warn("SceneRenderer — PointLight {} 개 발견. 셰이더 MAX_POINT_LIGHTS={} 초과분 무시.",
			             points.size(), Const::MAX_POINT_LIGHTS);
		if (static_cast<int>(spots.size()) > Const::MAX_SPOT_LIGHTS)
			spdlog::warn("SceneRenderer — SpotLight {} 개 발견. 셰이더 MAX_SPOT_LIGHTS={} 초과분 무시.",
			             spots.size(), Const::MAX_SPOT_LIGHTS);

		auto &rc = DeviceContext::Get();
		for (const auto *prog : programs)
		{
			if (!prog)
				continue;
			// Lighting schema sentinel — UNI_VIEW_POS 가 program 의 UniformCache 에 없으면
			//  본 program 은 lighting 미사용 (simple/window/postfx 등) -> 송신 통째 skip.
			//  -> warn-once 노이즈 차단 + glUseProgram 비용 회피.
			if (prog->GetLocation(Const::UNI_VIEW_POS) < 0)
				continue;
			rc.UseProgram(*prog);

			// viewPos — Phong specular 계산용.
			Uniforms::SetVec3(*prog, Const::UNI_VIEW_POS, viewPos);

			// DirLight — 1개. 없으면 enabled=0 만 전송 (uniform 0 보장).
			if (dir)
			{
				Uniforms::SetDirLight(*prog, Const::UNI_DIR_LIGHT, *dir, dir->GetWorldDirection());
				Uniforms::SetInt(*prog, Const::UNI_DIR_LIGHT_ENABLED, 1);
			}
			else
			{
				Uniforms::SetInt(*prog, Const::UNI_DIR_LIGHT_ENABLED, 0);
			}

			// PointLights — 최대 MAX_POINT_LIGHTS 개. 초과는 무시. 부족하면 enabled=0 으로 slot 채움.
			for (std::size_t i = 0; i < static_cast<std::size_t>(Const::MAX_POINT_LIGHTS); ++i)
			{
				const std::string idxStr = Const::UNI_POINT_LIGHTS_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				const std::string enStr = Const::UNI_POINT_LIGHTS_ENABLED_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				if (i < points.size())
				{
					Uniforms::SetPointLight(*prog, idxStr.c_str(), *points[i],
					                        points[i]->GetWorldPosition());
					Uniforms::SetInt(*prog, enStr.c_str(), 1);
				}
				else
				{
					Uniforms::SetInt(*prog, enStr.c_str(), 0);
				}
			}

			// SpotLights — 최대 MAX_SPOT_LIGHTS 개. 초과는 무시. 부족하면 enabled=0 으로 slot 채움.
			// (PointLights 와 완전 동일 패턴 — 셰이더 spotLights[]/spotLightsEnabled[] 배열.)
			for (std::size_t i = 0; i < static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS); ++i)
			{
				const std::string idxStr = Const::UNI_SPOT_LIGHTS_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				const std::string enStr = Const::UNI_SPOT_LIGHTS_ENABLED_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				if (i < spots.size())
				{
					Uniforms::SetSpotLight(*prog, idxStr.c_str(), *spots[i],
					                       spots[i]->GetWorldPosition(),
					                       spots[i]->GetWorldDirection());
					Uniforms::SetInt(*prog, enStr.c_str(), 1);
				}
				else
				{
					Uniforms::SetInt(*prog, enStr.c_str(), 0);
				}
			}
		}
	}

	void SceneRenderer::CollectFromActor(const Scene::Actor &actor,
	                                     const vmath::mat4 &viewMat,
	                                     uint64_t cullingMask)
	{
		if (!actor.IsActive())
			return;

		// SP4 D-15: cullingMask 비트 AND 로 actor 의 layer 가 카메라 가시 여부 판정.
		// 0 이면 본인은 skip — 단 자식은 layer 가 다를 수 있으므로 계속 traverse.
		const bool visibleToCamera = (cullingMask & actor.GetLayer()) != 0ull;

		if (visibleToCamera)
		{
			if (auto *mr = actor.GetComponent<Scene::MeshRenderer>())
			{
				if (mr->IsEnabled() && mr->Visible && mr->Mesh && mr->Material)
				{
					const vmath::mat4 model = actor.GetWorldMatrix();
					// view-space origin z: (viewMat * model) 의 4번째 열(translation) z 성분.
					// vmath 는 mat*vec 오버로드 미제공 -> mat4 직접 인덱싱으로 depth 추출.
					const float depthZ = (viewMat * model)[3][2];
					// SSoT — DrawCommand 는 MeshRenderer 만 의존. program/mesh/material/actor 는
					//   MeshPassProcessor::Process 에서 mr 경유로 추출 (Filament/Unreal/Cocos 정통).
					//   per-frame 가변 데이터 (modelMatrix/queueLayer/depth) 만 별도 보관.
					DrawCommand cmd;
					cmd.meshRenderer = mr;
					cmd.modelMatrix = model;
					// 최종 queue = Material.GetQueueLayer() + mr.QueueOffset (Unity Renderer.sortingOrder).
					cmd.queueLayer = mr->Material->GetQueueLayer() + mr->QueueOffset;
					cmd.depth = depthZ;
					// SP-MaterialSSoT — per-actor GL state override 제거. Material.PassKind 가 SSoT.
					mProcessor.Submit(cmd);
				}
			}
		}

		for (const auto &child : actor.GetChildren())
			CollectFromActor(*child, viewMat, cullingMask);
	}
} // namespace SJH
