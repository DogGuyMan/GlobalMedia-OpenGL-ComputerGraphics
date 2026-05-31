/**
 * @file main.cpp
 * @brief M1 컨벤션 정착 — Director + SceneRenderer + Material + MeshRenderer 패턴.
 *        직접 GL 호출 제거 (migrate_demo / tweeny_demo 정통).
 *        TestPattern frame 0 빌보드 1장 정적 표시.
 */

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <sb7.h>
#include <spdlog/spdlog.h>
#include <vmath.h>

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include "Audio/AudioSystem.h"
#include "Audio/FmodPlayable.h"
#include "Audio/FmodStudioPlayable.h"
#include "Entity/Player/PlayerActor.h"
#include "InputHandler/ActorFolower.h"
#include "InputHandler/PlayerController.h"
#include "Manager.h"
#include "Physics/filter.h"
#include "Tween/TweenPlayable.h"
#include "VFX/EffekseerPlayable.h"
#include "VFX/ParticleStage.h"
#include "playable/composite_playable.h"

#include "Stage/StageBuilder.h"
#include "UI/ExitButtonLayer.h"
#include "UI/ImGuiLayerStack.h"
#include "UI/PostFXDebugLayer.h"
#include "buffer/framebuffer.h"
#include "common/common.h"
#include "material/pass.h"
#include "object/mesh.h"
#include "program/program.h"
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
		struct ProgramConfig
		{
			const char *Name;
			const char *VertFile;
			const char *FragFile;
		};
		// 체인 인덱스 = 실행 순서 (doc/design/PostFX.md §3.1).

		const ProgramConfig PASSTHOURH_PROGRAM_CONFIG = {
		    "screen_passthrough",
		    "./resources/shaders/passthrough.vs",
		    "./resources/shaders/passthrough.fs"};

		// 체인 인덱스 = 실행 순서. 모든 PostFX 단계가 동일 postprocess.vs 공유.
		// 실제 디렉토리 = resources/shaders/postprocess/ (shaders 복수).
		const std::vector<ProgramConfig> POSTFX_PROGRAM_CONFIGS = {
		    {"blurring", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/blurring.fs"},
		    {"gamma", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/gamma.fs"},
		    {"invert", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/invert.fs"},
		    {"sharpening", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sharpening.fs"},
		    {"sobel", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sobel.fs"},
		};
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

			auto &manager = TopdownShooter::Manager::Get();
			manager.Init();
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();
			auto &phys = TopdownShooter::Manager::Get().Physics();
			auto &vfxs = TopdownShooter::Manager::Get().VFX();

			mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);
			WramupSceneRenderer(reg, Manager::Get().SceneRenderer(), PASSTHOURH_PROGRAM_CONFIG);
			mCamera = CreateAndRegisterWorldCamera();
			mScreenCamera = CreateAndRegisterScreenCamera();
			WarmupPassRenderer(reg, POSTFX_PROGRAM_CONFIGS);

			// ── stages 컬렉션 — World → Particle → Screen → ScreenQuad 순 ─────────
			// ScreenQuadStage 는 Step 3 에서 이미 mStages 에 push 된 상태.
			// 카메라 stages 를 *ScreenQuadStage 앞* 에 insert + ParticleStage 는
			// 아래 별도 블록에서 begin()+1 위치에 삽입 — 최종 순서:
			//   [0] worldCam      → sceneFB clear + 3D WorldMesh
			//   [1] ParticleStage → sceneFB (NoClear) + Effekseer 합성   (※ 아래 블록)
			//   [2] screenCam     → PassComponent 체인 (NoClear)
			//   [3] ScreenQuadStage → backbuffer 합성
			mStages.insert(
			    mStages.begin(),
			    std::make_unique<SJH::CameraStage>(&Manager::Get().SceneRenderer(), mScreenCamera));
			mStages.insert(
			    mStages.begin(),
			    std::make_unique<SJH::CameraStage>(&Manager::Get().SceneRenderer(), mCamera));

			// ── ParticleStage insert — Manager.Init() 직후 (VFXSystem 사용 가능 시점) ──
			//    위치: stages.begin() + 1 (worldCam 뒤, screenCam 앞)
			//    최종 stages: [worldCam, ParticleStage, screenCam, ScreenQuadStage]
			//    spec D-5 — Manager.Init() 흐름 보존을 위해 카메라 stages insert 와 분리.
			//               (spec 원문 식별자: "Director.Init() 흐름 보존" — Manager rename 이전 용어)
			mStages.insert(
			    mStages.begin() + 1,
			    std::make_unique<TopdownShooter::VFX::ParticleStage>(
			        &TopdownShooter::Manager::Get().VFX(), mCamera));

			WramupFMOD(dir, reg, Manager::Get().Audio());
			reg.CreateEffect(vfxs.GetManager(), "muzzle", u"resources/vfx/distortion.efk");

			dir.Root().AddChild(std::move(TopdownShooter::Stage::CreateStageActor({
			    &phys.World(),
			    &reg,
			})));

			WramupPlayer(reg, dir, phys);
			WarmupSkybox(reg, dir);
			WarmupImgui(reg);

			dir.Enter();
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
				if (mScreenCamera)
					mScreenCamera->Aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
				mSceneFB = SJH::Framebuffer::Create(fbW, fbH);
				if (mCamera)
					mCamera->SetTargetRenderTarget(mSceneFB.get());
				if (mScreenCamera)
					mScreenCamera->SetTargetRenderTarget(mSceneFB.get());
			}

			// ImGui NewFrame 우선 — io.WantCaptureMouse/Keyboard 가 입력 디스패치에 영향.
			ImGui_ImplGlfwGL3_NewFrame();

			mKeyboard.PollHeld(window);
			TopdownShooter::Manager::Get().Update(dt);
			SJH::Scene::Director::Get().Update(dt);
			TopdownShooter::Manager::Get().Physics().SyncToTransform(SJH::Scene::Director::Get().Root());

			// 스카이박스 시간(u_time) 동기화 — 위치는 셰이더가 view 이동 제거로 자동 처리.
			if (mSkyboxMat)
			{
				mSkyboxMat->Properties.Floats["u_time"] = static_cast<float>(currentTime);
			}

			// ── stages 컬렉션 순회 — World → Screen → ScreenQuad ─────────────────
			// ScreenQuadStage 의 sources 는 *stages 순회 직전* 갱신 (지난 프레임 PassComponent 출력).
			{
				auto *out = Manager::Get().SceneRenderer().GetLastSceneOutput();
				mScreenQuadStagePtr->SetSources({out ? out : mSceneFB.get()}); // ! ??? 필요한 것 맞나?
			}
			for (auto &s : mStages)
				s->Render(*mDefaultTarget);

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
			mScreenCamera = nullptr;
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
			if (mScreenCamera)
				mScreenCamera->Aspect = static_cast<float>(w) / static_cast<float>(h);
		}

	  private:
		// ── 멤버 ────────────────────────────────────────────────────────────────────
		SJH::RenderTargetUPtr mDefaultTarget;
		// SP-RenderStage 완성 — Application 이 stages 컬렉션을 명시 순서로 순회.
		std::vector<std::unique_ptr<SJH::IRenderStage>> mStages;
		SJH::ScreenQuadStage *mScreenQuadStagePtr;

		SJH::FramebufferUPtr mSceneFB;
		std::array<SJH::FramebufferUPtr, 5> mPostFXFBs;
		std::vector<SJH::Scene::PassComponent *> mPassComponents; // PostFXDebug ImGui 패널이 토글 대상 참조 — 비소유 raw
		float mGamma = 1.0f;

		// ImGui
		ImGuiContext *mImGuiCtx = nullptr;
		UI::ImGuiLayerStack mImGuiStack;
		bool mShowEditor = true;

		// 씬 오브젝트
		SJH::Scene::Actor *mSkyboxActor = nullptr;
		SJH::Material *mSkyboxMat = nullptr;
		SJH::Scene::Actor *mSpriteActor = nullptr;
		SJH::Scene::Camera *mCamera = nullptr;
		SJH::Scene::Camera *mScreenCamera = nullptr;
		SJH::SpriteSequence::SpriteSequencePlayable *mSpriteSeq = nullptr;
		SJH::SpriteSequence::SpriteFrameClip mWholeAtlasClip{};
		SJH::Sprite::SpriteRenderer *mSprite = nullptr;
		SJH::KeyboardInput<Controller::PlayerController::Action> mKeyboard;
		SJH::MouseInput mMouse;

		SJH::Scene::Camera *CreateAndRegisterWorldCamera()
		{
			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);

			auto &dir = SJH::Scene::Director::Get();

			// ── World Camera (Perspective) — 3D 월드 ────────────────────────────
			auto worldCamActor = SJH::Scene::CreateCameraActor("WorldCamera", 45.0f, aspect, 0.1f, 100.0f);
			auto &worldCamTransform = worldCamActor->GetTransform();
			worldCamTransform.SetTransformWithVectors(
			                     vmath::vec3(0.0f, 4.0f, 8.0f),
			                     vmath::vec3(-30.0f, 0.0f, 0.0))
			    .PrintTransform();

			auto *camera = worldCamActor->GetComponent<SJH::Scene::Camera>();
			camera
			    ->SetCullingMask(SJH::Scene::Layer::Default |
			                     SJH::Scene::Layer::Player |
			                     SJH::Scene::Layer::Enemy |
			                     SJH::Scene::Layer::DebugDraw)
			    .SetTargetRenderTarget(mSceneFB.get());

			worldCamActor
			    ->AddComponent<Controller::ActorFolower>()
			    ->SetMouseInput(&mMouse)
			    .SetCamera(camera)
			    .SetFollowOffset(worldCamTransform.Translate)
			    .SetUp();

			dir.Root().AddChild(std::move(worldCamActor));
			return camera;
		}

		SJH::Scene::Camera *CreateAndRegisterScreenCamera()
		{
			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);

			auto &dir = SJH::Scene::Director::Get();

			// ── Screen Camera (Orthographic) — HUD + PassComponent 체인 ────────
			auto screenCamActor = SJH::Scene::CreateCameraActor("ScreenCamera", 45.0f, aspect, -1.0f, 1.0f);

			auto &screenCamTransform = screenCamActor->GetTransform();
			auto *camera = screenCamActor->GetComponent<SJH::Scene::Camera>();
			camera->IsOrthographic = true;
			camera->OrthoSize = 1.0f;
			camera->NoClear = true; // WorldCamera 출력 보존 — clear 없이 합성
			camera
			    ->SetCullingMask(SJH::Scene::Layer::UI |
			                     SJH::Scene::Layer::Screen)
			    .SetTargetRenderTarget(mSceneFB.get());

			dir.Root().AddChild(std::move(screenCamActor));
			return camera;
		}

		void WramupFMOD(SJH::Scene::Director &dir, SJH::ResourceRegistry &reg, TopdownShooter::Audio::AudioSystem &audio)
		{

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

		void WramupSceneRenderer(SJH::ResourceRegistry &reg, SJH::SceneRenderer &scene_renderer, ProgramConfig program_config)
		{
			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);

			// ScreenQuadStage — passthrough 셰이더 + ScreenQuad 메쉬 등록
			auto *passthroughProg = reg.CreateProgram(
			    program_config.Name,
			    program_config.VertFile,
			    program_config.FragFile);

			mSceneFB = SJH::Framebuffer::Create(fbW, fbH);
			auto *quadMesh = reg.RegisterMesh("mesh_screen_quad", SJH::Mesh::CreateScreenQuad());

			// ── ScreenQuadStage 생성 — 소유권 stages 컬렉션으로 이전 ──────────────
			// 이 시점에 카메라 멤버 (mCamera/mScreenCam) 는 아직 valid 아님 (line 143 / 203 에서 대입).
			// 그래서 ScreenQuadStage 만 먼저 mStages 에 push, 카메라 stages 는 Step 4 에서
			// *앞에* insert 하여 최종 [worldCam, screenCam, ScreenQuadStage] 순서 정착.
			auto screenQuadStage = std::make_unique<SJH::ScreenQuadStage>(
			    *passthroughProg,
			    *quadMesh); // ! 필요한거 맞나.

			screenQuadStage->SetSources({mSceneFB.get()}); // 초기 sources — sceneFB fallback
			mScreenQuadStagePtr = screenQuadStage.get();
			mStages.push_back(std::move(screenQuadStage));

			// AI 에이전틱
			auto *bypassMat = reg.CreateSharedMaterial("mat_bypass_passthrough");
			bypassMat->SetProgram(passthroughProg);

			scene_renderer.SetScreenQuadMesh(quadMesh);
			scene_renderer.SetBypassMaterial(bypassMat);
		}

		void WarmupPassRenderer(SJH::ResourceRegistry &reg /*, ScreenRenderer*/, const std::vector<ProgramConfig> &program_configs)
		{
			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);

			mPassComponents.clear();
			SJH::Framebuffer *prevFB = mSceneFB.get();
			for (std::size_t i = 0; i < program_configs.size(); ++i)
			{
				const auto &def = program_configs[i];
				const auto progKey = std::string("postfx_") + def.Name;
				const auto matKey = std::string("mat_pass_") + def.Name;

				auto *prog = reg.CreateProgram(
				    program_configs[i].Name,
				    program_configs[i].VertFile,
				    program_configs[i].FragFile);
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
				mPassComponents.push_back(pc); // PostFXDebug ImGui 패널이 토글 대상 참조

				prevFB = mPostFXFBs[i].get();

				mScreenCamera->GetOwner()->AddChild(std::move(passActor));
			}
		}

		void WramupPlayer(SJH::ResourceRegistry &reg, SJH::Scene::Director &dir, Physics::PhysicsSystem &phys)
		{
			TopdownShooter::Entity::Player::PlayerActorConfig pac;
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

			mSprite = spriteActor->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);

			mWholeAtlasClip = SJH::SpriteSequence::SpriteFrameClip{0, atlas->FrameCount(), 4.0f};
			mSpriteSeq = spriteActor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
			    mSprite, &mWholeAtlasClip);
			mSpriteSeq->SetIsLoop(true);
			mSpriteSeq->Play();

			mSpriteActor = dir.Root().AddChild(std::move(spriteActor));
			mCamera
				->GetOwner()
				->GetComponent<Controller::ActorFolower>()
				->SetFollowTarget(mSpriteActor);
		}

		void WarmupSkybox(SJH::ResourceRegistry &reg, SJH::Scene::Director &dir)
		{
			auto *skyboxProg = reg.CreateProgram(
			    "matrix_skybox",
			    "resources/shaders/matrix_skybox.vs",
			    "resources/shaders/matrix_skybox.fs");

			auto *charsTex = reg.CreateTexture("chars", SJH::Image::Load("chars", "resources/texture/characters.png").get());
			// 매트릭스 글자 스크롤 필수 — 셰이더의 char_uv.x 가 +noise+time 으로 1 을 넘어 순환한다.
			// CreateTexture 기본 wrap 은 GL_CLAMP_TO_EDGE(texture.cpp) 라 끝 열에 고착돼 세로 줄로
			// 보이므로, 글자 열이 순환하도록 REPEAT 로 덮어쓴다 (uniform_atlas Bind→SetWrap 선례).
			charsTex->Bind();
			charsTex->SetWrap(GL_REPEAT, GL_REPEAT);
			// 촘촘한 격자에서 글자칸이 작아지면 기본 MIPMAP_LINEAR(texture.cpp)가 LOD 를 올려
			// 글자를 회색으로 뭉갠다 → mipmap 없는 GL_LINEAR 로 또렷하게 유지.
			charsTex->SetFilter(GL_LINEAR, GL_LINEAR);
			auto *noiseTex = reg.CreateTexture("noise_tex", SJH::Image::Load("noise_tex", "resources/texture/matrix_noise.png").get());
			// 셰이더가 noise 좌표를 NOISE_SCALE(=8)배로 키워 샘플 → 1 을 넘는 좌표가 클램프되지
			// 않고 타일링되도록 REPEAT 필수 (CLAMP 면 가장자리 한 색으로 뭉개짐).
			noiseTex->Bind();
			noiseTex->SetWrap(GL_REPEAT, GL_REPEAT);

			mSkyboxMat = reg.CreateSharedMaterial("mat_matrix_skybox");
			mSkyboxMat->SetProgram(skyboxProg);
			// Skybox Pass — DepthFunc LEQUAL(.xyww 트릭) + CullMode FRONT(박스 안쪽 면) +
			// DepthWrite off + queue 2500(Opaque 다음). pass.h 의 Kind::Skybox 가 전부 자동 도출.
			mSkyboxMat->SetPass(SJH::Pass::Kind::Skybox);
			// 텍스처 유닛 분리 필수 — TextureBinding.Unit 이 둘 다 기본값 0 이면
			// 두 sampler 가 같은 유닛을 가리켜 한 텍스처만 읽힌다 (PropertyBlockSetter 가
			// binding.Unit 그대로 BindTexture + sampler int 송신).
			mSkyboxMat->Properties.Textures["chars"] = {charsTex, 0};
			mSkyboxMat->Properties.Textures["noise_tex"] = {noiseTex, 1};
			mSkyboxMat->Properties.Floats["u_time"] = 0.0f;

			auto *skyboxMesh = reg.RegisterMesh("mesh_skybox", SJH::Mesh::CreateBox());
			auto skyboxActor = std::make_unique<SJH::Scene::Actor>("MatrixSkybox");
			// Transform 은 셰이더가 무시한다 — matrix_skybox.vs 는 uModel 을 쓰지 않고
			// view 의 이동 성분을 제거(mat3)해 박스를 항상 카메라 중심에 둔다.
			skyboxActor->AddComponent<SJH::Scene::MeshRenderer>(skyboxMesh, mSkyboxMat);
			mSkyboxActor = dir.Root().AddChild(std::move(skyboxActor));
		}

		void WarmupImgui(SJH::ResourceRegistry &reg)
		{
			// === ImGui v1.53 init (install_callbacks=false — sb7 가 GLFW 콜백 소유) ===
			mImGuiCtx = ImGui::CreateContext();
			ImGui::StyleColorsDark();
			ImGui_ImplGlfwGL3_Init(window, /*install_callbacks=*/false);
			glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
			glfwSetCharCallback(window, ImGui_ImplGlfwGL3_CharCallback);

			// ExitButton 텍스처 — Game UI Layer
			const auto *exitTex = reg.CreateTexture(
			    "exit_texture",
			    SJH::Image::Load("exit_texture", "resources/texture/exit_texture.png").get());

			// ImGui 레이어 등록 — Game(항상) / Editor(F1 토글)
			mImGuiStack.Push(std::make_unique<UI::ExitButtonLayer>(window, exitTex));

			std::vector<UI::PassDebugEntry> debugEntries;
			for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size(); ++i)
			{
				if (i < mPassComponents.size())
					debugEntries.push_back({POSTFX_PROGRAM_CONFIGS[i].Name, mPassComponents[i]});
			}
			mImGuiStack.Push(std::make_unique<UI::PostFXDebugLayer>(
			    std::move(debugEntries), mGamma));
		}
	};

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
