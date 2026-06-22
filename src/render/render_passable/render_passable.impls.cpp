/**
 * @file render_passable.impls.cpp
 * @brief @c IPassable 구현체 통합 정의 - @c SceneRenderer + @c ScreenQuadStage.
 *
 * @details
 *  ### SceneRenderer 구현 흐름 (RenderWithCamera 기준)
 *  1. Camera::GetTargetRenderTarget() 검증 - nullptr 이면 warn+skip.
 *  2. Camera::NoClear 분기:
 *     - false(기본): @c DeviceContext::BeginFrame(rt) - bind + clear(color|depth|stencil) + depth/blend 기본값 설정.
 *     - true: @c BindTarget + depth/blend 만 설정 (UI 레이어 등 clear 없이 위에 그리는 경우).
 *  3. SceneContext 에서 활성 Light 목록 수집 -> @c LightUboUploader::Update + @c BindTo.
 *  4. @c CollectFromActor DFS - Actor 트리를 순회하며 MeshRenderer(IRenderable*) / PassComponent 를 Submit 으로 변환.
 *  5. @c RenderableProcessor::Sort + Process(rc, cam) - 정렬 후 IRenderable 잎 위임 + GL draw 발행.
 *  6. @c mLastSceneOutput 갱신 - PassComponent 체인의 마지막 출력 FB 추적.
 *
 *  ### ScreenQuadStage 구현 흐름 (Render 기준)
 *  - backbuffer 바인딩 -> depth/blend 설정 -> passthrough Program 활성화 -> screen quad VAO 바인딩
 *    -> **EBO 재핀**(Effekseer/Box2D VAO-EBO 오염 방어) -> FBO 목록 순서대로 합성(첫 replace, 2+ alpha blend).
 *
 *  ### 비-책임 (공통)
 *  - [X] GL uniform 직접 송신 - LightUboUploader(LightBlock UBO) / RenderableProcessor->MeshRenderer::Render 위임.
 *  - [X] GL 상태 머신 전환 - RenderableProcessor / 본 stage -> @c DeviceContext::ApplyRenderStateBlock 위임 (D-RS-1).
 *  - [X] FBO / Program / Mesh 생성 및 소유 - ResourceRegistry 책임.
 */
#include "render/render_passable/render_passable.impls.h"

