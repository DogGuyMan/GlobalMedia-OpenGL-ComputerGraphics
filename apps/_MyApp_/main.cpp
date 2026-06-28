/**
 * @file main.cpp
 * @brief M1 컨벤션 정착 — Director + SceneRenderer + Material + MeshRenderer 패턴.
 *        직접 GL 호출 제거 (migrate_demo / tweeny_demo 정통).
 *        TestPattern frame 0 빌보드 1장 정적 표시.
 */

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <sb7.h>
#include <spdlog/spdlog.h>

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include "Audio/AudioSystem.h"
#include "Audio/Constants.h"
#include "Bootstrap/AudioWarmup.h"
#include "Bootstrap/EngineBootstrap.h"
#include "Bootstrap/InitScheduler.h"
#include "Bootstrap/PlayerBuilder.h"
#include "Bootstrap/WorldSceneBuilder.h"
#include "Constants.h"
#include "GameSystems.h"
#include "InputHandler/PlayerController.h"
#include "VFX/Constants.h"
#include "VFX/ParticleStage.h"

#include "Playable/Constants.h"
#include "Spawns/OneShotSweeper.h"
#include "Spawns/VfxInstance.h"
#include "Spawns/WorldTextInstance.h"
#include "UI/VfxSpawnLayer.h"

#include "Audio/FmodStudioPlayable.h"
#include "Bootstrap/actor_factory.h"
#include "Entity/Components/LifeComponents.h"
#include "Stage/Components/GameContextComponent.h"
#include "Stage/Constants.h"
#include "Stage/Stage.h"
#include "Stage/StageBuilder.h"
#include "Stage/State/StageState.Impl.h"
#include "Stage/State/StageStateMachine.h"
#include "Stage/WaveController.h"
#include "UI/ImGuiLayerStack.h"
#include "UI/ImGuiPass.h"
#include "UI/PassDebugLayer.h"
#include "UI/StateOverlayLayer.h"
#include "UI/UiBootstrap.h"
#include "common/common.h"
#include "common/window_helper.h"
#include "material/material.h"
#include "object/mesh.h"
#include "render/device_context.h"
#include "render/pass_iterator.h"
#include "render/render_passable/render_passable.impls.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/scene.h"

