/**
 * @file main.cpp
 * @brief M1 컨벤션 정착 — Manager + SceneRenderer + Material + MeshRenderer 패턴.
 *        직접 GL 호출 제거 (migrate_demo / tweeny_demo 정통).
 *        TestPattern frame 0 빌보드 1장 정적 표시.
 */

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <sb7.h>
#include <spdlog/spdlog.h>
#include <vmath.h>

// ImGui v1.53 — client-side (Core Module 아님). memory: imgui_v1_53_glfw_compat
#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include "Audio/FmodPlayable.h"
#include "Audio/FmodStudioPlayable.h"
#include "Manager.h"
#include "Entity/Player/PlayerActor.h"
#include "InputHandler/PlayerController.h"
#include "InputHandler/TargetFollowableCameraController.h"
#include "Physics/filter.h"
#include "Tween/TweenPlayable.h"
#include "VFX/EffekseerPlayable.h"
#include "playable/composite_playable.h"

#include "Stage/StageBuilder.h"
#include "UI/ExitButtonLayer.h"
#include "UI/ImGuiLayerStack.h"
#include "UI/PostFXDebugLayer.h"
#include "buffer/framebuffer.h"
#include "common/common.h"
#include "object/mesh.h"
#include "render/camera_stage.h"
#include "render/pass_component.h"
#include "render/render_target.h"
#include "render/scene_renderer.h"
#include "render/screen_quad_stage.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_frame_clip.h"
#include "sprite/sprite_sequence_playable.h"
#include <cmath>
#include <tweeny/tweeny.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace TopdownShooter
{
	namespace
	{
		struct PostFXDef
		{
			const char *Name;
			const char *FragFile;
		};
		// 체인 인덱스 = 실행 순서 (doc/design/PostFX.md §3.1).
		constexpr std::array<PostFXDef, 5> kPostFXDefs = {{
		    {"blurring", "resources/shader/postprocess/blurring.fs"},
		    {"gamma", "resources/shader/postprocess/gamma.fs"},
		    {"invert", "resources/shader/postprocess/invert.fs"},
		    {"sharpening", "resources/shader/postprocess/sharpening.fs"},
		    {"sobel", "resources/shader/postprocess/sobel.fs"},
		}};
		constexpr const char *kPostFXVertFile = "resources/shader/postprocess/postprocess.vs";
	} // namespace

	class game_application : public sb7::application
	{
	  public:
		void init() override
		{
			sb7::application::init();
			info.majorVersion = 4;
			info.minorVersion = 1;
			info.flags.debug = 1;

			SJH::CrossPlatformDir();
		}

		void startup() override
		{
			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
			glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

			mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);
			mSceneFB = SJH::Framebuffer::Create(fbW, fbH);

			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();

			// Atlas — registry 가 LoadFromPNG + SetGrid 일괄.
			auto *atlas = reg.CreateUniformAtlas(
			    "test_pattern",
			    "resources/texture/TestPattern.png",
			    4, 4);
			if (!atlas)
			{
				spdlog::error("[M1] atlas load failed");
				return;
			}
			const auto *exitTex = reg.CreateTexture("exit_texture",
				 SJH::Image::Load("exit_texture", "resources/texture/exit_texture.png").get());

			// ScreenQuadStage — passthrough 셰이더 + ScreenQuad 메쉬 등록
			auto *passthroughProg = reg.CreateProgram(
			    "screen_passthrough",
			    "resources/shaders/passthrough.vs",
			    "resources/shaders/passthrough.fs");
			auto *quadMesh = reg.RegisterMesh("mesh_screen_quad", SJH::Mesh::CreateScreenQuad());
			// ── ScreenQuadStage 생성 — 소유권 stages 컬렉션으로 이전 ──────────────
			// 이 시점에 카메라 멤버 (mCamera/mScreenCam) 는 아직 valid 아님 (line 143 / 203 에서 대입).
			// 그래서 ScreenQuadStage 만 먼저 mStages 에 push, 카메라 stages 는 Step 4 에서
			// *앞에* insert 하여 최종 [worldCam, screenCam, ScreenQuadStage] 순서 정착.
			auto screenQuadOwned = std::make_unique<SJH::ScreenQuadStage>(*passthroughProg, *quadMesh);
			screenQuadOwned->SetSources({mSceneFB.get()}); // 초기 sources — sceneFB fallback
			mScreenQuadStagePtr = screenQuadOwned.get();   // raw 캐시 — render() 의 SetSources 갱신용 // ! ??? 필요한 것 맞나?
			mStages.push_back(std::move(screenQuadOwned));

			// ── World Camera (Perspective) — 3D 월드 ->sceneFB ─────────────────────────
			auto worldCamActor = SJH::Scene::CreateCameraActor("WorldCamera", 45.0f, aspect, 0.1f, 100.0f);
			worldCamActor->GetTransform().Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
			worldCamActor->GetTransform().EulerRot = vmath::vec3(-45.0f, 0.0f, 0.0f);
			auto *worldCam = worldCamActor->GetComponent<SJH::Scene::Camera>();
			auto *camCtrl = worldCamActor->AddComponent<Controller::TargetFollowableCameraController>();
			camCtrl->SetMouseInput(&mMouse).SetCamera(worldCam).SetUp();
			worldCam->SetCullingMask(SJH::Scene::Layer::Default |
			                         SJH::Scene::Layer::Player |
			                         SJH::Scene::Layer::Enemy |
			                         SJH::Scene::Layer::DebugDraw);
			worldCam->SetTargetRenderTarget(mSceneFB.get());
			mCameraActor = dir.Root().AddChild(std::move(worldCamActor));
			mCamera = worldCam;

			// ── Screen Camera (Orthographic) — HUD + PassComponent 체인 ────────────────
			// 초기 activeFB = sceneFB (World Camera 출력 공유) — HUD 가 씬에 직접 합성.
			auto screenCamActor = SJH::Scene::CreateCameraActor("ScreenCamera", 45.0f, aspect, -1.0f, 1.0f);
			auto *screenCam = screenCamActor->GetComponent<SJH::Scene::Camera>();
			screenCam->IsOrthographic = true;
			screenCam->OrthoSize = 1.0f;
			screenCam->NoClear = true; // WorldCamera 출력 보존 — clear 없이 합성
			screenCam->SetCullingMask(SJH::Scene::Layer::UI | SJH::Scene::Layer::Screen);
			screenCam->SetTargetRenderTarget(mSceneFB.get());

			// ── PassComponent 체인 (blurring→gamma→invert→sharpening→sobel) ─────────────
			mRenderSys.SetScreenQuadMesh(quadMesh);
			{
				auto *bypassMat = reg.CreateSharedMaterial("mat_bypass_passthrough");
				bypassMat->SetProgram(passthroughProg);
				mRenderSys.SetBypassMaterial(bypassMat);
			}
			mPassComponents.clear();
			SJH::Framebuffer *prevFB = mSceneFB.get();

			for (std::size_t i = 0; i < kPostFXDefs.size(); ++i)
			{
				const auto &def = kPostFXDefs[i];
				const auto progKey = std::string("postfx_") + def.Name;
				const auto matKey = std::string("mat_pass_") + def.Name;

				auto *prog = reg.CreateProgram(progKey, kPostFXVertFile, def.FragFile);
				if (!prog)
				{
					spdlog::error("[PassComponent] 셰이더 로드 실패: {}", def.FragFile);
					continue;
				}

				auto *mat = reg.CreateSharedMaterial(matKey);
				mat->SetProgram(prog);
				if (std::string(def.Name) == "gamma")
					mat->Properties.Floats["gamma"] = mGamma;

				mPostFXFBs[i] = SJH::Framebuffer::Create(fbW, fbH);
				if (!mPostFXFBs[i])
				{
					spdlog::error("[PassComponent] FB 생성 실패: {}", def.Name);
					continue;
				}

				auto passActor = std::make_unique<SJH::Scene::Actor>(std::string("PassActor_") + def.Name);
				passActor->SetLayer(SJH::Scene::Layer::Screen);

				auto *pc = passActor->AddComponent<SJH::Scene::PassComponent>(
				    prevFB, mPostFXFBs[i].get(), mat);
				mPassComponents.push_back(pc);

				prevFB = mPostFXFBs[i].get();

				screenCamActor->AddChild(std::move(passActor));
			}

			mScreenCamActor = dir.Root().AddChild(std::move(screenCamActor));
			mScreenCam = screenCam;

			// ── stages 컬렉션 — World → Screen → ScreenQuad 순 (SP-RenderStage) ─
			// ScreenQuadStage 는 Step 3 에서 이미 mStages 에 push 된 상태.
			// 카메라 stages 를 *ScreenQuadStage 앞* 에 insert — 최종 순서:
			//   [0] worldCam → sceneFB 에 WorldMesh
			//   [1] screenCam → PassComponent 체인 (NoClear)
			//   [2] ScreenQuadStage → backbuffer 합성
			mStages.insert(
			    mStages.begin(),
			    std::make_unique<SJH::CameraStage>(&mRenderSys, mScreenCam));
			mStages.insert(
			    mStages.begin(),
			    std::make_unique<SJH::CameraStage>(&mRenderSys, mCamera));

			// === M5 — Manager 가 Audio + VFX + Physics 일괄 초기화 ===
			TopdownShooter::Manager::Get().Init();
			auto &phys = TopdownShooter::Manager::Get().Physics();

			// === M5 T3 — BGM (FMOD Studio) ===
			{
				auto &audio = TopdownShooter::Manager::Get().Audio();
				audio.LoadBank("resources/banks/Master.strings.bank");
				audio.LoadBank("resources/banks/Master.bank");

				auto *bgmEvent = audio.LoadEvent("event:/BGM");
				if (bgmEvent)
				{
					auto *bgmActor = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("BgmActor"));
					auto *bgm = bgmActor->AddComponent<TopdownShooter::Audio::FmodStudioPlayable>(bgmEvent);
					bgm->SetIsLoop(true);
					bgm->Play();
				}

				reg.CreateSound(audio.GetSystem(), "shot", "resources/audio/Laser.wav");
			}

			// === M5 T2 — Muzzle VFX ===
			{
				auto mgr = TopdownShooter::Manager::Get().VFX().GetManager();
				reg.CreateEffect(mgr, "muzzle", u"resources/vfx/distortion.efk");
			}

			auto stage = TopdownShooter::Stage::CreateStageActor({
			    &phys.World(),
			    &reg,
			});
			dir.Root().AddChild(std::move(stage));

			pac.name = "PlayerSprite";
			pac.life.hp = 100;
			pac.movement.speed = 3.0f;
			pac.controller.keyboard = &mKeyboard;
			pac.physics.world = &phys.World();
			pac.physics.size = vmath::vec2(1.0f, 1.0f);
			pac.physics.startPosition = vmath::vec2(0.0f, 0.0f);
			pac.physics.density = 1.0f;
			pac.physics.linearDamping = 5.0f;
			pac.physics.categoryBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Player);
			pac.physics.maskBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PlayerMask);

			auto spriteActor = TopdownShooter::Entity::Player::CreatePlayerActor(pac);
			spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
			spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);

			mSprite = spriteActor->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);

			mWholeAtlasClip = SJH::SpriteSequence::SpriteFrameClip{0, atlas->FrameCount(), 4.0f};
			mSpriteSeq = spriteActor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
			    mSprite, &mWholeAtlasClip);
			mSpriteSeq->SetIsLoop(true);
			mSpriteSeq->Play();

			mSpriteActor = dir.Root().AddChild(std::move(spriteActor));

			camCtrl->SetFollowTarget(mSpriteActor)
			    .SetFollowOffset(vmath::vec3(0.0f, 5.0f, 5.0f));

			dir.Enter();

			// === ImGui v1.53 init (install_callbacks=false — sb7 가 GLFW 콜백 소유) ===
			mImGuiCtx = ImGui::CreateContext();
			ImGui::StyleColorsDark();
			ImGui_ImplGlfwGL3_Init(window, /*install_callbacks=*/false);
			glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
			glfwSetCharCallback(window, ImGui_ImplGlfwGL3_CharCallback);

			// ImGui 레이어 등록 — Game(항상) / Editor(F1 토글)
			mImGuiStack.Push(std::make_unique<UI::ExitButtonLayer>(window, exitTex));
			std::vector<UI::PassDebugEntry> debugEntries;
			for (std::size_t i = 0; i < kPostFXDefs.size(); ++i)
			{
				if (i < mPassComponents.size())
					debugEntries.push_back({kPostFXDefs[i].Name, mPassComponents[i]});
			}
			mImGuiStack.Push(std::make_unique<UI::PostFXDebugLayer>(
			    std::move(debugEntries), mGamma));
		}

		void render(double currentTime) override
		{
			const float dt = static_cast<float>(SJH::DeltaTime(currentTime));

			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			if (!mDefaultTarget || mDefaultTarget->GetWidth() != fbW || mDefaultTarget->GetHeight() != fbH)
			{
				mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);
				if (mCamera)
					mCamera->Aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
				if (mScreenCam)
					mScreenCam->Aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
				mSceneFB = SJH::Framebuffer::Create(fbW, fbH);
				if (mCamera)
					mCamera->SetTargetRenderTarget(mSceneFB.get());
				if (mScreenCam)
					mScreenCam->SetTargetRenderTarget(mSceneFB.get());
			}

			// ImGui NewFrame 우선 — io.WantCaptureMouse/Keyboard 가 입력 디스패치에 영향.
			ImGui_ImplGlfwGL3_NewFrame();

			mKeyboard.PollHeld(window);
			TopdownShooter::Manager::Get().Update(dt);
			SJH::Scene::Director::Get().Update(dt);
			TopdownShooter::Manager::Get().Physics().SyncToTransform(SJH::Scene::Director::Get().Root());

			// ── stages 컬렉션 순회 — World → Screen → ScreenQuad ─────────────────
			// ScreenQuadStage 의 sources 는 *stages 순회 직전* 갱신 (지난 프레임 PassComponent 출력).
			{
				auto *out = mRenderSys.GetLastSceneOutput();
				mScreenQuadStagePtr->SetSources({out ? out : mSceneFB.get()}); // ! ??? 필요한 것 맞나?
			}
			for (auto &s : mStages)
				s->Render(*mDefaultTarget);

			// === M5 — Effekseer 렌더 (backbuffer 합성 후, swap 전) ===
			if (mCamera)
			{
				vmath::mat4 view = mCamera->GetViewMatrix();
				vmath::mat4 proj = mCamera->GetProjectionMatrix();
				TopdownShooter::Manager::Get().VFX().Draw(&view[0][0], &proj[0][0]);
			}

			// ImGui 창 빌드 + 렌더 (항상 최상위).
			mImGuiStack.RenderAll(mShowEditor);
			ImGui::Render();
		}

		void shutdown() override
		{
			ImGui_ImplGlfwGL3_Shutdown();
			ImGui::DestroyContext(mImGuiCtx);
			mImGuiCtx = nullptr;

			SJH::Scene::Director::Get().Exit();
			mCamera = nullptr;
			mCameraActor = nullptr;
			mScreenCam = nullptr;
			mScreenCamActor = nullptr;
			mPassComponents.clear();
			mSpriteActor = nullptr;
			mStages.clear();
			mDefaultTarget.reset();
			mSprite = nullptr;
			mSpriteSeq = nullptr;
			TopdownShooter::Manager::Get().Shutdown();
		}

		void onKey(int key, int action) override
		{
			ImGui_ImplGlfwGL3_KeyCallback(window, key, /*scancode*/ 0, action, /*mods*/ 0);
			if (ImGui::GetIO().WantCaptureKeyboard)
				return;

			// 리펙토링 대상
			if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
				mShowEditor = !mShowEditor;

			mKeyboard.Dispatch(key, action);

			// 리펙토링 대상
			// === M5 CO2 — G 키: Parallel( TweenShake ∥ FmodStudio.Damaged ) ===
			if (key == GLFW_KEY_G && action == GLFW_PRESS)
			{
				auto &audio = TopdownShooter::Manager::Get().Audio();
				auto *damagedEvt = audio.LoadEvent("event:/Damaged");

				auto *dActor = SJH::Scene::Director::Get().Root().AddChild(
				    std::make_unique<SJH::Scene::Actor>("DamageComposite"));
				auto *par = dActor->AddComponent<SJH::Playable::ParallelPlayable>();

				auto tween = tweeny::from(0.0f).to(1.0f).during(100).via(tweeny::easing::sinusoidalInOut);
				par->Join(std::make_unique<TopdownShooter::Tween::TweenPlayable<float>>(
				    std::move(tween),
				    [](float v) {
					    float offset = std::sin(v * 8.0f * 3.14159f) * 5.0f;
					    spdlog::info("[shake] v={:.3f} offset={:.3f}", v, offset);
				    }));
				if (damagedEvt)
					par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(damagedEvt));

				par->Play();
			}
		}

		void onMouseButton(int button, int action) override
		{
			ImGui_ImplGlfwGL3_MouseButtonCallback(window, button, action, /*mods*/ 0);
			if (ImGui::GetIO().WantCaptureMouse)
				return;

			double x = 0.0, y = 0.0;
			glfwGetCursorPos(window, &x, &y);
			mMouse.HandleButton(button, action, x, y);

			// 리펙토링 대상
			// === M5 CO1 — 마우스 좌클릭: Sequence( Effekseer.distortion  Parallel( Fmod.Laser ∥ FmodStudio.Slash ) ) ===
			if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
			{
				auto &reg = SJH::ResourceRegistry::Get();
				auto &audio = TopdownShooter::Manager::Get().Audio();
				auto &vfx = TopdownShooter::Manager::Get().VFX();

				auto *shot = reg.FindSound("shot");
				auto *muzzle = reg.FindEffect("muzzle");
				auto *slashEvt = audio.LoadEvent("event:/Slash");

				if (shot && muzzle && slashEvt)
				{
					auto *cActor = SJH::Scene::Director::Get().Root().AddChild(
					    std::make_unique<SJH::Scene::Actor>("ShotComposite"));
					auto *seq = cActor->AddComponent<SJH::Playable::SequencePlayable>();

					seq->Append(std::make_unique<TopdownShooter::VFX::EffekseerPlayable>(
					    vfx.GetManager(), muzzle, vmath::vec3(0.0f),
					    TopdownShooter::VFX::TrackPolicy::Static));

					auto par = std::make_unique<SJH::Playable::ParallelPlayable>();
					par->Join(std::make_unique<TopdownShooter::Audio::FmodPlayable>(audio.GetSystem(), shot));
					par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(slashEvt));
					seq->Append(std::move(par));

					seq->Play();
				}
			}
		}

		void onMouseMove(int x, int y) override
		{
			// ImGui v1.53 은 NewFrame 시 직접 glfwGetCursorPos 폴링 — forward 불필요.
			if (ImGui::GetIO().WantCaptureMouse)
				return;
			mMouse.HandleMove(static_cast<double>(x), static_cast<double>(y));
		}

		void onResize(int /*logicalW*/, int /*logicalH*/) override
		{
			int w = 0, h = 0;
			glfwGetFramebufferSize(window, &w, &h);
			if (w <= 0 || h <= 0)
				return;
			sb7::application::onResize(w, h);
			glViewport(0, 0, w, h);
			mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(w, h);
			if (mCamera)
				mCamera->Aspect = static_cast<float>(w) / static_cast<float>(h);
			if (mScreenCam)
				mScreenCam->Aspect = static_cast<float>(w) / static_cast<float>(h);
		}

	  private:
		// ── 멤버 ────────────────────────────────────────────────────────────────────
		SJH::SceneRenderer mRenderSys;
		SJH::RenderTargetUPtr mDefaultTarget;
		SJH::FramebufferUPtr mSceneFB;

		// SP-RenderStage 완성 — Application 이 stages 컬렉션을 명시 순서로 순회.
		std::vector<std::unique_ptr<SJH::IRenderStage>> mStages;
		SJH::ScreenQuadStage *mScreenQuadStagePtr = nullptr; // 비소유 raw — owner = mStages // ! ??? 필요한 것 맞나?

		// PassComponent 체인 FBs + 포인터 목록
		std::array<SJH::FramebufferUPtr, 5> mPostFXFBs;
		std::vector<SJH::Scene::PassComponent *> mPassComponents;
		float mGamma = 1.0f;

		// ImGui
		ImGuiContext *mImGuiCtx = nullptr;
		UI::ImGuiLayerStack mImGuiStack;
		bool mShowEditor = true;

		// 씬 오브젝트
		SJH::Scene::Actor *mCameraActor = nullptr;
		SJH::Scene::Actor *mScreenCamActor = nullptr;
		SJH::Scene::Actor *mSpriteActor = nullptr;
		SJH::Scene::Camera *mCamera = nullptr;
		SJH::Scene::Camera *mScreenCam = nullptr;
		SJH::SpriteSequence::SpriteSequencePlayable *mSpriteSeq = nullptr;
		SJH::SpriteSequence::SpriteFrameClip mWholeAtlasClip{};
		SJH::Sprite::SpriteRenderer *mSprite = nullptr;
		SJH::KeyboardInput<Controller::PlayerController::Action> mKeyboard;
		SJH::MouseInput mMouse;
		TopdownShooter::Entity::Player::PlayerActorConfig pac;
	};

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