#include "render/mesh_pass_processor.h"
#include "render/pass_component.h"
#include "render/draw_ops.h"
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
	void SceneRenderer::Draw(DeviceContext & /*rec*/, const Texture * /*before*/)
	{
		mLastSceneOutput = nullptr;

		// rec/before 파라미터는 IPassable 인터페이스 준수를 위해 유지 (ScreenQuadStage 등 다른
		// Stage 는 이 파라미터를 출력으로 사용). SceneRenderer 는 각 Camera 의 전용 RT 만 사용한다.

		// 1. SceneContext 에서 Camera 컬렉션 직접 조회 - DFS 폐기 (Cocos2D `Scene::_cameras` 정통).
		const auto &cameras = Scene::Director::Get().GetContext().GetCameras();
		if (cameras.empty())
		{
			spdlog::warn("SceneRenderer::Draw - SceneContext 에 Camera 0 - 프레임 skip.");
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
			// 매 draw ApplyRenderStateBlock 로 설정하므로 여기선 target bind 만 (D-RS-5 - 구 SetDepthTest/SetBlend 제거).
			rc.BindTarget(*rt);
		}
		else
		{
			rc.BeginFrame(*rt);
		}

		const auto viewMat     = cam.GetViewMatrix();
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
		mProcessor.Sort();
		mProcessor.Process(rc, cam);
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
			// MeshRenderer - IRenderable* 로 Submit. 가드/depth 계산 구 동작 보존.
			if (auto *mr = actor.GetComponent<Scene::MeshRenderer>())
			{
				if (mr->IsEnabled() && mr->Visible && mr->Mesh && mr->Material)
				{
					const glm::mat4 model  = actor.GetWorldMatrix();
					const float     depthZ = (viewMat * model)[3][2];
					// mr 은 IRenderable 로 암묵 upcasting. queueLayer 는 Submit 이 r->QueueLayer() 로 캡처.
					// mr->QueueLayer() = Pass::QueueOf(Material->GetPass(), QueueOffset) = 구 GetQueueLayer()+QueueOffset 동일.
					mProcessor.Submit(mr, depthZ);
				}
			}

			// PassComponent - ScreenQuad 수집 (disabled = bypass blit, passMat=nullptr, 구 동작 보존).
			if (auto *pc = actor.GetComponent<Scene::PassComponent>())
			{
				if (pc->InputFB && pc->OutputFB)
				{
					mProcessor.SubmitScreenQuad(
						pc->InputFB,
						pc->OutputFB,
						(pc->Enabled && pc->mMaterial) ? pc->mMaterial : nullptr);
				}
			}
		}

		for (const auto &child : actor.GetChildren())
			CollectFromActor(*child, viewMat, cullingMask);
	}

	// ===================================================================================
	//  ScreenQuadStage - N FBO -> backbuffer 합성 (final blit) -- Draw(rec, before)
	// ===================================================================================
	ScreenQuadStage::ScreenQuadStage(Program &passthrough, Mesh &screenQuad)
	    : mProgram(passthrough), mMesh(screenQuad)
	{
	}

	void ScreenQuadStage::SetSources(std::vector<const Framebuffer *> sources)
	{
		mSources = std::move(sources);
	}

	void ScreenQuadStage::Draw(DeviceContext & /*rec*/, const Texture * /*before*/)
	{
		if (mSources.empty())
		{
			spdlog::warn("[ScreenQuadStage] sources empty - skip");
			return;
		}

		auto &rc = DeviceContext::Get();

		// backbuffer 바인딩 + 클리어. GL state 는 state-as-data (D-RS-5).
		if (!mBackbuffer)
		{
			spdlog::warn("[ScreenQuadStage] backbuffer 미주입 - skip");
			return;
		}
		rc.BindTarget(*mBackbuffer);
		rc.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// 직전 stage(SceneRenderer/ParticlePass)가 캐시 뒤에서 GL state 를 바꿨을 수 있으니 진입 시 무효화.
		rc.InvalidateStateCache();
		// 첫 소스: Screen state (depth off / cull off / blend off=replace - 전 프레임 백버퍼 잔상 차단).
		rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Screen));

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
				Pass::RenderStateBlock blendState = Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Screen);
				blendState.BlendEnable         = true;
				rc.ApplyRenderStateBlock(blendState);
			}

			rc.DrawIndexed(mMesh.GetIndexCount());
		}

		// 후속 foreign 소비자(ImGui 등)는 자기 GL state 를 직접 설정한다. 캐시 desync 차단을 위해 stage 종료 시 무효화
		// (D-RS-2 - 다음 DeviceContext 소비자 진입 시 재적용. 다음 프레임 BeginFrame 도 무효화).
		rc.InvalidateStateCache();
	}

	// ===================================================================================
	//  CameraStage - 단일 Camera 를 IPassable 로 wrap (SceneRenderer::RenderWithCamera 위임)
	// ===================================================================================
	CameraStage::CameraStage(SceneRenderer* renderer, Scene::Camera* camera)
	    : mRenderer(renderer), mCamera(camera)
	{
	}

	void CameraStage::Draw(DeviceContext & /*rec*/, const Texture * /*before*/)
	{
		if (!mRenderer || !mCamera)
		{
			spdlog::warn("CameraStage::Draw - renderer/camera nullptr - skip.");
			return;
		}
		mRenderer->RenderWithCamera(*mCamera);
	}

	// ===================================================================================
	//  WorldPass - 단일 World Camera 자기완결 렌더 (구 SceneRenderer::RenderWithCamera + CameraStage 통합, Task 3.1)
	// ===================================================================================
	void WorldPass::Draw(DeviceContext &rec, const Texture * /*before*/)
	{
		if (!mCamera)
		{
			spdlog::warn("WorldPass::Draw - camera nullptr - skip.");
			return;
		}

		auto *rt = mCamera->GetTargetRenderTarget();
		if (!rt)
		{
			spdlog::warn("WorldPass: Camera '{}' 에 RenderTarget 없음 - 프레임 skip.",
			             mCamera->GetOwner() ? mCamera->GetOwner()->GetName() : "?");
			return;
		}

		// mClearsTarget=false 면(선행 SkyboxPass 가 이미 clear+skybox 로 sceneFB 를 채움) clear 스킵 - 그 위에 그린다.
		// NoClear - clear 없이 위에 그림(UI 레이어 등). GL state 는 Process 진입 시 InvalidateStateCache +
		// 매 draw ApplyRenderStateBlock 로 설정하므로 여기선 target bind 만(D-RS-5).
		if (mCamera->NoClear || !mClearsTarget)
			rec.BindTarget(*rt);
		else
			rec.BeginFrame(*rt);

		const auto viewMat     = mCamera->GetViewMatrix();
		const auto cullingMask = mCamera->CullingMask;

		DirLight                  *dir = nullptr;
		std::vector<PointLight *>  points;
		std::vector<SpotLight *>   spots;

		glm::vec3 viewPos(0.0f, 0.0f, 0.0f);
		if (auto *camOwner = mCamera->GetOwner())
		{
			const auto camWorld = camOwner->GetWorldMatrix();
			viewPos = glm::vec3(camWorld[3][0], camWorld[3][1], camWorld[3][2]);
		}
		
		// [FUTURE-OPT-RENDERLIST] 광원 enabled 필터를 Pass 마다 재수행 중. 라이트는 이미 SceneContext 가
		// 보유(Light::OnEnter -> GetContext().AddLight, light.cpp:73)이므로, enabled 사전필터 캐시를 두면
		// Pass 당 재필터를 없앨 수 있다. 단 enabled 토글이 잦으면 iteration 필터가 더 단순 -
		// 우선순위는 아래 MeshRenderer DFS 쪽이 높다(같은 [FUTURE-OPT-RENDERLIST] 태그).
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

		// D-LUD O4 - Update(광원 -> 공유 LightBlock UBO 패킹) 후 BindTo(UBO 결속).
		mUploader.Update(dir, points, spots, viewPos);
		mUploader.BindTo(mActivePrograms);

		mProc.Clear();
		// [FUTURE-OPT-RENDERLIST] 매 Pass Actor 트리 전체를 DFS 로 재순회해 MeshRenderer 를 수집한다(scalability 한계).
		// AAA 정통(Unreal FScene::Primitives / Unity renderer list / Godot RenderingServer instance)은 씬그래프와
		// *렌더 리스트* 를 분리해 flat list 를 캐시하고 add/remove 시에만 dirty 갱신한다. 본 엔진은 이미
		// Camera(camera.cpp:71)/Light(light.cpp:73) 를 SceneContext auto-register 로 캐시(memory scenecontext-auto-register)
		// 하는데 MeshRenderer 만 빠져 있다 - MeshRenderer OnEnter/OnExit 에서 SceneContext.AddRenderable/Remove 로
		// 동일 패턴 적용 가능. CullingMask 필터 + depth/Sort 는 view 의존이라 Pass 별 유지(캐시는 membership 만).
		// 1단계=프레임당 1회 수집해 Pass 간 공유(pass-to-pass 중복 제거, dangling 위험 0) ->
		// 2단계=dirty-tracked 캐시(O(변경)). 제거경로(RemoveChild/defer despawn) 누락 시 dangling 주의.
		CollectFromActor(Scene::Director::Get().Root(), viewMat, cullingMask);
		mProc.Sort();
		mProc.Process(rec, *mCamera);
	}

	void WorldPass::CollectFromActor(const Scene::Actor &actor,
	                                 const glm::mat4   &viewMat,
	                                 uint64_t           cullingMask)
	{
		if (!actor.IsActive())
			return;

		const bool visibleToCamera = (cullingMask & actor.GetLayer()) != 0ull;
		if (visibleToCamera)
		{
			// MeshRenderer 만 Submit. depth 계산/가드는 구 SceneRenderer::CollectFromActor 와 동일.
			if (auto *mr = actor.GetComponent<Scene::MeshRenderer>())
			{
				// skybox(Pass==Skybox)는 SkyboxPass 가 그리므로 WorldPass 는 제외(중복 draw 방지).
				if (mr->IsEnabled() && mr->Visible && mr->Mesh && mr->Material
				    && mr->Material->GetPass() != Pass::RenderQueue::Skybox)
				{
					const glm::mat4 model  = actor.GetWorldMatrix();
					const float     depthZ = (viewMat * model)[3][2];
					mProc.Submit(mr, depthZ);
				}
			}
			// PassComponent(ScreenQuad) 미수집 - Task 3.5 PostFxPass 담당(현재는 screenCam 패스의 SceneRenderer 가 수집).
		}

		for (const auto &child : actor.GetChildren())
			CollectFromActor(*child, viewMat, cullingMask);
	}

	const Texture *WorldPass::GetPassResult() const
	{
		if (!mCamera)
			return nullptr;
		// 카메라 RT 가 Framebuffer 면 그 color attachment 가 이 Pass 의 출력(D6). backbuffer 면 nullptr.
		auto *fb = dynamic_cast<Framebuffer *>(mCamera->GetTargetRenderTarget());
		return fb ? fb->GetColorAttachment().get() : nullptr;
	}

	// ===================================================================================
	//  SkyboxPass - background-first skybox 합성 (구 WorldPass queue-2500 skybox 분리, Task 3.2)
	// ===================================================================================
	void SkyboxPass::Draw(DeviceContext &rec, const Texture * /*before*/)
	{
		if (!mSkybox || !mCamera)
		{
			spdlog::warn("SkyboxPass::Draw - skybox/camera nullptr - skip.");
			return;
		}
		auto *rt = mCamera->GetTargetRenderTarget();
		if (!rt)
		{
			spdlog::warn("SkyboxPass::Draw - camera RenderTarget 없음 - skip.");
			return;
		}

		// background-first: SkyboxPass 가 sceneFB clear 책임 인수(color+depth+stencil + InvalidateStateCache + Opaque baseline).
		rec.BeginFrame(*rt);
		// skybox ROP(DepthWrite off, LEQUAL, CullFront) 적용 후 자가발행 Render - 구 WorldPass queue-2500 leaf 와 동일 호출.
		rec.ApplyRenderStateBlock(mSkybox->GetRenderStateBlock());
		mSkybox->Render(rec, *mCamera);
	}

	const Texture *SkyboxPass::GetPassResult() const
	{
		if (!mCamera)
			return nullptr;
		// 공유 sceneFB 의 color attachment(D6). background-first 라 WorldPass 와 같은 RT 를 가리킨다.
		auto *fb = dynamic_cast<Framebuffer *>(mCamera->GetTargetRenderTarget());
		return fb ? fb->GetColorAttachment().get() : nullptr;
	}

	// ===================================================================================
	//  PostFxPass - per-effect PostFX blit (구 RenderableProcessor screen 루프 1단계 분리, Task 3.5)
	// ===================================================================================
	void PostFxPass::Draw(DeviceContext &rec, const Texture * /*before*/)
	{
		// before(PassIterator 동적 체이닝)는 4.1 에서 소비 - 현재는 사전배선 InputFB 사용(픽셀 보존).
		if (!mPc || !mPc->InputFB || !mPc->OutputFB || !mQuad)
			return;
		// Enabled=false 면 bypass passthrough (구 SubmitScreenQuad passMat=nullptr 동작 보존).
		Material *mat = (mPc->Enabled && mPc->mMaterial) ? mPc->mMaterial : mBypass;
		if (!mat)
			return;
		const Program *prog = mat->GetProgram();
		if (!prog)
			return;

		rec.BeginFrame(*mPc->OutputFB);
		rec.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Screen));

		// uScene = InputFB color attachment (unit 0). 구 screen 루프와 동일.
		mat->Properties.Textures["uScene"] = {mPc->InputFB->GetColorAttachment().get(), 0};

		rec.UseProgram(*prog);
		if (prog->HasUniformBlocks())
		{
			UploadMaterialUboMembers(*prog, mat->Properties);
			prog->BindUniformBlocks();
		}
		BindSamplers(rec, mat->Properties, *prog);

		rec.BindVAO(mQuad->GetVAO());
		// VAO EBO 오염 가드(Effekseer/Box2D) - 구 동작 보존.
		if (auto ebo = mQuad->GetIndexBuffer())
			ebo->Bind();
		rec.DrawIndexed(mQuad->GetIndexCount());
	}

	const Texture *PostFxPass::GetPassResult() const
	{
		return (mPc && mPc->OutputFB) ? mPc->OutputFB->GetColorAttachment().get() : nullptr;
	}
} // namespace SJH
