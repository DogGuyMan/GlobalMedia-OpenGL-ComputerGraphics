#include "render/scene_renderer.h"
#include "material/material.h"
#include "object/light.h"
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
	void SceneRenderer::Render(RenderTarget & /*defaultTarget*/)
	{
		// defaultTarget 파라미터는 IRenderStage 인터페이스 준수를 위해 유지 (ScreenQuadStage 등 다른
		// Stage 는 이 target 을 출력으로 사용). SceneRenderer 는 각 Camera 의 전용 RT 만 사용한다.

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

	// std::vector<Scene::Camera *> cameras;
	// public void AddCamera(Scene::Camera * camera) { cameras.push_back(camera); std::sort(cameras.begin(), cameras.end(), [](Scene::Camera *a, Scene::Camera *b) { return a->Depth < b->Depth; }); }
	// public void RemoveCamera(Scene::Camera * camera) { cameras.push_back(camera); std::sort(cameras.begin(), cameras.end(), [](Scene::Camera *a, Scene::Camera *b) { return a->Depth < b->Depth; }); }

	void SceneRenderer::RenderWithCamera(Scene::Camera &cam)
	{
		auto *rt = cam.GetTargetRenderTarget();
		if (!rt)
		{
			spdlog::warn("SceneRenderer: Camera '{}' 에 RenderTarget 없음 — 프레임 skip.",
			             cam.GetOwner() ? cam.GetOwner()->GetName() : "?");
			return;
		}

		auto &rc = DeviceContext::Get();
		rc.BeginFrame(*rt);

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

		mDispatcher.Dispatch(programs, dir, points, spots, viewPos);

		mProcessor.Clear();
		CollectFromActor(Scene::Director::Get().Root(), viewMat, cullingMask);
		mProcessor.SortMultiStage();
		mProcessor.Process(rc, viewMat, projMat);
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
