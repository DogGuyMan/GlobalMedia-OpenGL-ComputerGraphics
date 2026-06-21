/**
 * @file render_stage.impls.cpp
 * @brief @c IRenderStage 구현체 통합 정의 - @c SceneRenderer + @c ScreenQuadStage.
 *
 * @details
 *  ### SceneRenderer 구현 흐름 (RenderWithCamera 기준)
 *  1. Camera::GetTargetRenderTarget() 검증 - nullptr 이면 warn+skip.
 *  2. Camera::NoClear 분기:
 *     - false(기본): @c DeviceContext::BeginFrame(rt) - bind + clear(color|depth|stencil) + depth/blend 기본값 설정.
 *     - true: @c BindTarget + depth/blend 만 설정 (UI 레이어 등 clear 없이 위에 그리는 경우).
 *  3. SceneContext 에서 활성 Light 목록 수집 -> @c LightUboUploader::Update + @c BindTo.
 *  4. @c CollectFromActor DFS - Actor 트리를 순회하며 MeshRenderer / PassComponent 를 DrawCommand 로 변환.
 *  5. @c MeshPassProcessor::SortMultiStage + Process - 정렬 후 GL draw 발행.
 *  6. @c mLastSceneOutput 갱신 - PassComponent 체인의 마지막 출력 FB 추적.
 *
 *  ### ScreenQuadStage 구현 흐름 (Render 기준)
 *  - backbuffer 바인딩 -> depth/blend 설정 -> passthrough Program 활성화 -> screen quad VAO 바인딩
 *    -> **EBO 재핀**(Effekseer/Box2D VAO-EBO 오염 방어) -> FBO 목록 순서대로 합성(첫 replace, 2+ alpha blend).
 *
 *  ### 비-책임 (공통)
 *  - [X] GL uniform 직접 송신 - LightUboUploader(LightBlock UBO) / MeshPassProcessor(UBO 멤버+BindSamplers) 위임.
 *  - [X] GL 상태 머신 전환 - MeshPassProcessor / 본 stage -> @c DeviceContext::ApplyPipelineState 위임 (D-RS-1).
 *  - [X] FBO / Program / Mesh 생성 및 소유 - ResourceRegistry 책임.
 */
#include "render/render_stage/render_stage.impls.h"

#include "render/mesh_pass_processor.h"
#include "render/pass_component.h"
#include "render/device_context.h"
#include "render/mesh_renderer.h"
#include "buffer/render_target.h"
#include "buffer/framebuffer.h"
#include "material/material.h"
#include "scene/light.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "object/mesh.h"
#include "program/program.h"
#include "program/program_uniforms.h"

