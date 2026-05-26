#include "render/scene_renderer.h"
#include "buffer/framebuffer.h"
#include "material/material.h"
#include "object/light.h"
#include "object/mesh.h"
#include "render/device_context.h"
#include "render/mesh_renderer.h"
#include "render/property_block_setter.h"
#include "render/render_target.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include <spdlog/spdlog.h>
#include <utility>
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
			mLastActiveFXOutput = nullptr;
			return;
		}

		// 2. addCamera 호출 순서 그대로 렌더 (Cocos2D `addChild` 정통). std::sort 폐기.
		for (auto *cam : cameras)
			if (cam->IsEnabled())
				RenderWithCamera(*cam);

		// 3. PostFX 체인 — 씬 카메라 렌더 직후 (doc/design/PostFX.md §3.2).
		//    첫 번째 활성 Camera 의 RT(Framebuffer) 를 SceneInput 으로 삼는다.
		mLastActiveFXOutput = nullptr;
		if (!mPostFXChain.empty() && mQuadMesh)
		{
			for (auto *cam : cameras)
			{
				if (!cam->IsEnabled()) continue;
				if (auto *fb = dynamic_cast<Framebuffer *>(cam->GetTargetRenderTarget()))
				{
					RunPostFXChain(*fb);
					break;
				}
			}
		}
	}

	void SceneRenderer::SetPostFXChain(std::vector<PostFXPass> chain, Mesh *quadMesh)
	{
		mPostFXChain = std::move(chain);
		mQuadMesh    = quadMesh;
		mLastActiveFXOutput = nullptr;
	}

	void SceneRenderer::ClearPostFXChain()
	{
		mPostFXChain.clear();
		mQuadMesh           = nullptr;
		mLastActiveFXOutput = nullptr;
	}

	void SceneRenderer::RunPostFXChain(const Framebuffer &sceneInput)
	{
		mLastActiveFXOutput     = nullptr;
		const Framebuffer *inputFB = &sceneInput;
		auto              &rc      = DeviceContext::Get();

		for (auto &pass : mPostFXChain)
		{
			if (!pass.Enabled || !pass.OutputFB || !pass.Material)
				continue;
			auto *prog = pass.Material->GetProgram();
			if (!prog)
				continue;

			rc.BeginFrame(*pass.OutputFB);
			rc.SetDepthTest(false);
			rc.SetBlend(false);

			// 입력 텍스처 바인딩 — MaterialPropertyBlock.Textures["uScene"] 에 저장.
			// PropertyBlockSetter::Set 이 매 프레임 sampler unit 0 에 바인드.
			pass.Material->Properties.Textures["uScene"] = {
			    inputFB->GetColorAttachment().get(), 0};

			rc.UseProgram(*prog);
			PropertyBlockSetter::Set(rc, pass.Material->Properties, *prog);

			rc.BindVAO(mQuadMesh->GetVAO());
			// VAO 오염 가드 — Effekseer/Box2D 가 EBO 를 덮어쓸 수 있음 (memory: vao_ebo_thirdparty_corruption).
			if (auto ebo = mQuadMesh->GetIndexBuffer())
				ebo->Bind();
			rc.DrawIndexed(mQuadMesh->GetIndexCount());

			rc.SetDepthTest(true);

			inputFB             = pass.OutputFB;
			mLastActiveFXOutput = pass.OutputFB;
		}
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
		}

		for (const auto &child : actor.GetChildren())
			CollectFromActor(*child, viewMat, cullingMask);
	}
} // namespace SJH
