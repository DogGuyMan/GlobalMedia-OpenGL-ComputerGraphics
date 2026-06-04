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
#include "Bootstrap/AudioWarmup.h"
#include "Bootstrap/PlayerBuilder.h"
#include "Bootstrap/WorldSceneBuilder.h"
#include "InputHandler/PlayerController.h"
#include "Manager.h"
#include "VFX/ParticleStage.h"
#include "VFX/Constants.h"   // MUZZLE_EFFECT / TEST_EFFECTS (VFX 자원 테이블)

#include "diagnostics/effekseer_diagnostics.h"   // VFX 텍스처 로드 검증
#include "Playable/PostFXRegistry.h"             // 연출 foundation — PostFX pass Material 레지스트리
#include "Playable/PostFXConstants.h"            // PostFX 파이프라인 정의(PASSTHOURH/POSTFX_PROGRAM_CONFIGS) + fog/vignette 색
#include "Spawns/OneShotSweeper.h"
#include "Spawns/VfxInstance.h"
#include "Spawns/WorldTextInstance.h"   // <- 추가 (데모 트리거)
#include "UI/VfxSpawnLayer.h"

#include "Stage/StageBuilder.h"
#include "Stage/WaveController.h"
#include "Stage/Constants.h"   // Stage::ARENA_HALF_EXTENT
#include "Stage/Stage.h"       // EStageStatus (TogglePause)
#include "Stage/Components/GameContextComponent.h"
#include "Stage/State/StageStateMachine.h"
#include "Stage/State/StageState.Impl.h"   // Title/CombatPlay/Pause/GameOver + GetCtx
#include "UI/ImGuiLayerStack.h"
#include "UI/PostFXDebugLayer.h"
#include "UI/StateOverlayLayer.h"
#include "UI/UiBootstrap.h"
#include "buffer/framebuffer.h"
#include "common/common.h"
#include "common/window_helper.h"
#include "render/camera_stage.h"
#include "render/pass_component.h"
#include "render/render_pipeline.h"
#include "render/render_target.h"
#include "render/scene_renderer.h"
#include "render/screen_quad_stage.h"
#include "resource_registry/resource_registry.h"
#include "resource_registry/image.h"        // SJH::Image::Load
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace TopdownShooter
{
	namespace
	{
		// PostFX 파이프라인 정의(ProgramConfig/PASSTHOURH/POSTFX_PROGRAM_CONFIGS)는
		// Playable/PostFXConstants.h 로 이관 — 여기선 using 으로 노출(기존 unqualified 사용처 보존).
		using Playable::PASSTHOURH_PROGRAM_CONFIG;
		using Playable::POSTFX_PROGRAM_CONFIGS;
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

			// 기본 창을 16:9 창모드로 (sb7 기본 800×600=4:3 덮어쓰기).
			// macOS Retina 는 실제 framebuffer 가 2배(2560×1440)지만 비율은 16:9 유지.
			// fullscreen 은 기본 0(windowed). GLFW 3.0 이라 aspect 하드락 API(3.2+) 는 없음 — 초기 크기만 지정.
			info.windowWidth  = 1280;
			info.windowHeight = 720;

			SJH::CrossPlatformDir();
		}

		void startup() override
		{
			// A1 — GLFW window 정보 1 호출.
			const auto fb = SJH::GetFramebufferInfo(window);
			glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

			auto &manager = TopdownShooter::Manager::Get();
			manager.Init();
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();
			auto &phys = TopdownShooter::Manager::Get().Physics();
			auto &vfxs = TopdownShooter::Manager::Get().VFX();

			mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fb.Width, fb.Height);
			mSceneFB       = SJH::Framebuffer::CreateWithDepthTexture(fb.Width, fb.Height);

			// A3 — RenderPipeline 셋업 + ScreenQuadStage 받아 mStages push.
			SJH::Render::DefaultPipelineConfig pipelineCfg{
			    PASSTHOURH_PROGRAM_CONFIG.Name,
			    PASSTHOURH_PROGRAM_CONFIG.VertFile,
			    PASSTHOURH_PROGRAM_CONFIG.FragFile,
			    // ScreenQuadMeshKey / BypassMatKey 는 default
			};
			auto sqStage = SJH::Render::SetupDefaultPipeline(reg, manager.SceneRenderer(), mSceneFB.get(), pipelineCfg);
			mScreenQuadStagePtr = sqStage.get();
			mStages.push_back(std::move(sqStage));

			// WorldScene (camera + light + skybox) — Bootstrap 빌더로 추출 (B/C 그룹).
			// stages 가 mCamera 의존이라 stages 셋업보다 먼저 호출. light·skybox 도 함께
			// 생성되나 Light=SceneRenderer 매 프레임 수집 / Skybox=Pass queue 정렬이라 순서 무관(시각 동일).
			auto worldScene = Bootstrap::BuildWorldScene({fb.Aspect, &mMouse, mSceneFB.get()});
			mCamera    = worldScene.WorldCamera;
			mSkyboxMat = worldScene.SkyboxMat;

			// A2 — ScreenCamera Pure factory.
			auto screenCamActor = SJH::Scene::CreateScreenCameraActor("ScreenCamera", fb.Aspect, mSceneFB.get());
			mScreenCamera = screenCamActor->GetComponent<SJH::Scene::Camera>();
			auto* screenCamActorPtr = dir.Root().AddChild(std::move(screenCamActor));

			// A4 — PostFX 체인 빌드.
			auto chain = SJH::Render::BuildPostFXChain(reg, *screenCamActorPtr, POSTFX_PROGRAM_CONFIGS, mSceneFB.get(), fb.Width, fb.Height);
			mPostFXFBs      = std::move(chain.Framebuffers);
			mPassComponents = std::move(chain.PassComponents);

			// invert/blurring/sobel 만 기본 비활성 (자동활성화 취소) — 나머지 PostFX 는 기본 ON.
			// ImGui(F1) 체크박스로 토글. (Enabled=false ⇒ mesh_pass_processor 가 bypass blit.)
			for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size() && i < mPassComponents.size(); ++i)
			{
				const auto &name = POSTFX_PROGRAM_CONFIGS[i].Name;
				if (mPassComponents[i] && (name == "invert" || name == "blurring" || name == "sobel"))
					mPassComponents[i]->Enabled = false;
			}

			// fog 의 non-float 초기값 + uDepth 바인딩 (PostFXStageConfig.InitFloats 는 Floats 만 지원).
			if (auto *fogMat = FindFogMaterial())
			{
				fogMat->Properties.Vec3s["uFogColor"] = Playable::FOG_COLOR; // {20,36,10}
				fogMat->Properties.Ints["uFogMode"]   = Playable::FOG_MODE; // 0=Linear, 1=Exp, 2=Exp2
			}
			RebindFogUniforms(); // uDepth = sceneFB depth 텍스처 (unit 1).

			// grayscale_vignetting 의 vec3 초기값 (InitFloats 밖) — 비네팅 색 명시 set.
			if (auto *gvMat = FindPassMaterial("grayscale_vignetting"))
				gvMat->Properties.Vec3s["uVignetteColor"] = Playable::VIGNETTE_COLOR; // {255,0,0} 빨강 비네팅.

			// 연출 foundation — PostFX pass Material 을 레지스트리에 등록 (hit-FX 트랙의 PostFXTweenPlayable 이
			// PostFXRegistry::Get().Material("grayscale_vignetting")->Properties 로 도달). pass material 유효 지점.
			TopdownShooter::Playable::PostFXRegistry::Get().Register("grayscale_vignetting", FindPassMaterial("grayscale_vignetting"));

			// ── stages 컬렉션 — World -> Particle -> Screen -> ScreenQuad 순 ─────────
			// ScreenQuadStage 는 Step 3 에서 이미 mStages 에 push 된 상태.
			// 카메라 stages 를 *ScreenQuadStage 앞* 에 insert + ParticleStage 는
			// 아래 별도 블록에서 begin()+1 위치에 삽입 — 최종 순서:
			//   [0] worldCam      -> sceneFB clear + 3D WorldMesh
			//   [1] ParticleStage -> sceneFB (NoClear) + Effekseer 합성   (※ 아래 블록)
			//   [2] screenCam     -> PassComponent 체인 (NoClear)
			//   [3] ScreenQuadStage -> backbuffer 합성
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

			Bootstrap::WarmupAudio(Manager::Get().Audio());
			reg.CreateEffect(vfxs.GetManager(), VFX::MUZZLE_EFFECT.key, VFX::MUZZLE_EFFECT.path);

			// VFX 테스트 — 6종 Effekseer 이펙트 로드 (ImGui 드롭다운 선택 + 좌클릭 위치 소환).
			// ※ .efk 가 참조하는 텍스처가 resources/vfx/ 아래에 있어야 실제로 보인다 (현재 누락 가능 — 별도 배치 필요).
			std::vector<UI::VfxSpawnLayer::Entry> vfxEntries;
			{
				for (const auto &v : VFX::TEST_EFFECTS)
				{
					if (auto *eff = reg.CreateEffect(vfxs.GetManager(), v.key, v.path))
					{
						vfxEntries.push_back({v.key, eff});
						// 진단 — .efk 가 참조하는 텍스처가 실제 해석되는지 검증 (u16 경로 -> ASCII narrow).
						std::string narrow;
						for (const char16_t *p = v.path; *p; ++p)
							narrow.push_back(static_cast<char>(*p));
						SJH::Diagnostics::EffekseerDiagnostics::CheckEffectTextures(narrow);
					}
					else
						spdlog::warn("[vfx-test] load failed: {}", v.key);
				}
			}

			dir.Root().AddChild(std::move(TopdownShooter::Stage::CreateStageActor({
			    &phys.World(),
			    &reg,
			})));

			mFxRoot = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("FxRoot"));

			// VFX::Spawn 파사드 컨텍스트 등록 — seam(dust/hit/gunshoot)이 이 fxRoot+VFXSystem 으로 단발 스폰.
			VFX::SetSpawnContext(mFxRoot, &vfxs);

			auto player = Bootstrap::BuildPlayer({&mKeyboard, &mMouse, &phys.World(), mCamera});
			mSpriteActor = player.SpriteActor;

			// 적 웨이브 스폰 트리거 — player(mSpriteActor) 생성 이후라야 SimplePursueAI 타깃 유효.
			// root 하위 Component 라 Director::Update(dt) 가 자동 tick. arenaHalfExtent 는 StageConfig 기본(10.0f)과 일치.
			auto *waveSpawner = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("WaveSpawner"));
			waveSpawner->AddComponent<Stage::WaveController>(&phys.World(), waveSpawner, mSpriteActor, Stage::ARENA_HALF_EXTENT);

			// UI — render(PostFX) ↔ UI 매핑은 Composition Root(main) 책임. 빌더는 결과만 받음.
			std::vector<UI::PassDebugEntry> debugEntries;
			for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size(); ++i)
				if (i < mPassComponents.size())
					debugEntries.push_back({POSTFX_PROGRAM_CONFIGS[i].Name, mPassComponents[i]});
			mImGuiCtx = UI::BuildGameUI({window, &reg, &mImGuiStack, std::move(debugEntries), &mGamma, [this] { TogglePause(); }});

			// VFX 테스트 드롭다운 (항상 표시) + 좌클릭 Ground 좌표 -> 선택 이펙트 소환.
			{
				auto layer = std::make_unique<UI::VfxSpawnLayer>(std::move(vfxEntries));
				mVfxLayer  = layer.get();
				mImGuiStack.Push(std::move(layer));
			}
			dir.Enter();

			// ── Stage FSM (Hybrid) ──────────────────────────────────────────────
			// 게임 로직(Manager/Director/Physics Update)을 State 가 게이트. startup 본문 유지.
			// GameContext — State 가 owner.FindChild("GameContext") 로 접근하는 공유 컨텍스트.
			auto *ctxActor = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("GameContext"));
			mCtx           = ctxActor->AddComponent<Stage::Components::GameContextComponent>();

			// 오버레이 텍스처 (Title/Pause/GameOver) 로드.
			mCtx->titleTex    = reg.CreateTexture("ui_title", SJH::Image::Load("ui_title", "resources/texture/Title.png").get());
			mCtx->pauseTex    = reg.CreateTexture("ui_pause", SJH::Image::Load("ui_pause", "resources/texture/Pause.png").get());
			mCtx->gameOverTex = reg.CreateTexture("ui_gameover", SJH::Image::Load("ui_gameover", "resources/texture/GameOver.png").get());

			// blur PostFX 핸들 — TitleState 가 OnEnter/OnExit 에서 Enabled 토글 (기본 OFF).
			for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size() && i < mPassComponents.size(); ++i)
				if (mPassComponents[i] && POSTFX_PROGRAM_CONFIGS[i].Name == "blurring")
					mCtx->blurPass = mPassComponents[i];

			// 오버레이 레이어 — ImGuiLayerStack 이 소유(마지막 push = 최상위). ctx 엔 비소유 raw.
			{
				auto overlay  = std::make_unique<UI::StateOverlayLayer>();
				mCtx->overlay = overlay.get();
				mImGuiStack.Push(std::move(overlay));
			}

			// StateMachine — game_application 멤버 보유(D1, Actor 부착 안 함). startup=Title.
			mStageFsm = std::make_unique<Stage::StageStateMachine>(dir.Root());
			mStageFsm->RegisterState(std::make_unique<Stage::TitleState>(mStageFsm.get()));
			mStageFsm->RegisterState(std::make_unique<Stage::CombatPlayState>(mStageFsm.get()));
			mStageFsm->RegisterState(std::make_unique<Stage::PauseState>(mStageFsm.get()));
			mStageFsm->RegisterState(std::make_unique<Stage::GameOverState>(mStageFsm.get()));

			// WaveController 연결 — Player 사망 감시 → GameOver 전이 구동(S5b).
			mCtx->waveCtrl = waveSpawner->GetComponent<Stage::WaveController>();
			if (mCtx->waveCtrl)
				mCtx->waveCtrl->SetStageStateMachine(mStageFsm.get());

			// FMOD — Stage State 가 구동할 핸들 주입 (OnEnter 전에 세팅 필수).
			//   BgmActor 는 WarmupAudio→BuildBGM 이 Root 직속으로 생성 (이미 존재).
			mCtx->audio = &TopdownShooter::Manager::Get().Audio();
			if (auto *bgmActor = dir.Root().FindChild("BgmActor"))
				mCtx->bgmPlayable = bgmActor->GetComponent<Audio::FmodStudioPlayable>();
			if (mSpriteActor)
				mCtx->playerLife = mSpriteActor->GetComponent<Entity::Components::Life>();

			mStageFsm->OnEnter(); // curState=Title → TitleState::OnEnter (overlay->Show(titleTex) + BGM_STATE=0)
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
				mSceneFB->Resize(fbW, fbH);                       // 교체 -> in-place (포인터 안정: 첫 PassComponent.InputFB dangling 해소)
				for (auto &fb : mPostFXFBs)                       // 중간 FB 동기 리사이즈 (스케일 불일치 해소)
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
				TopdownShooter::Manager::Get().Audio().SetListener(mCamera->GetOwner()->GetTransform().Translate);

			// Stage FSM — 게임 로직 위탁. CombatPlayState 만 Manager/Director/Physics Update(D5 freeze).
			//   NewFrame(260) 직후라 State 의 ImGui::IsKeyPressed/IsMouseClicked(frame-edge) 유효.
			if (mStageFsm)
				mStageFsm->Update(dt);

			// 오디오 펌프 — 게임 sim freeze 와 무관하게 매 프레임(ungated). 이게 없으면 Title/Pause 에서
			// FMOD Studio update 가 안 돌아 Play()/setParameter(BGM_STATE/Health) 명령이 처리되지 않음
			// (Title BGM 무음 버그의 원인). FSM Update 뒤에 둬 listener/파라미터 갱신을 함께 flush.
			TopdownShooter::Manager::Get().Audio().Update(dt);

			if (mFxRoot) TopdownShooter::Spawns::SweepFinishedChildren(*mFxRoot);
			// 디졸브 끝난 사망 적을 RemoveChild -> OnExit -> Physics::OnExit::DestroyBody (deferred — Director.Update 밖이라 iterator 안전).
			if (mCtx && mCtx->waveCtrl) mCtx->waveCtrl->SweepDespawned();

			// 스카이박스 시간(u_time) 동기화 — 위치는 셰이더가 view 이동 제거로 자동 처리.
			if (mSkyboxMat)
			{
				mSkyboxMat->Properties.Floats["u_time"] = static_cast<float>(currentTime);
			}

			// fog — WorldCamera projection 역행렬 송신 (Properties.Mat4s 자동 송신, D4).
			// Camera 의 닫힌 해 inverse (cofactor 일반 inverse 폐기 — perspective/ortho 분기 자체 처리).
			if (auto *fogMat = FindFogMaterial(); fogMat && mCamera)
				fogMat->Properties.Mat4s["uInverseProjection"] =
				    mCamera->GetInverseProjectionMatrix();

			// ── stages 컬렉션 순회 — World -> Screen -> ScreenQuad ─────────────────
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
			mFxRoot = nullptr;
			mStageFsm.reset();   // States 해제 (WaveController->mStageFsm 는 이후 미사용)
			mCtx = nullptr;
			mStages.clear();
			mDefaultTarget.reset();
			TopdownShooter::Manager::Get().Shutdown();
		}

		void onKey(int key, int action) override
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

		void onMouseButton(int button, int action) override
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
		std::vector<SJH::FramebufferUPtr> mPostFXFBs;  // configs 가변 size 대응 (D-6)
		std::vector<SJH::Scene::PassComponent *> mPassComponents; // PostFXDebug ImGui 패널이 토글 대상 참조 — 비소유 raw
		float mGamma = 1.0f;

		// ImGui
		ImGuiContext *mImGuiCtx = nullptr;
		UI::ImGuiLayerStack mImGuiStack;
		UI::VfxSpawnLayer  *mVfxLayer = nullptr; // VFX 테스트 드롭다운 (비소유 — 스택이 소유)
		bool mShowEditor = true;

		// 씬 오브젝트
		SJH::Material *mSkyboxMat = nullptr;
		SJH::Scene::Actor *mSpriteActor = nullptr;
		SJH::Scene::Actor *mFxRoot = nullptr; // 단발 시퀀스 전용 부모 (sweep 대상)
		SJH::Scene::Camera *mCamera = nullptr;
		SJH::Scene::Camera *mScreenCamera = nullptr;
		SJH::KeyboardInput<Controller::PlayerController::Action> mKeyboard;
		SJH::MouseInput mMouse;

		// Stage FSM (Hybrid) — render() 의 게임로직 Update 를 State 가 게이트.
		std::unique_ptr<Stage::StageStateMachine> mStageFsm;
		Stage::Components::GameContextComponent  *mCtx = nullptr; // 비소유 — Root 의 GameContext actor 가 소유

		// 이름으로 PostFX PassComponent 의 Material 탐색 — POSTFX_PROGRAM_CONFIGS 와 mPassComponents 인덱스 정합.
		// (PostFXStageConfig::Name 은 std::string -> operator==(name) 는 정상 문자열 비교.)
		SJH::Material *FindPassMaterial(const char *name)
		{
			for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size() && i < mPassComponents.size(); ++i)
				if (mPassComponents[i] && POSTFX_PROGRAM_CONFIGS[i].Name == name)
					return mPassComponents[i]->mMaterial;
			return nullptr;
		}

		// fog material 단축 — uInverseProjection / uDepth 송신부에서 사용.
		SJH::Material *FindFogMaterial() { return FindPassMaterial("fog"); }

		// fog material 의 uDepth 를 현재 mSceneFB 의 depth 텍스처(unit 1)로 (재)바인딩.
		// startup + resize 직후 호출 — sceneFB 재생성 시 dangling 방지 (D2).
		void RebindFogUniforms()
		{
			auto *fogMat = FindFogMaterial();
			if (!fogMat || !mSceneFB || !mSceneFB->GetDepthAttachment())
				return;
			fogMat->Properties.Textures["uDepth"] = {mSceneFB->GetDepthAttachment().get(), 1}; // unit 1 (uScene=0).
		}

		// Pause 버튼(PauseButtonLayer) 콜백 — 현재 State 기준 CombatPlay↔Pause 토글.
		void TogglePause()
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
