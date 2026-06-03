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
#include "Spawns/OneShotSweeper.h"
#include "Spawns/VfxInstance.h"
#include "Spawns/WorldTextInstance.h"   // <- 추가 (데모 트리거)
#include "UI/VfxSpawnLayer.h"

#include "Stage/StageBuilder.h"
#include "Stage/WaveController.h"
#include "UI/ImGuiLayerStack.h"
#include "UI/PostFXDebugLayer.h"
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
		// D-6 data-driven — gamma 초기값을 InitFloats 로 명시.
		const std::vector<SJH::Render::PostFXStageConfig> POSTFX_PROGRAM_CONFIGS = {
		    // 실행 순서 재배열 (2026-06-01 사용자 지정): gamma->sharpening->bloom->fog->invert->blur->sobel.
		    {"gamma",      "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/gamma.fs",      {{"gamma", 1.0f}}},
		    {"sharpening", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sharpening.fs", {}},
		    {"bloom",      "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/bloom.fs",
		     {{"uBloomThreshold", 0.769f}, {"uBloomSpread", 2.342f}, {"uBloomIntensity", 0.927f}}},
		    {"fog",        "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/fog.fs",
		     {{"uFogDensity", 0.042f}, {"uFogStart", 0.0f}, {"uFogEnd", 50.0f}}},
		    {"grayscale_vignetting", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/grayscale_vignetting.fs",
		     {{"uGrayscaleAmount", 1.0f}, {"uVignetteAmount", 0.0f}}}, // uVignetteColor(vec3)는 startup 에서 set.
		    {"invert",     "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/invert.fs",     {}},
		    {"blurring",   "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/blurring.fs",   {}},
		    {"sobel",      "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sobel.fs",      {}},
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
				fogMat->Properties.Vec3s["uFogColor"] = vmath::vec3(20.0f / 255.0f, 36.0f / 255.0f, 10.0f / 255.0f); // {20,36,10}
				fogMat->Properties.Ints["uFogMode"]   = 2; // 0=Linear, 1=Exp, 2=Exp2
			}
			RebindFogUniforms(); // uDepth = sceneFB depth 텍스처 (unit 1).

			// grayscale_vignetting 의 vec3 초기값 (InitFloats 밖) — 비네팅 색 명시 set.
			if (auto *gvMat = FindPassMaterial("grayscale_vignetting"))
				gvMat->Properties.Vec3s["uVignetteColor"] = vmath::vec3(1.0f, 0.0f, 0.0f); // {255,0,0} 빨강 비네팅.

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

			auto player = Bootstrap::BuildPlayer({&mKeyboard, &mMouse, &phys.World(), mCamera});
			mSpriteActor = player.SpriteActor;

			// 적 웨이브 스폰 트리거 — player(mSpriteActor) 생성 이후라야 SimplePursueAI 타깃 유효.
			// root 하위 Component 라 Director::Update(dt) 가 자동 tick. arenaHalfExtent 는 StageConfig 기본(10.0f)과 일치.
			auto *waveSpawner = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("WaveSpawner"));
			waveSpawner->AddComponent<Stage::WaveController>(&phys.World(), waveSpawner, mSpriteActor, 10.0f);

			// UI — render(PostFX) ↔ UI 매핑은 Composition Root(main) 책임. 빌더는 결과만 받음.
			std::vector<UI::PassDebugEntry> debugEntries;
			for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size(); ++i)
				if (i < mPassComponents.size())
					debugEntries.push_back({POSTFX_PROGRAM_CONFIGS[i].Name, mPassComponents[i]});
			mImGuiCtx = UI::BuildGameUI({window, &reg, &mImGuiStack, std::move(debugEntries), &mGamma});

			// VFX 테스트 드롭다운 (항상 표시) + 좌클릭 Ground 좌표 -> 선택 이펙트 소환.
			{
				auto layer = std::make_unique<UI::VfxSpawnLayer>(std::move(vfxEntries));
				mVfxLayer  = layer.get();
				mImGuiStack.Push(std::move(layer));
			}
			if (mSpriteActor)
				if (auto *pc = mSpriteActor->GetComponent<Controller::PlayerController>())
					pc->SetGroundClickCallback([this](const vmath::vec3 &p) {
						// PlayerController 의 마우스->Ground raycast 결과(p)에 선택 이펙트를 단발 스폰.
						if (mVfxLayer && mFxRoot)
							if (auto *fx = mVfxLayer->GetSelectedEffect())
								TopdownShooter::Spawns::SpawnVfxInstance(
								    *mFxRoot, &TopdownShooter::Manager::Get().VFX(), fx, p);

						// 데모 — 클릭 지점에 데미지 텍스트 (World Text 검증)
						if (mFxRoot)
							if (auto *font = TopdownShooter::Manager::Get().WorldText().GetFont())
							{
								// 1~3자리 자릿수별 center 정렬 확인용 더미값 (실제 데미지는 전투 배선 시 — spec 범위 밖)
								static const int demoVals[] = {5, 42, 137, 9, 88, 250, 1, 76, 999};
								static std::size_t demoN = 0;
								const std::string dmg = "-" + std::to_string(demoVals[demoN++ % (sizeof(demoVals) / sizeof(demoVals[0]))]);
								TopdownShooter::Spawns::WorldTextStyle style;
								style.scale = 0.5f;   // 0.5배 사이즈다운 (Transform.Scale 합성)
								TopdownShooter::Spawns::SpawnWorldText(*mFxRoot, font, p, dmg, style);
							}
					});

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
			TopdownShooter::Manager::Get().Update(dt);
			SJH::Scene::Director::Get().Update(dt);
			if (mFxRoot) TopdownShooter::Spawns::SweepFinishedChildren(*mFxRoot);
			TopdownShooter::Manager::Get().Physics().SyncToTransform(SJH::Scene::Director::Get().Root());

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

	};

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
