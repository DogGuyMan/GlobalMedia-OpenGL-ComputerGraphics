#include "render/scene_renderer.h"
#include "render/mesh_pass_processor.h"
#include "render/pass_component.h"
#include <vmath.h>
#include <cstdint>
#include <vector>
#include "material/material.h"
#include "object/light.h"
#include "object/mesh.h"
#include "render/device_context.h"
#include "render/mesh_renderer.h"
#include "render/render_target.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include <spdlog/spdlog.h>

namespace SJH
{
	void SceneRenderer::Render(RenderTarget & /*defaultTarget*/)
	{
		mLastSceneOutput = nullptr;

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
		for (auto *cam : cameras)
			if (cam->IsEnabled())
				RenderWithCamera(*cam);
	}

	void SceneRenderer::SetScreenQuadMesh(Mesh *mesh)
	{
		mProcessor.SetScreenQuadMesh(mesh);
	}

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

		const auto viewMat     = cam.GetViewMatrix();
		const auto projMat     = cam.GetProjectionMatrix();
		const auto cullingMask = cam.CullingMask;
		DirLight                   *dir;
		std::vector<PointLight *>   points;
		std::vector<SpotLight *>    spots;

		vmath::vec3 viewPos(0.0f, 0.0f, 0.0f);
		if (auto *camOwner = cam.GetOwner())
		{
			const auto camWorld = camOwner->GetWorldMatrix();
			viewPos = vmath::vec3(camWorld[3][0], camWorld[3][1], camWorld[3][2]);
		}

		auto &ctx = Scene::Director::Get().GetContext();
		dir       = (ctx.GetDirLight() && ctx.GetDirLight()->IsEnabled()) ? ctx.GetDirLight() : nullptr;
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
		if (auto *lastFB = mProcessor.GetLastOutputFB())
			mLastSceneOutput = lastFB;
	}

	void SceneRenderer::CollectFromActor(const Scene::Actor &actor,
	                                     const vmath::mat4  &viewMat,
	                                     uint64_t            cullingMask)
	{
		if (!actor.IsActive())
			return;

		const bool visibleToCamera = (cullingMask & actor.GetLayer()) != 0ull;

		if (visibleToCamera)
		{
			if (auto *mr = actor.GetComponent<Scene::MeshRenderer>())
			{
				if (mr->IsEnabled() && mr->Visible && mr->Mesh && mr->Material)
				{
					const vmath::mat4 model  = actor.GetWorldMatrix();
					const float       depthZ = (viewMat * model)[3][2];
					DrawCommand       cmd;
					cmd.meshRenderer = mr;
					cmd.modelMatrix  = model;
					cmd.queueLayer   = mr->Material->GetQueueLayer() + mr->QueueOffset;
					cmd.depth        = depthZ;
					mProcessor.Submit(cmd);
				}
			}

			// ScreenQuad — PassComponent 수집
			if (auto *pc = actor.GetComponent<Scene::PassComponent>())
			{
				if (pc->Enabled && pc->InputFB && pc->OutputFB && pc->mMaterial)
				{
					DrawCommand cmd;
					cmd.kind         = DrawCommand::Kind::ScreenQuad;
					cmd.queueLayer   = pc->QueueOffset;
					cmd.inputFB      = pc->InputFB;
					cmd.outputFB     = pc->OutputFB;
					cmd.passMaterial = pc->mMaterial;
					mProcessor.Submit(cmd);
				}
			}
		}

		for (const auto &child : actor.GetChildren())
			CollectFromActor(*child, viewMat, cullingMask);
	}
} // namespace SJH