#include <GL/gl3w.h>
#include <glm/glm.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace SJH
{
	// ===================================================================================
	//  SceneRenderer - High-level Orchestrator
	// ===================================================================================
	void SceneRenderer::Render(RenderTarget & /*defaultTarget*/)
	{
		mLastSceneOutput = nullptr;

		// defaultTarget 파라미터는 IRenderStage 인터페이스 준수를 위해 유지 (ScreenQuadStage 등 다른
		// Stage 는 이 target 을 출력으로 사용). SceneRenderer 는 각 Camera 의 전용 RT 만 사용한다.

		// 1. SceneContext 에서 Camera 컬렉션 직접 조회 - DFS 폐기 (Cocos2D `Scene::_cameras` 정통).
		const auto &cameras = Scene::Director::Get().GetContext().GetCameras();
		if (cameras.empty())
		{
			spdlog::warn("SceneRenderer::Render - SceneContext 에 Camera 0 - 프레임 skip.");
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

	void SceneRenderer::SetBypassMaterial(Material *mat)
	{
		mProcessor.SetBypassMaterial(mat);
	}

	void SceneRenderer::RenderWithCamera(Scene::Camera &cam)
	{
		auto *rt = cam.GetTargetRenderTarget();
		if (!rt)
		{
			spdlog::warn("SceneRenderer: Camera '{}' 에 RenderTarget 없음 - 프레임 skip.",
			             cam.GetOwner() ? cam.GetOwner()->GetName() : "?");
			return;
		}

		auto &rc = DeviceContext::Get();

		/*
		* AI를 통한 디버깅
		*/
		if (cam.NoClear)
		{
			// clear 없이 위에 그림 (UI 레이어 등). GL state 는 직후 Process 가 진입 시 InvalidateStateCache +
			// 매 draw ApplyPipelineState 로 설정하므로 여기선 target bind 만 (D-RS-5 - 구 SetDepthTest/SetBlend 제거).
			rc.BindTarget(*rt);
		}
		else
		{
			rc.BeginFrame(*rt);
		}

		const auto viewMat     = cam.GetViewMatrix();
		const auto projMat     = cam.GetProjectionMatrix();
		const auto cullingMask = cam.CullingMask;
		DirLight                   *dir;
		std::vector<PointLight *>   points;
		std::vector<SpotLight *>    spots;

		glm::vec3 viewPos(0.0f, 0.0f, 0.0f);
		if (auto *camOwner = cam.GetOwner())
		{
			const auto camWorld = camOwner->GetWorldMatrix();
			viewPos = glm::vec3(camWorld[3][0], camWorld[3][1], camWorld[3][2]);
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

		// D-1 push -- 외부가 SetActivePrograms 로 주입한 스냅샷 사용 (rr 직접 pull 제거).
		// D-LUD O4 -- Update(광원 -> 공유 LightBlock UBO 패킹 + 캐시) 후 BindTo(UBO 결속 / loose 송신).
		mUploader.Update(dir, points, spots, viewPos);
		mUploader.BindTo(mActivePrograms);

		mProcessor.Clear();
		CollectFromActor(Scene::Director::Get().Root(), viewMat, cullingMask);
		mProcessor.SortMultiStage();
		mProcessor.Process(rc, viewMat, projMat);
		if (auto *lastFB = mProcessor.GetLastOutputFB())
			mLastSceneOutput = lastFB;
	}

	void SceneRenderer::CollectFromActor(const Scene::Actor &actor,
	                                     const glm::mat4  &viewMat,
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
					const glm::mat4 model  = actor.GetWorldMatrix();
					const float       depthZ = (viewMat * model)[3][2];
					DrawCommand       cmd;
					cmd.meshRenderer = mr;
					cmd.modelMatrix  = model;
					cmd.queueLayer   = mr->Material->GetQueueLayer() + mr->QueueOffset;
					cmd.depth        = depthZ;
					mProcessor.Submit(cmd);
				}
			}

			// ScreenQuad - PassComponent 수집 (disabled = bypass blit, passMaterial=nullptr)
			if (auto *pc = actor.GetComponent<Scene::PassComponent>())
			{
				if (pc->InputFB && pc->OutputFB)
				{
					DrawCommand cmd;
					cmd.kind         = DrawCommand::Kind::ScreenQuad;
					cmd.queueLayer   = pc->QueueOffset;
					cmd.inputFB      = pc->InputFB;
					cmd.outputFB     = pc->OutputFB;
					cmd.passMaterial = (pc->Enabled && pc->mMaterial) ? pc->mMaterial : nullptr;
					mProcessor.Submit(cmd);
				}
			}
		}

		for (const auto &child : actor.GetChildren())
			CollectFromActor(*child, viewMat, cullingMask);
	}

	// ===================================================================================
	//  ScreenQuadStage - N FBO -> backbuffer 합성 (final blit)
	// ===================================================================================
	ScreenQuadStage::ScreenQuadStage(Program &passthrough, Mesh &screenQuad)
	    : mProgram(passthrough), mMesh(screenQuad)
	{
	}

	void ScreenQuadStage::SetSources(std::vector<const Framebuffer *> sources)
	{
		mSources = std::move(sources);
	}

	void ScreenQuadStage::Render(RenderTarget &target)
	{
		if (mSources.empty())
		{
			spdlog::warn("[ScreenQuadStage] sources empty - skip");
			return;
		}

		auto &rc = DeviceContext::Get();

		// backbuffer 바인딩 + 클리어. GL state 는 state-as-data (D-RS-5).
		rc.BindTarget(target);
		rc.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// 직전 stage(SceneRenderer/ParticleStage)가 캐시 뒤에서 GL state 를 바꿨을 수 있으니 진입 시 무효화.
		rc.InvalidateStateCache();
		// 첫 소스: Screen state (depth off / cull off / blend off=replace - 전 프레임 백버퍼 잔상 차단).
		rc.ApplyPipelineState(Pass::DefaultPipelineStateOf(Pass::Kind::Screen));

		rc.UseProgram(mProgram);
		rc.BindVAO(mMesh.GetVAO());

		// Effekseer / Box2D 등 서드파티 GL 코드가 이 VAO 가 바인딩된 채로
		// glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, X) 를 호출하면 VAO 의 EBO 참조가
		// 덮어쓰여 glDrawElements ->GL_INVALID_OPERATION 이 발생한다.
		// 매 프레임 EBO 를 재핀해 원상복구.
		if (auto ebo = mMesh.GetIndexBuffer())
			ebo->Bind();

		for (std::size_t i = 0; i < mSources.size(); ++i)
		{
			const Framebuffer *fb = mSources[i];
			assert(fb != nullptr && "ScreenQuadStage: null Framebuffer source");

			const auto &tex = fb->GetColorAttachment();
			assert(tex && "ScreenQuadStage: Framebuffer has no color attachment");

			// sampler 컨벤션: `uScene` (SP4 migrate_demo / Unity _MainTex 정통).
			rc.BindTexture(0, tex->GetTextureID());
			Uniforms::SetInt(mProgram, "uScene", 0);

			// 2+ 소스 - 전 pass 위에 alpha blend 합성 (Screen state 의 BlendEnable 1필드만 켜서 적용 - D-RS-5).
			if (i == 1)
			{
				Pass::PipelineState blendState = Pass::DefaultPipelineStateOf(Pass::Kind::Screen);
				blendState.BlendEnable         = true;
				rc.ApplyPipelineState(blendState);
			}

			rc.DrawIndexed(mMesh.GetIndexCount());
		}

		// 후속 foreign 소비자(ImGui 등)는 자기 GL state 를 직접 설정한다. 캐시 desync 차단을 위해 stage 종료 시 무효화
		// (D-RS-2 - 다음 DeviceContext 소비자 진입 시 재적용. 다음 프레임 BeginFrame 도 무효화).
		rc.InvalidateStateCache();
	}

	// ===================================================================================
	//  CameraStage - 단일 Camera 를 IRenderStage 로 wrap (SceneRenderer::RenderWithCamera 위임)
	// ===================================================================================
	CameraStage::CameraStage(SceneRenderer* renderer, Scene::Camera* camera)
	    : mRenderer(renderer), mCamera(camera)
	{
	}

	void CameraStage::Render(RenderTarget& /*target*/)
	{
		if (!mRenderer || !mCamera)
		{
			spdlog::warn("CameraStage::Render - renderer/camera nullptr - skip.");
			return;
		}
		mRenderer->RenderWithCamera(*mCamera);
	}
} // namespace SJH