#include "Capture/golden_capture.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace TopdownShooter
{
	class game_application : public sb7::application, public Bootstrap::IClientBootstrap
	{
	  public:
		void init() override // ! 모듈화 대상 (GL 컨텍스트/창 설정 = sb7 boot lifecycle)
		{
			sb7::application::init();
			info.majorVersion = 4;
			info.minorVersion = 1;
			info.flags.debug = 1;

			// 기본 창을 16:9 창모드로 (sb7 기본 800×600=4:3 덮어쓰기).
			// macOS Retina 는 실제 framebuffer 가 2배(2560×1440)지만 비율은 16:9 유지.
			// fullscreen 은 기본 0(windowed). GLFW 3.0 이라 aspect 하드락 API(3.2+) 는 없음 — 초기 크기만 지정.
			info.windowWidth = 1280;
			info.windowHeight = 720;

			SJH::CrossPlatformDir();
		}

		void startup() override // ! 모듈화 대상 (boot 진입점 = AppRunner boot; capture 트리거는 test 모듈로)
		{
			// GU0 capture 모드 활성화 -- SJH_GOLDEN_CAPTURE 환경 변수 존재 시.
			// GL 컨텍스트 생성 직후, 부트 시퀀스 전에 설정.
			mCaptureMode = (std::getenv("SJH_GOLDEN_CAPTURE") != nullptr);
			if (mCaptureMode)
			{
				spdlog::info("[GoldenCapture] capture 모드 활성화 -- 고정-dt 1/60, 180 프레임 후 PNG 저장 후 종료.");
				std::srand(42); // 결정적 시뮬을 위한 고정 시드
			}

			// A1 -- GLFW window 정보 캐시 (hook 들이 공유).
			mFbInfo = SJH::GetFramebufferInfo(window);
			glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

			// 부트 시퀀스 -- 엔진 스켈레톤이 3 hook 을 순차 호출 (제어역전).
			// 각 hook 은 phase-local InitScheduler 로 자기 phase task 를 topo-sort 직렬 실행.
			Bootstrap::EngineBootstrap engine;
			engine.Boot(*this);
		}

		// -- phase 1: 자원/시스템 (topo L0~L2) --
		void OnResourcesReady() override // ! 모듈화 대상 (phase1 자원/시스템 boot)
		{
			Bootstrap::InitScheduler sched;

			// T1 core -- 렌더 타깃 + 시스템 초기화 + 오디오 워밍업.
			sched.Task(Bootstrap::EInitTask::Core).Gl().Does([&] {
				mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(mFbInfo.Width, mFbInfo.Height);
				mSceneFB = SJH::Framebuffer::CreateWithDepthTexture(mFbInfo.Width, mFbInfo.Height);

				TopdownShooter::GameSystems::Get().Init();
				Bootstrap::WarmupAudio(GameSystems::Get().Audio());
				return mSceneFB != nullptr;
			});

			sched.RunAll();
		}

		// -- phase 2: 씬/파이프라인 (topo L3~L5) --
		void OnSceneSetup() override // ! 모듈화 대상 (phase2 씬/파이프라인 조립 = 핵심 추출 대상)
		{
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();
			auto &manager = TopdownShooter::GameSystems::Get();
			auto &phys = manager.Physics();
			auto &vfxs = manager.VFX();

			Bootstrap::InitScheduler sched;

			// T2 screenPipeline -- DefaultPipeline + ScreenCamera + PostFX 체인 + fog/vignette + 레지스트리.
			sched.Task(Bootstrap::EInitTask::ScreenPipeline).Gl().Does([&] {
				// 풀스크린 blit 용 screen quad mesh (PostFxPass 가 비소유 참조 - 소유=rr).
				mScreenQuadMeshPtr = reg.RegisterMesh("mesh_screen_quad", SJH::Mesh::CreateScreenQuad());
				if (!mScreenQuadMeshPtr)
				{
					spdlog::error("[ScreenPipeline] screen quad Mesh 등록 실패");
					return false;
				}

				// present(passthrough) Material - 효과 체인 끝에서 마지막 활성 결과를 backbuffer 로 blit (output=nullptr present).
				auto *ptProg = reg.CreateProgram(
				    Playable::PASSTHOURH_PROGRAM_CONFIG.Name,
				    Playable::PASSTHOURH_PROGRAM_CONFIG.VertFile,
				    Playable::PASSTHOURH_PROGRAM_CONFIG.FragFile);
				if (!ptProg)
				{
					spdlog::error("[ScreenPipeline] passthrough 셰이더 로드 실패");
					return false;
				}
				mPresentMatPtr = reg.CreateSharedMaterial("mat_pass_present");
				mPresentMatPtr->SetProgram(ptProg);

				// PostFX 효과별 Program + Material("mat_pass_<name>") + 중간 FBO (config 순서, 1:1).
				//   rr 키 = mat_pass_<name> (HpGrayscalePostFX / PostFXTweenPlayable / PassDebugLayer 조회 규약 일치).
				mPostFXFBs.clear();
				mPostFXFBs.reserve(Playable::POSTFX_PROGRAM_CONFIGS.size());
				for (const auto &c : Playable::POSTFX_PROGRAM_CONFIGS)
				{
					auto *prog = reg.CreateProgram(c.Name, c.VertFile, c.FragFile);
					if (!prog)
					{
						spdlog::error("[ScreenPipeline] PostFX 셰이더 로드 실패: {}", c.FragFile);
						mPostFXFBs.push_back(nullptr); // configs 인덱스 정합(placeholder).
						continue;
					}
					auto *mat = reg.CreateSharedMaterial(std::string("mat_pass_") + c.Name);
					mat->SetProgram(prog);
					for (const auto &[name, value] : c.InitFloats) // D-6 data-driven float 초기값.
						mat->Properties.Floats[name] = value;
					mPostFXFBs.push_back(SJH::Framebuffer::Create(mFbInfo.Width, mFbInfo.Height)); // 중간 FBO(color sampler).
				}

				// 특수 초기값(vec3/int - InitFloats 밖) - 효과 머티리얼을 키로 조회해 set.
				if (auto *fogMat = reg.FindSharedMaterial(std::string("mat_pass_") + Playable::PASS_FOG))
				{
					fogMat->Properties.Vec3s["uFogColor"] = Playable::FOG_COLOR;
					fogMat->Properties.Ints["uFogMode"] = Playable::FOG_MODE;
				}
				// grayscale 공유 Material - HpGrayscalePostFX SSOT + PassDebugLayer read-only 표시용 캡처.
				mGrayscaleMatPtr = reg.FindSharedMaterial(std::string("mat_pass_") + Playable::PASS_GRAYSCALE_VIGNETTING);
				if (mGrayscaleMatPtr)
					mGrayscaleMatPtr->Properties.Vec3s["uVignetteColor"] = Playable::VIGNETTE_COLOR; // vec3 초기값(InitFloats 밖).

				RebindFogUniforms(); // fog uDepth(unit1) = sceneFB depth 텍스처 바인딩.

				// 화면 카메라 (resize aspect 추적용 - present 자체는 PostFxPass 가 backbuffer 직접 bind).
				auto screenCamActor = SJH::Scene::CreateScreenCameraActor(ACTOR_SCREEN_CAMERA, mFbInfo.Aspect, mSceneFB.get());
				mScreenCamera = screenCamActor->GetComponent<SJH::Scene::Camera>();
				dir.Root().AddChild(std::move(screenCamActor));

				return true;
			});

			// T3 world -- WorldScene(camera/light/skybox) + 스테이지 액터 + FxRoot + spawn 컨텍스트
			//             + muzzle 이펙트 + 플레이어 + 웨이브 컨트롤러.
			sched.Task(Bootstrap::EInitTask::World).Needs({Bootstrap::EInitTask::VfxUi}).Gl().Does([&] { // vfxUi 가 TEST_EFFECTS(orbital_background) 를 선행 로드 -> 스테이지 FindEffect 의존
				auto worldScene = Bootstrap::BuildWorldScene({mFbInfo.Aspect, &mMouse, mSceneFB.get()});
				mCamera = worldScene.WorldCamera;
				mSkyboxMat = worldScene.SkyboxMat;
				mSkyboxRenderer = worldScene.SkyboxRenderer;

				dir.Root().AddChild(std::move(TopdownShooter::Stage::CreateStageActor({&phys.World(), &reg})));

				mFxRoot = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>(ACTOR_FX_ROOT));
				VFX::SetSpawnContext(mFxRoot, &vfxs);
				WorldText::SetSpawnContext(mFxRoot, manager.WorldText().GetFont());

				reg.CreateEffect(vfxs.GetManager(), VFX::MUZZLE_EFFECT.key, VFX::MUZZLE_EFFECT.path);

				auto player = Bootstrap::BuildPlayer({&mKeyboard, &mMouse, &phys.World(), mCamera});
				mSpriteActor = player.SpriteActor;

				auto *waveSpawner = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>(Stage::ACTOR_WAVE_SPAWNER));
				waveSpawner->AddComponent<Stage::WaveController>(&phys.World(), waveSpawner, mSpriteActor, Stage::ARENA_HALF_EXTENT);
				return mCamera != nullptr && mSpriteActor != nullptr;
			});

			// T4 stages -- PassIterator 가 모든 Pass 를 소유. 코스 순서대로 Add(std::move).
			//   [Skybox -> World -> Particle -> Grayscale(present) -> ImGui]. 비소유 관찰 포인터는 move 전에 캡처.
			sched.Task(Bootstrap::EInitTask::Stages).Needs({Bootstrap::EInitTask::ScreenPipeline, Bootstrap::EInitTask::World}).Cpu().Does([&] {
				// 1) Skybox - background-first (sceneFB clear+skybox 책임).
				mPassIterator.Add(std::make_unique<SJH::SkyboxPass>(mSkyboxRenderer, mCamera));

				// 2) World - skybox 위에 그림. SetClearsTarget(false) 로 clear 스킵. SetActivePrograms 주입 위해 관찰 포인터 캡처.
				auto worldPass = std::make_unique<SJH::WorldPass>(mCamera);
				mWorldPassPtr = &worldPass->SetClearsTarget(false); // move 전 캡처 (소유=PassIterator, 수명=app 동안 안정).
				mPassIterator.Add(std::move(worldPass));

				// 3) Particle - sceneFB 에 Effekseer 합성 (worldCam RT).
				mPassIterator.Add(std::make_unique<TopdownShooter::VFX::ParticlePass>(&GameSystems::Get().VFX(), mCamera));

				// 4) PostFX 효과 체인 - 각 효과는 중간 FBO 에 그림(output=fbo). before=직전 활성 결과(PassIterator lastResult).
				//    config 순서 = 체인 순서. invert/blurring/sobel 은 기본 비활성(Enabled=false, 동적 skip).
				for (std::size_t i = 0; i < Playable::POSTFX_PROGRAM_CONFIGS.size(); ++i)
				{
					const std::string &name = Playable::POSTFX_PROGRAM_CONFIGS[i].Name;
					auto *mat = reg.FindSharedMaterial(std::string("mat_pass_") + name);
					auto *fbo = (i < mPostFXFBs.size() && mPostFXFBs[i]) ? mPostFXFBs[i].get() : nullptr;
					if (!mat || !fbo)
						continue; // 셰이더 로드 실패 placeholder - skip(체인은 lastResult 가 재연결).
					auto fx = std::make_unique<SJH::PostFxPass>(mat, fbo, mScreenQuadMeshPtr, name);
					if (name == Playable::PASS_INVERT || name == Playable::PASS_BLURRING || name == Playable::PASS_SOBEL)
						fx->Enabled = false; // 기본 비활성.
					mPassIterator.Add(std::move(fx));
				}

				// 5) present(passthrough, output=nullptr) - 마지막 활성 효과 결과를 backbuffer 로 합성. 매 프레임 SetBackbuffer.
				auto present = std::make_unique<SJH::PostFxPass>(mPresentMatPtr, nullptr, mScreenQuadMeshPtr, "present");
				mPresentPassPtr = present.get(); // move 전 캡처.
				mPassIterator.Add(std::move(present));

				// 6) ImGui - 종단 Pass (backbuffer 위에 UI).
				mPassIterator.Add(std::make_unique<UI::ImGuiPass>());
				return true;
			});

			// T5 vfxUi -- VFX 테스트 이펙트 로드 + 게임 UI(PostFX 디버그) + VFX 소환 레이어.
			sched.Task(Bootstrap::EInitTask::VfxUi).Needs({Bootstrap::EInitTask::ScreenPipeline}).Gl().Does([&] {
				std::vector<UI::VfxSpawnLayer::Entry> vfxEntries;
				for (const auto &v : VFX::TEST_EFFECTS)
				{
					if (auto *eff = reg.CreateEffect(vfxs.GetManager(), v.key, v.path))
					{
						vfxEntries.push_back({v.key, eff});
						std::string narrow;
						for (const char16_t *p = v.path; *p; ++p)
							narrow.push_back(static_cast<char>(*p));
					}
					else
						spdlog::warn("[vfx-test] load failed: {}", v.key);
				}

				// ! 제거 대상 std::vector<UI::PassDebugEntry> debugEntries;
				// ! 제거 대상 for (std::size_t i = 0; i < Playable::POSTFX_PROGRAM_CONFIGS.size(); ++i)
				// ! 제거 대상 if (i < mPassComponents.size())
				// ! 제거 대상 debugEntries.push_back({Playable::POSTFX_PROGRAM_CONFIGS[i].Name, mPassComponents[i]});
				mImGuiCtx = UI::BuildGameUI({window, &reg, &mImGuiStack, [this] { TogglePause(); }});

				auto layer = std::make_unique<UI::VfxSpawnLayer>(std::move(vfxEntries));
				mVfxLayer = layer.get();
				mImGuiStack.Push(std::move(layer));

				// Pass 활성화 토글 디버그 패널(Editor kind, F1). mPassIterator 는 Stages task 에서 채워지므로
				// 포인터만 주입(매 프레임 OnBuildUI 가 lazy 조회). grayscale 강도 관찰용 공유 Material 도 주입.
				// capture G1 에서 일시 비활성화를 위해 raw 포인터 캡처 (소유 = 스택).
				{
					auto dbg = std::make_unique<UI::PassDebugLayer>(&mPassIterator, mGrayscaleMatPtr);
					mPassDebugLayerPtr = dbg.get();
					mImGuiStack.Push(std::move(dbg));
				}
				return mImGuiCtx != nullptr;
			});

			sched.RunAll();
		}

		// -- phase 3: 진입/FSM (topo L6~L9) --
		void OnBeforeFirstFrame() override // ! 모듈화 대상 (phase3 Director.Enter + Stage FSM 진입)
		{
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();

			Bootstrap::InitScheduler sched;

			// T6 enter -- Director.Enter (모든 Component OnEnter -- Camera/Light 자동 등록).
			sched.Task(Bootstrap::EInitTask::Enter).Gl().Does([&] {
				dir.Enter();
				return true;
			});

			// T7 fsm -- GameContext + 오버레이 텍스처 + Stage FSM 등록/와이어링/진입.
			sched.Task(Bootstrap::EInitTask::Fsm).Needs({Bootstrap::EInitTask::Enter}).Gl().Does([&] {
				auto *ctxActor = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>(Stage::ACTOR_GAME_CONTEXT));
				mCtx = ctxActor->AddComponent<Stage::Components::GameContextComponent>();

				mCtx->titleTex = reg.CreateTexture(STR_UI_TITLE, SJH::Image::Load(STR_UI_TITLE, PATH_UI_TITLE).get());
				mCtx->pauseTex = reg.CreateTexture(STR_UI_PAUSE, SJH::Image::Load(STR_UI_PAUSE, PATH_UI_PAUSE).get());
				mCtx->gameOverTex = reg.CreateTexture(STR_UI_GAMEOVER, SJH::Image::Load(STR_UI_GAMEOVER, PATH_UI_GAMEOVER).get());

				// blur present 토글 핸들 - PassIterator 에서 키로 조회(방식 B). Title/Pause 동안 Enabled 토글 대상.
				mCtx->blurPass = mPassIterator.Find(Playable::PASS_BLURRING);

				{
					auto overlay = std::make_unique<UI::StateOverlayLayer>();
					mCtx->overlay = overlay.get();
					mImGuiStack.Push(std::move(overlay));
				}

				mStageFsm = std::make_unique<Stage::StageStateMachine>(dir.Root());
				mStageFsm->RegisterState(std::make_unique<Stage::TitleState>(mStageFsm.get()));
				mStageFsm->RegisterState(std::make_unique<Stage::CombatPlayState>(mStageFsm.get()));
				mStageFsm->RegisterState(std::make_unique<Stage::PauseState>(mStageFsm.get()));
				mStageFsm->RegisterState(std::make_unique<Stage::GameOverState>(mStageFsm.get()));

				// WaveController 연결 -- phase 2 에서 만든 WaveSpawner 를 이름으로 조회 (local 승격 회피).
				if (auto *waveSpawner = dir.Root().FindChild(Stage::ACTOR_WAVE_SPAWNER))
				{
					mCtx->waveCtrl = waveSpawner->GetComponent<Stage::WaveController>();
					if (mCtx->waveCtrl)
						mCtx->waveCtrl->SetStageStateMachine(mStageFsm.get());
				}

				mCtx->audio = &TopdownShooter::GameSystems::Get().Audio();
				if (auto *bgmActor = dir.Root().FindChild(Audio::ACTOR_BGM))
					mCtx->bgmPlayable = bgmActor->GetComponent<Audio::FmodStudioPlayable>();
				if (mSpriteActor)
					mCtx->playerLife = mSpriteActor->GetComponent<Entity::Components::Life>();

				mStageFsm->OnEnter();
				return true;
			});

			sched.RunAll();
		}

		void render(double currentTime) override // ! 모듈화 대상 (매프레임 렌더 루프 = capture 가 override 로 재사용하는 핵심)
		{
			// GU0 capture 모드 -- 고정-dt 1/60 로 결정적 시뮬.
			// capture 모드에서는 currentTime 인자를 무시하고 프레임 번호 기반 가상 시간을 사용.
			double effectiveTime = currentTime;
			float dt = static_cast<float>(SJH::DeltaTime(currentTime));
			if (mCaptureMode)
			{
				effectiveTime = mCaptureFrame / 60.0;
				dt = 1.0f / 60.0f;
			}

			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			if (!mDefaultTarget || mDefaultTarget->GetWidth() != fbW || mDefaultTarget->GetHeight() != fbH)
			{
				mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);
				if (mCamera)
					mCamera->Aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
				if (mScreenCamera)
					mScreenCamera->Aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
				mSceneFB->Resize(fbW, fbH); // in-place (포인터 안정)
				for (auto &fb : mPostFXFBs) // 중간 PostFX FB 동기 리사이즈 (PostFxPass 비소유 참조 - 포인터 안정).
					if (fb)
						fb->Resize(fbW, fbH);
				if (mCamera)
					mCamera->SetTargetRenderTarget(mSceneFB.get());
				if (mScreenCamera)
					mScreenCamera->SetTargetRenderTarget(mSceneFB.get());
				RebindFogUniforms(); // resize 후 fog uDepth 방어 재바인딩 (in-place 라 no-op이나 의미 보존, D2).
			}

			// ImGui NewFrame 우선 — io.WantCaptureMouse/Keyboard 가 입력 디스패치에 영향.
			ImGui_ImplGlfwGL3_NewFrame();

			mKeyboard.PollHeld(window);
			if (mCamera && mCamera->GetOwner())
				TopdownShooter::GameSystems::Get().Audio().SetListener(mCamera->GetOwner()->GetTransform().Translate);

			// Stage FSM — 게임 로직 위탁. CombatPlayState 만 GameSystems/Director/Physics Update(D5 freeze).
			//   NewFrame(260) 직후라 State 의 ImGui::IsKeyPressed/IsMouseClicked(frame-edge) 유효.
			if (mStageFsm)
				mStageFsm->Update(dt);

			// 오디오 펌프 — 게임 sim freeze 와 무관하게 매 프레임(ungated). 이게 없으면 Title/Pause 에서
			// FMOD Studio update 가 안 돌아 Play()/setParameter(BGM_STATE/Health) 명령이 처리되지 않음
			// (Title BGM 무음 버그의 원인). FSM Update 뒤에 둬 listener/파라미터 갱신을 함께 flush.
			TopdownShooter::GameSystems::Get().Audio().Update(dt);

			// 지연 FX 스폰 flush -- Life seam(VFX::Spawn / WorldText::SpawnDamage)이 Director::Update
			// 순회 *도중*(UltimateLaser RaycastAll->DoDamaged) 적재한 요청을 여기(순회 밖)서 실제 AddChild.
			// 순회 중 직접 AddChild 시 fxRoot.mChildren 재할당 -> Actor::Update 라이브 iterator
			// 무효화(SIGSEGV) 회피 (SweepDespawned 와 동일 deferred 패턴).
			TopdownShooter::VFX::FlushSpawns();
			TopdownShooter::WorldText::FlushSpawns();
			if (mFxRoot)
				TopdownShooter::Spawns::SweepFinishedChildren(*mFxRoot);
			// 디졸브 끝난 사망 적을 RemoveChild -> OnExit -> Physics::OnExit::DestroyBody (deferred — Director.Update 밖이라 iterator 안전).
			if (mCtx && mCtx->waveCtrl)
				mCtx->waveCtrl->SweepDespawned();

			// 스카이박스 시간(u_time) 동기화 — 위치는 셰이더가 view 이동 제거로 자동 처리.
			// capture 모드에서는 effectiveTime(프레임 번호 기반 가상 시간) 사용 -> 결정적 시뮬.
			if (mSkyboxMat)
				mSkyboxMat->Properties.Floats[UNI_SKYBOX_TIME] = static_cast<float>(effectiveTime);

			// fog — WorldCamera projection 역행렬 송신 (Properties.Mat4s 자동 송신, D4).
			// Camera 의 닫힌 해 inverse (cofactor 일반 inverse 폐기 — perspective/ortho 분기 자체 처리).
			if (auto *fogMat = FindFogMaterial(); fogMat && mCamera)
				fogMat->Properties.Mat4s[UNI_FOG_INV_PROJ] =
				    mCamera->GetInverseProjectionMatrix();

			// worldCam 라이트 업로드 - WorldPass 자체 uploader 에 스냅샷 주입(3.1). (SceneRenderer screen 경로 폐기 - 3.5)
			if (mWorldPassPtr)
				mWorldPassPtr->SetActivePrograms(SJH::ResourceRegistry::Get().GetAllPrograms());

			// grayscale present(output=nullptr)의 backbuffer 는 resize 마다 재생성되므로 매 프레임 주입.
			if (mPresentPassPtr)
				mPresentPassPtr->SetBackbuffer(mDefaultTarget.get());
			// ImGui 창 빌드 (GL draw 아님 - command 기록만). 종단 ImGuiPass 가 ImGui::Render 로 발행하기 전에 빌드.
			mImGuiStack.RenderAll(mShowEditor);

			// stages 실행 - PassIterator 가 before/GetPassResult 체이닝으로 순회(현재 BeforeIndex=-1 -> before=nullptr, 사전배선 사용).
			// ! 제거 대상  [S ! kybox, World, Particle, PostFx(e0..eN), ScreenQuad, ImGuiPass]. ImGuiPass(종단)가 ImGui::Render.
			mPassIterator.Execute(SJH::DeviceContext::Get(), *mDefaultTarget);

			// GU1 capture 모드 -- 180 프레임 도달 시 3 변형 렌더+캡처 후 종료.
			// 변형 순서: G1(전체, PassDebugLayer 제외) -> G2(ImGui 제외) -> G3(Skybox+PostFX+ScreenQuad 만).
			if (mCaptureMode)
			{
				if (mCaptureFrame == 180)
				{
					// test/golden/ 디렉토리 생성 보장 -- 실행 cwd 기준(build_ninja/apps/_MyApp_/).
					const std::string outDir = "test/golden";
					std::filesystem::create_directories(outDir);

					// --- G1: 전체 파이프라인 (PassDebugLayer 만 제외, 다른 ImGui 유지) ---
					// PassDebugLayer(Editor kind) 를 일시 비활성화 후 ImGui 프레임을 재빌드+실행하여 캡처.
					if (mPassDebugLayerPtr)
						mPassDebugLayerPtr->Enabled = false;
					ImGui_ImplGlfwGL3_NewFrame();
					mImGuiStack.RenderAll(mShowEditor);
					mPassIterator.Execute(SJH::DeviceContext::Get(), *mDefaultTarget);
					if (mPassDebugLayerPtr)
						mPassDebugLayerPtr->Enabled = true;
					const bool okG1 = TopdownShooter::Capture::CaptureBackbufferToPng(
					    outDir + "/golden_full.png", fbW, fbH);
					spdlog::info("[GoldenCapture] G1 {}", okG1 ? "OK" : "FAIL");

					// --- G2: ImGui 제외 (Skybox/World/Particle/PostFX/present 만) ---
					// DebugPassIndex 를 ImGuiPass 직전 인덱스로 설정해 ImGuiPass 를 건너뜀.
					// Keys() 마지막 = "ImGui" -> Keys().size()-2 = ImGuiPass 직전 인덱스.
					{
						const int stopAt = static_cast<int>(mPassIterator.Keys().size()) - 2;
						mPassIterator.DebugPassIndex = stopAt;
						mPassIterator.Execute(SJH::DeviceContext::Get(), *mDefaultTarget);
						mPassIterator.DebugPassIndex = -1;
					}
					const bool okG2 = TopdownShooter::Capture::CaptureBackbufferToPng(
					    outDir + "/golden_no_imgui.png", fbW, fbH);
					spdlog::info("[GoldenCapture] G2 {}", okG2 ? "OK" : "FAIL");

					// --- G3: Skybox+PostFX+present 만 (World/Particle/ImGui 제외) ---
					// WorldPass 와 ParticlePass 를 일시 비활성화 후 Execute, 복원.
					// DebugPassIndex = present 인덱스(Keys().size()-2) 로 ImGui 까지 스킵.
					{
						auto *worldPass = mPassIterator.Find("World");
						auto *particlePass = mPassIterator.Find("Particle");
						if (worldPass)
							worldPass->Enabled = false;
						if (particlePass)
							particlePass->Enabled = false;
						const int stopAt = static_cast<int>(mPassIterator.Keys().size()) - 2;
						mPassIterator.DebugPassIndex = stopAt;
						mPassIterator.Execute(SJH::DeviceContext::Get(), *mDefaultTarget);
						mPassIterator.DebugPassIndex = -1;
						if (worldPass)
							worldPass->Enabled = true;
						if (particlePass)
							particlePass->Enabled = true;
					}
					const bool okG3 = TopdownShooter::Capture::CaptureBackbufferToPng(
					    outDir + "/golden_skybox.png", fbW, fbH);
					spdlog::info("[GoldenCapture] G3 {}", okG3 ? "OK" : "FAIL");

					// glfwSetWindowShouldClose 로 sb7 run 루프 정상 종료.
					// GLFW_TRUE 는 3.2+ 이후 정의 -- sb7 내장 GLFW 3.0.4 는 1 로 대체.
					glfwSetWindowShouldClose(window, 1);
				}
				++mCaptureFrame;
			}
		}

		void shutdown() override // ! 모듈화 대상 (자원/씬 정리 = boot 의 역순)
		{
			ImGui_ImplGlfwGL3_Shutdown();
			ImGui::DestroyContext(mImGuiCtx);
			mImGuiCtx = nullptr;

			SJH::Scene::Director::Get().Exit();
			mCamera = nullptr;
			mScreenCamera = nullptr;
			mSpriteActor = nullptr;
			mFxRoot = nullptr;
			mStageFsm.reset(); // States 해제 (WaveController->mStageFsm 는 이후 미사용)
			mCtx = nullptr;
			mWorldPassPtr = nullptr;
			mPresentPassPtr = nullptr;
			mSkyboxRenderer = nullptr;
			mGrayscaleMatPtr = nullptr;
			mPresentMatPtr = nullptr;
			mScreenQuadMeshPtr = nullptr;
			mPassIterator.Clear(); // GL 컨텍스트 살아있는 동안 Pass 해제 (PostFxPass 가 mPostFXFBs 참조 - 먼저).
			mPostFXFBs.clear();    // 중간 FBO 해제 (Pass 해제 후 - dangling 방지).
			mDefaultTarget.reset();
			TopdownShooter::GameSystems::Get().Shutdown();
		}

		void onKey(int key, int action) override // ! 모듈화 대상 (키 입력 핸들러 = 게임루프 입력)
		{
			ImGui_ImplGlfwGL3_KeyCallback(window, key, /*scancode*/ 0, action, /*mods*/ 0);
			if (ImGui::GetIO().WantCaptureKeyboard)
				return;

			// F1 — 에디터 토글 (디버그 UI, 플레이어 입력과 무관 -> application 잔류).
			if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
				mShowEditor = !mShowEditor;

			// 게임플레이 키(WASD held + G Damage)는 PlayerController 가 바인딩 — Dispatch 로 위임.
			// (G Damage Composite 로직은 WramupPlayer 의 onDamage 콜백으로 이동.)
			mKeyboard.Dispatch(key, action);
		}

		void onMouseButton(int button, int action) override // ! 모듈화 대상 (마우스 버튼 핸들러 = 게임루프 입력)
		{
			ImGui_ImplGlfwGL3_MouseButtonCallback(window, button, action, /*mods*/ 0);
			if (ImGui::GetIO().WantCaptureMouse)
				return;

			// 좌클릭(Shot Composite)은 MouseInput -> PlayerController fire 콜백이 디스패치.
			// (콜백 로직은 WramupPlayer 의 onFire 로 이동.) 여기선 raw 버튼만 MouseInput 에 전달.
			double x = 0.0, y = 0.0;
			glfwGetCursorPos(window, &x, &y);
			mMouse.HandleButton(button, action, x, y);
		}

		void onMouseMove(int x, int y) override // ! 모듈화 대상 (마우스 이동 핸들러 = 게임루프 입력)
		{
			// ImGui v1.53 은 NewFrame 시 직접 glfwGetCursorPos 폴링 — forward 불필요.
			if (ImGui::GetIO().WantCaptureMouse)
				return;
			mMouse.HandleMove(static_cast<double>(x), static_cast<double>(y));
		}

		void onResize(int /*logicalW*/, int /*logicalH*/) override // ! 모듈화 대상 (리사이즈 핸들러 = 렌더 타깃 재생성)
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
		// ── 캡처 모드 멤버 (GU0) ────────────────────────────────────────────────────
		/// SJH_GOLDEN_CAPTURE 환경 변수 설정 시 capture 모드 활성화.
		bool mCaptureMode = false; // ! 모듈화 대상 (capture 전용 상태 → test/ 캡처 모듈; AppRunner 아님)
		/// capture 모드에서 누적된 렌더 프레임 수. 180 도달 후 PNG 저장 + 종료.
		int mCaptureFrame = 0; // ! 모듈화 대상 (capture 전용 상태 → test/ 캡처 모듈; AppRunner 아님)

		// ── 멤버 ────────────────────────────────────────────────────────────────────
		SJH::FramebufferInfo mFbInfo{}; ///< startup 캐시 -- 3 hook 이 공유하는 window/fb 크기/비율. // ! 모듈화 대상 (창/fb 정보 = boot+render 공유 상태)
		SJH::RenderTargetUPtr mDefaultTarget; // ! 모듈화 대상 (backbuffer 렌더 타깃 = 파이프라인 출력)
		// SP-RenderStage 완성 — Application 이 stages 컬렉션을 명시 순서로 순회.
		SJH::PassIterator mPassIterator;            ///< 모든 Pass 소유(unique_ptr) + 코스 순서 실행 + Find(key) 조회. // ! 모듈화 대상 (전 패스 소유 = 렌더 파이프라인 본체)
		SJH::WorldPass *mWorldPassPtr = nullptr;    ///< worldCam WorldPass(비소유 관찰, 소유=PassIterator) - 매 프레임 SetActivePrograms 주입. // ! 모듈화 대상 (매프레임 SetActivePrograms 주입점 = 렌더루프 결합)
		SJH::PostFxPass *mPresentPassPtr = nullptr; ///< present PostFxPass(passthrough, output=nullptr, 비소유 관찰) - 매 프레임 SetBackbuffer 주입. // ! 모듈화 대상 (매프레임 SetBackbuffer 주입점 = 렌더루프 결합)
		SJH::Mesh *mScreenQuadMeshPtr = nullptr;    ///< PostFxPass blit 용 screen quad (비소유, 소유=ResourceRegistry). // ! 모듈화 대상 (PostFX blit mesh = 파이프라인 자원)
		SJH::Material *mGrayscaleMatPtr = nullptr;  ///< grayscale_vignetting 공유 Material (HpGrayscalePostFX 와 SSOT 공유, 소유=ResourceRegistry). // ! 모듈화 대상 (PostFX 머티리얼 = 파이프라인 자원)
		SJH::Material *mPresentMatPtr = nullptr;    ///< present passthrough Material ("mat_pass_present", 소유=ResourceRegistry). // ! 모듈화 대상 (present 머티리얼 = 파이프라인 자원)

		SJH::FramebufferUPtr mSceneFB; // ! 모듈화 대상 (씬 렌더 FBO + depth(fog) = 파이프라인 중간 출력)
		std::vector<SJH::FramebufferUPtr> mPostFXFBs; ///< 효과별 중간 FBO (POSTFX_PROGRAM_CONFIGS 와 1:1, 소유). resize 동기. // ! 모듈화 대상 (PostFX 중간 FBO = 파이프라인 자원)

		// ImGui
		ImGuiContext *mImGuiCtx = nullptr; // ! 모듈화 대상 (ImGui 컨텍스트 = UI 레이어)
		UI::ImGuiLayerStack mImGuiStack; // ! 모듈화 대상 (ImGui 레이어 스택 = UI)
		UI::VfxSpawnLayer *mVfxLayer = nullptr;        // VFX 테스트 드롭다운 (비소유 — 스택이 소유) // ! 모듈화 대상 (VFX UI 레이어)
		UI::IImGuiLayer *mPassDebugLayerPtr = nullptr; // PassDebugLayer raw 포인터 (비소유 — 스택이 소유). capture G1 일시 비활성화용. // ! 모듈화 대상 (디버그 UI + capture G1 토글 결합)
		bool mShowEditor = true; // ! 모듈화 대상 (에디터 토글 상태 = UI/입력)

		// 씬 오브젝트
		SJH::Material *mSkyboxMat = nullptr;         ///< skybox Material (비소유, 소유=ResourceRegistry). // ! 모듈화 대상 (매프레임 u_time 주입점 = 렌더루프 결합)
		SJH::IRenderable *mSkyboxRenderer = nullptr; ///< skybox MeshRenderer(IRenderable) - SkyboxPass 주입용(비소유). // ! 모듈화 대상 (SkyboxPass 주입 대상 = 파이프라인)
		SJH::Scene::Actor *mSpriteActor = nullptr; // ! 모듈화 대상 (플레이어 = 씬 상태)
		SJH::Scene::Actor *mFxRoot = nullptr; // 단발 시퀀스 전용 부모 (sweep 대상) // ! 모듈화 대상 (FX 부모 = 씬 상태)
		SJH::Scene::Camera *mCamera = nullptr; // ! 모듈화 대상 (world 카메라 = 렌더 입력)
		SJH::Scene::Camera *mScreenCamera = nullptr; // ! 모듈화 대상 (화면 카메라 = 렌더 입력)
		SJH::KeyboardInput<Controller::PlayerController::Action> mKeyboard; // ! 모듈화 대상 (입력 = 게임루프)
		SJH::MouseInput mMouse; // ! 모듈화 대상 (입력 = 게임루프)

		// Stage FSM (Hybrid) — render() 의 게임로직 Update 를 State 가 게이트.
		std::unique_ptr<Stage::StageStateMachine> mStageFsm; // ! 모듈화 대상 (게임로직 FSM = 루프 드라이브)
		Stage::Components::GameContextComponent *mCtx = nullptr; // 비소유 — Root 의 GameContext actor 가 소유 // ! 모듈화 대상 (게임 컨텍스트 와이어링 = 상태)

		// ! 모듈화 대상 — 위 mFbInfo~mCtx 의존 멤버 전체(렌더 타깃 · PassIterator · PostFX 자원 · 씬 ·
		//   시스템(GameSystems) · ImGui 스택 · Stage FSM)는 "부트 + 렌더루프" 단위로 추출 예정.
		//   golden capture 가 main 무침투로 이 파이프라인을 재사용하기 위한 모듈화 경계.
		//   (capture 전용 mCaptureMode / mCaptureFrame 은 test/ 캡처 모듈로 별도 분리.)

		// PostFX 공유 Material 단축 조회 — rr 의 mat_pass_<name> (ScreenPipeline 에서 생성한 것만 존재).
		// fog 패스는 현재 미생성이라 nullptr 반환 → 호출부(uInverseProjection/uDepth 송신)는 자연 no-op.
		SJH::Material *FindFogMaterial() // ! 모듈화 대상 (fog 머티리얼 조회 헬퍼 = 파이프라인 보조)
		{
			return SJH::ResourceRegistry::Get().FindSharedMaterial(std::string("mat_pass_") + Playable::PASS_FOG);
		}

		// fog material 의 uDepth 를 현재 mSceneFB depth 텍스처(unit 1)로 (재)바인딩. fog 미생성 시 guard no-op.
		// startup + resize 직후 호출 — sceneFB 재생성 시 dangling 방지 (D2).
		void RebindFogUniforms() // ! 모듈화 대상 (fog uDepth 재바인딩 = 파이프라인 보조, resize 시 호출)
		{
			auto *fogMat = FindFogMaterial();
			if (!fogMat || !mSceneFB || !mSceneFB->GetDepthAttachment())
				return;
			fogMat->Properties.Textures[UNI_FOG_DEPTH] = {mSceneFB->GetDepthAttachment().get(), 1}; // unit 1 (uScene=0).
		}

		// Pause 버튼(PauseButtonLayer) 콜백 — 현재 State 기준 CombatPlay↔Pause 토글.
		void TogglePause() // ! 모듈화 대상 (Pause 버튼 콜백 = 게임로직 FSM 토글)
		{
			if (!mStageFsm)
				return;
			const auto s = mStageFsm->State();
			if (s == Stage::EStageStatus::CombatPlay)
				mStageFsm->TryTransit(Stage::EStageStatus::Pause);
			else if (s == Stage::EStageStatus::Pause)
				mStageFsm->TryTransit(Stage::EStageStatus::CombatPlay);
		}
	};

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
