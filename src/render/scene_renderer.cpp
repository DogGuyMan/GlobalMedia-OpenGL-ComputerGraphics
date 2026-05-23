#include "render/scene_renderer.h"
#include "buffer/framebuffer.h"
#include "common/constants.h" // UNI_* / NUM_POINT_LIGHTS — 매직 스트링 차단.
#include "material/material.h"
#include "object/light.h" // SP5 — DirLight/PointLight/SpotLight (Scene::Component)
#include "program/program.h"
#include "program/program_uniforms.h" // SP5 — Uniforms::SetDirLight/SetPointLight/SetSpotLight
#include "render/device_context.h"
#include "render/mesh_renderer.h"
#include "render/render_target.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <unordered_set>
#include <vector>

namespace SJH
{
	void SceneRenderer::Render(RenderTarget &defaultTarget)
	{
		// 1. Actor 트리 DFS 로 모든 Camera 컴포넌트 수집.
		std::vector<Scene::Camera *> cameras;
		CollectCameras(Scene::Director::Get().Root(), cameras);

		if (cameras.empty())
		{
			spdlog::warn("SceneRenderer::Render — 씬 트리에 Camera 컴포넌트 없음. 프레임 skip.");
			return;
		}

		// 2. Unity Camera.depth 정렬 — 작은 값 먼저.
		std::sort(cameras.begin(), cameras.end(),
		                 [](Scene::Camera *a, Scene::Camera *b) { return a->Depth < b->Depth; });

		// 3. 각 Camera 마다 1패스 실행 — target FB / view / proj 자동.
		for (auto *cam : cameras)
			RenderWithCamera(*cam, defaultTarget);
	}

	void SceneRenderer::CollectCameras(const Scene::Actor &actor,
	                                   std::vector<Scene::Camera *> &out)
	{
		if (!actor.IsActive())
			return;

		if (auto *cam = actor.GetComponent<Scene::Camera>())
			if (cam->IsEnabled())
				out.push_back(cam);

		for (const auto &child : actor.GetChildren())
			CollectCameras(*child, out);
	}

	void SceneRenderer::RenderWithCamera(Scene::Camera &cam, RenderTarget &defaultTarget)
	{
		auto &rc = DeviceContext::Get();

		RenderTarget &target = cam.GetTargetFramebuffer()
		                           ? static_cast<RenderTarget &>(*cam.GetTargetFramebuffer())
		                           : defaultTarget;
		rc.BeginFrame(target);

		const auto viewMat = cam.GetViewMatrix();
		const auto projMat = cam.GetProjectionMatrix();
		const auto cullingMask = cam.CullingMask;

		vmath::vec3 viewPos(0.0f, 0.0f, 0.0f);
		if (auto *camOwner = cam.GetOwner())
		{
			const auto camWorld = camOwner->GetWorldMatrix();
			viewPos = vmath::vec3(camWorld[3][0], camWorld[3][1], camWorld[3][2]);
		}

		DirLight *dir = nullptr;
		std::vector<PointLight *> points;
		SpotLight *spot = nullptr;
		CollectLights(Scene::Director::Get().Root(), dir, points, spot);

		std::unordered_set<const Program *> programs;
		CollectPrograms(Scene::Director::Get().Root(), programs);

		SendLightUniforms(programs, dir, points, spot, viewPos);

		mProcessor.Clear();
		CollectFromActor(Scene::Director::Get().Root(), viewMat, cullingMask);
		mProcessor.SortMultiStage();
		mProcessor.Process(rc, viewMat, projMat);
	}

	void SceneRenderer::CollectLights(const Scene::Actor &actor,
	                                  DirLight *&outDir,
	                                  std::vector<PointLight *> &outPoints,
	                                  SpotLight *&outSpot)
	{
		if (!actor.IsActive())
			return;

		if (auto *l = actor.GetComponent<DirLight>())
		{
			if (l->IsEnabled())
			{
				if (outDir == nullptr)
					outDir = l;
				else
					spdlog::warn("SceneRenderer::CollectLights — DirLight 중복 발견. 첫 1개만 사용.");
			}
		}
		if (auto *l = actor.GetComponent<PointLight>())
		{
			if (l->IsEnabled())
				outPoints.push_back(l);
		}
		if (auto *l = actor.GetComponent<SpotLight>())
		{
			if (l->IsEnabled())
			{
				if (outSpot == nullptr)
					outSpot = l;
				else
					spdlog::warn("SceneRenderer::CollectLights — SpotLight 중복 발견. 첫 1개만 사용.");
			}
		}

		for (const auto &child : actor.GetChildren())
			CollectLights(*child, outDir, outPoints, outSpot);
	}

	void SceneRenderer::CollectPrograms(const Scene::Actor &actor,
	                                    std::unordered_set<const Program *> &out)
	{
		if (!actor.IsActive())
			return;

		if (auto *mr = actor.GetComponent<Scene::MeshRenderer>())
		{
			if (mr->IsEnabled() && mr->Material && mr->Material->GetProgram())
				out.insert(mr->Material->GetProgram());
		}

		for (const auto &child : actor.GetChildren())
			CollectPrograms(*child, out);
	}

	void SceneRenderer::SendLightUniforms(const std::unordered_set<const Program *> &programs,
	                                      DirLight *dir,
	                                      const std::vector<PointLight *> &points,
	                                      SpotLight *spot,
	                                      const vmath::vec3 &viewPos)
	{
		// lighting.fs 의 셰이더 컨벤션 매핑 (Const::NUM_POINT_LIGHTS 와 일치).
		// 누락 uniform 은 첫 호출 1회 warn (Diagnostics::UniformDiagnostics) — lighting.fs 사용
		//   안 하는 program (e.g., simple.fs) 은 모든 light uniform 누락 warn 정상.
		if (static_cast<int>(points.size()) > Const::NUM_POINT_LIGHTS)
			spdlog::warn("SceneRenderer — PointLight {} 개 발견. 셰이더 NUM_POINT_LIGHTS={} 초과분 무시.",
			             points.size(), Const::NUM_POINT_LIGHTS);

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

			// PointLights — 최대 NUM_POINT_LIGHTS 개. 초과는 무시. 부족하면 enabled=0 으로 slot 채움.
			for (std::size_t i = 0; i < static_cast<std::size_t>(Const::NUM_POINT_LIGHTS); ++i)
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

			// SpotLight — 1개.
			if (spot)
			{
				Uniforms::SetSpotLight(*prog, "spotLight", *spot,
				                       spot->GetWorldPosition(), spot->GetWorldDirection());
				Uniforms::SetInt(*prog, "spotLightEnabled", 1);
			}
			else
			{
				Uniforms::SetInt(*prog, "spotLightEnabled", 0);
			}
		}
	}

	void SceneRenderer::Render(RenderTarget &defaultTarget,
	                           const vmath::mat4 &viewMat, const vmath::mat4 &projMat)
	{
		// 테스트/디버그 overlay 용 — CameraComponent 우회. 모든 layer 그림 (SP3.5 호환).
		auto &rc = DeviceContext::Get();
		rc.BeginFrame(defaultTarget);
		mProcessor.Clear();
		CollectFromActor(Scene::Director::Get().Root(), viewMat, ~0u);
		mProcessor.SortMultiStage();
		mProcessor.Process(rc, viewMat, projMat);
	}

	void SceneRenderer::CollectFromActor(const Scene::Actor &actor,
	                                     const vmath::mat4 &viewMat,
	                                     uint32_t cullingMask)
	{
		if (!actor.IsActive())
			return;

		// SP4 D-15: cullingMask 비트 AND 로 actor 의 layer 가 카메라 가시 여부 판정.
		// 0 이면 본인은 skip — 단 자식은 layer 가 다를 수 있으므로 계속 traverse.
		const bool visibleToCamera = (cullingMask & actor.GetLayer()) != 0u;

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
					DrawCommand cmd;
					cmd.program = mr->Material->GetProgram();
					cmd.mesh = mr->Mesh;
					cmd.material = mr->Material;
					cmd.modelMatrix = model;
					// Filament/Unreal/Cocos 정통 — 진실의 원천 단일화:
					//  , Material.GetPass() = "어떤 종류" (Pass::Kind enum, private 캡슐화)
					//  , Material.GetQueueLayer() = Pass::QueueOf(GetPass()) 도출 (alias)
					//  , MeshRenderer.QueueOffset = "같은 Material 의 인스턴스 간 미세 순서" (Unity Renderer.sortingOrder)
					//
					//  최종 = Material.GetQueueLayer() + mr.QueueOffset.
					cmd.queueLayer = mr->Material->GetQueueLayer() + mr->QueueOffset;
					cmd.actor = &actor;
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
