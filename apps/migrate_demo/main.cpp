/**
 * @file main.cpp
 * @brief migrate_demo — OpenGL-With-CMake/src/context/context.cpp 를 SJH Actor/Component
 *        그래프 + SceneRenderer 으로 마이그레이트 (ImGui 컨트롤 + Stencil Outline + FlashLight 포함).
 *
 * @details
 *  ### 씬 (레퍼런스 회귀)
 *  - Plane / Box1 / Box2 — Phong 텍스쳐 머티리얼 (sampler2D material.diffuse/specular)
 *  - Outline — Box2 자식, scale 1.05, **stencil!=1 + depth off** 로 rim 만 노출
 *  - Windows × 3 — alpha discard 투명 텍스쳐
 *  - DirLight + PointLight × 2 + SpotLight (각 점/스포트 광원 위치에 마커 큐브)
 *
 *  ### 멀티 패스 PostFX (알파벳 순 체인)
 *  - SceneCamera (depth=0, SceneFB) — 본체 + 아웃라인 (stencil) 통합 렌더
 *  - PostFX 패스 5종 (depth=1..5, resources/shader/postprocess/&lt;name&gt;.fs):
 *      blurring -> gamma -> invert -> sharpening -> sobel
 *  - 각 패스: 자기 layer + 자기 camera + 자기 quad + 자기 intermediate FB 보유.
 *  - 매 프레임 활성 패스만 추려 chain 동적 재배선:
 *      input = (첫 패스 ? SceneFB : 이전 활성 패스의 outputFB)
 *      target = (마지막 활성 패스 ? backbuffer : 자기 outputFB)
 *  - ImGui 체크박스로 각 fx 개별 on/off — 끄면 해당 camera/renderer 자동 skip.
 *  - 모두 비활성 시 backbuffer 가 검정 — 의도된 단순화.
 *
 *  ### 조작
 *  - WASD/EQ : 이동, 우클릭 드래그 : 시점 회전
 *  - ImGui 패널 (좌상): 광원 enable / FlashLight / PostFX 토글 / gamma / 카메라 reset
 *  - F : FlashLight 모드 토글 (스포트 광원이 카메라 추종)
 *
 *  ### ImGui = Client 코드 (Core Module 아님)
 *  - apps/migrate_demo/CMakeLists.txt 가 extern/imgui v1.53 의 4 cpp 를 직접 컴파일.
 *  - sb7 의 입력 콜백 (onKey/onMouseButton) 에서 ImGui 콜백으로 *수동 forward*.
 */

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <sb7.h>

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include "buffer/framebuffer.h"
#include "common/common.h"
#include "common/layer.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "material/material_uniforms.h"
#include "object/light.h"
#include "render/scene_renderer.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "render/mesh_renderer.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"

#include <array>
#include <cstdlib>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>
#include <vmath.h>

#include "client.h"

namespace K = MigrateDemo::Constants;

namespace
{
	struct PostFXDef
	{
		const char *Name;
		const char *FragFile;
	};
	constexpr std::array<PostFXDef, 5> kPostFXDefs = {{
	    {"blurring", "resources/shader/postprocess/blurring.fs"},
	    {"gamma", "resources/shader/postprocess/gamma.fs"},
	    {"invert", "resources/shader/postprocess/invert.fs"},
	    {"sharpening", "resources/shader/postprocess/sharpening.fs"},
	    {"sobel", "resources/shader/postprocess/sobel.fs"},
	}};

	constexpr const char *kPostFXVertFile = "resources/shader/postprocess/postprocess.vs";

} // namespace

class migrate_demo_app : public sb7::application
{
  public:
	void init() override
	{
		sb7::application::init();
		info.majorVersion = 4;
		info.minorVersion = 1; // GLSL 410 정통 (memory: glsl_410_project_policy)

		// macOS GLFW chdir workaround — SJH::common 흡수
		SJH::ChdirToExecutableDir();
	}

	void startup() override
	{
		auto &reg = SJH::ResourceRegistry::Get();

		// === 1) Scene programs (Phong / Simple / Window) ===
		MigrateDemo::Scene::Programs progs;
		progs.phong  = reg.CreateProgram(K::Programs::PhongName,  K::Programs::PhongVS,  K::Programs::PhongFS);
		progs.simple = reg.CreateProgram(K::Programs::SimpleName, K::Programs::SimpleVS, K::Programs::SimpleFS);
		progs.window = reg.CreateProgram(K::Programs::WindowName, K::Programs::WindowVS, K::Programs::WindowFS);
		if (!progs.phong || !progs.simple || !progs.window)
		{
			spdlog::error(K::LogMsg::SceneShaderLoadFail);
			std::exit(1);
		}

		int fbW = 0, fbH = 0;
		glfwGetFramebufferSize(window, &fbW, &fbH);
		onResize(fbW, fbH);

		MigrateDemo::Scene::WarmupAssets(reg);

		// SceneFB — Scene 의 본체 렌더 타겟. PostFX 체인의 첫 입력.
		mSceneFB = SJH::Framebuffer::Create(fbW, fbH);
		if (!mSceneFB)
		{
			spdlog::error(K::LogMsg::SceneFBFail);
			std::exit(1);
		}

		// 씬 워밍업
		auto &dir = SJH::Scene::Director::Get();
		MigrateDemo::Scene::WarmupActors(progs, dir);
		mRefs = MigrateDemo::Scene::WarmupLights(progs, dir);

		// SceneCamera + Controller
		const float aspect = static_cast<float>(info.windowWidth) /
		                     static_cast<float>(info.windowHeight);
		auto sceneCamActor = SJH::Scene::CreateCameraActor(K::Actors::SceneCamera,
		                                                   45.0f, aspect, 0.1f, 100.0f);
		sceneCamActor->SetLayer(SJH::LAYER_SCENE);
		sceneCamActor->GetTransform().Translate = vmath::vec3(0.0f, 2.5f, 8.0f);
		sceneCamActor->GetTransform().EulerRot = vmath::vec3(-20.0f, 0.0f, 0.0f);

		auto *sceneCam = sceneCamActor->GetComponent<SJH::Scene::Camera>();
		sceneCam->Depth = 0;
		sceneCam->CullingMask = SJH::LAYER_SCENE;
		sceneCam->SetTargetFramebuffer(mSceneFB.get());

		auto *ctrl = sceneCamActor->AddComponent<MigrateDemo::Controller::CameraController>();
		ctrl->SetKeyboardInput(&mKeyboard)
		    .SetMouseInput(&mMouse)
		    .SetCamera(sceneCam)
		    .SetUp();

		mSceneCameraActor = sceneCamActor.get();
		dir.Root().AddChild(std::move(sceneCamActor));

		// === PostFX 5-pass 체인 빌드 (알파벳 순) ===
		BuildPostFXChain(reg, dir, fbW, fbH, aspect);

		dir.SetActiveCamera(sceneCam);
		dir.Enter();

		// === ImGui v1.53 init ===
		// install_callbacks=false — sb7 가 GLFW key/mouseButton 콜백을 이미 소유. 수동 forward.
		mImGuiCtx = ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplGlfwGL3_Init(window, /*install_callbacks*/ false);
		// 사용 안 하는 채널 (scroll/char) 은 ImGui 가 직접 받도록 — sb7 는 미사용.
		glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
		glfwSetCharCallback(window, ImGui_ImplGlfwGL3_CharCallback);

		spdlog::info(K::LogMsg::StartupDone);
	}

	void render(double currentTime) override
	{
		const float dt = static_cast<float>(SJH::DeltaTime(currentTime));

		// macOS Retina 호환 — info.windowWidth/Height (logical) 가 아닌 *physical* 사용.
		// GLFW 가 내부 cache 한 값 반환이라 매 프레임 호출 비용 미미.
		// SP-RTOwnership — Application 이 default backbuffer 의 owner.
		{
			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			if (!mDefaultTarget || mDefaultTarget->GetWidth() != fbW || mDefaultTarget->GetHeight() != fbH)
				mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);
		}

		// ImGui NewFrame 우선 — io.WantCaptureMouse/Keyboard 가 입력 디스패치에 영향.
		ImGui_ImplGlfwGL3_NewFrame();

		// F 키 — FlashLight 토글 (KeyboardInput<TAction> 이 enum 단일 고정이라 직접 폴링).
		{
			const bool fNow = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
			if (fNow && !mFKeyPrev && !ImGui::GetIO().WantCaptureKeyboard)
				mUI.flashLight = !mUI.flashLight;
			mFKeyPrev = fNow;
		}

		// UI 패널 빌드 + 상태 씬 반영.
		BuildUI();
		ApplyUIState();

		// 키보드 held 핸들러 폴링 — ImGui 가 키보드 캡쳐 중이 아니면.
		ImGuiIO &io = ImGui::GetIO();
		if (!io.WantCaptureKeyboard)
			mKeyboard.PollHeld(window);

		// Scene tick + 멀티 카메라 렌더 (SceneFB -> PostFX chain -> backbuffer).
		SJH::Scene::Director::Get().Update(dt);
		mRenderSys.Render(*mDefaultTarget);

		// ImGui draws — v1.53 의 io.RenderDrawListsFn 콜백을 통해 자동 (현재 backbuffer 에).
		ImGui::Render();
	}

	void shutdown() override
	{
		SJH::Scene::Director::Get().Exit();
		ImGui_ImplGlfwGL3_Shutdown();
		ImGui::DestroyContext(mImGuiCtx);
	}

	// === sb7 입력 콜백 -> ImGui forward + 게임 디스패치 ===
	void onKey(int key, int action) override
	{
		ImGui_ImplGlfwGL3_KeyCallback(window, key, /*scancode*/ 0, action, /*mods*/ 0);
		// ImGui 가 키보드 캡쳐 중이면 게임 dispatch 차단.
		if (ImGui::GetIO().WantCaptureKeyboard)
			return;
		mKeyboard.Dispatch(key, action);
	}

	void onMouseButton(int button, int action) override
	{
		ImGui_ImplGlfwGL3_MouseButtonCallback(window, button, action, /*mods*/ 0);
		if (ImGui::GetIO().WantCaptureMouse)
			return;
		double x = 0.0, y = 0.0;
		glfwGetCursorPos(window, &x, &y);
		mMouse.HandleButton(button, action, x, y);
	}

	void onMouseMove(int x, int y) override
	{
		// ImGui v1.53 은 cursor pos 를 NewFrame 시 직접 glfwGetCursorPos 로 폴링 — forward 불필요.
		if (ImGui::GetIO().WantCaptureMouse)
			return;
		mMouse.HandleMove(static_cast<double>(x), static_cast<double>(y));
	}

	void onResize(int logicalW, int logicalH) override
	{
		// sb7 는 glfwSetWindowSizeCallback 으로 *logical* size 전달 — macOS Retina 에서
		// physical framebuffer 와 2배 차이. viewport / FB 는 *physical* 기준이 정통.
		int w = 0, h = 0;
		glfwGetFramebufferSize(window, &w, &h);
		if (w <= 0 || h <= 0) return;

		sb7::application::onResize(w, h);   // base 의 info.windowWidth/Height 도 physical 로 갱신.
		glViewport(0, 0, w, h);
		auto &dir = SJH::Scene::Director::Get();
		mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(w, h);

		// 모든 카메라 aspect 일괄 갱신 (Scene + PostFX 5개).
		const float aspect = static_cast<float>(w) / static_cast<float>(h);
		for (auto &child : dir.Root().GetChildren())
		{
			if (auto *cam = child->GetComponent<SJH::Scene::Camera>())
				cam->Aspect = aspect;
		}

		// SceneFB 재생성 + 같은 FB 가리키던 카메라 target 일괄 재바인딩.
		if (mSceneFB)
		{
			if (auto newSceneFB = SJH::Framebuffer::Create(w, h))
			{
				const auto *oldFB = mSceneFB.get();
				for (auto &child : dir.Root().GetChildren())
				{
					auto *cam = child->GetComponent<SJH::Scene::Camera>();
					if (cam && cam->GetTargetFramebuffer() == oldFB)
						cam->SetTargetFramebuffer(newSceneFB.get());
				}
				mSceneFB = std::move(newSceneFB);
			}
		}

		// PostFX intermediate FB 재생성. uScene / camera target 재배선은 다음 프레임의
		// ApplyUIState() 가 *항상 처음부터* 다시 wire 하므로 여기서는 FB 만 교체.
		for (auto &p : mPostFX)
		{
			if (auto newFB = SJH::Framebuffer::Create(w, h))
				p.OutputFB = std::move(newFB);
		}
	}

  private:
	struct PostFXPass
	{
		const char *Name = nullptr;
		SJH::Material *Material = nullptr;
		SJH::Scene::Camera *Camera = nullptr;
		SJH::Scene::MeshRenderer *Renderer = nullptr;
		SJH::FramebufferUPtr OutputFB; ///< 매 패스마다 자기 intermediate FB 보유 (resize 시 재생성).
		bool Enabled = true;
	};

	struct UIState
	{
		bool dirLightEnabled = true;
		bool pointLightsEnabled[2] = {true, true};
		bool spotLightEnabled = true;
		bool flashLight = false; // 스포트가 카메라 추종 모드.
		float gamma = 1.0f;      // gamma.fs 의 `gamma` uniform 송신값.
		float clearColor[3] = {0.0f, 0.1f, 0.2f};
	};

	/// @brief PostFX 5-pass 체인 초기화 — 알파벳 순.
	void BuildPostFXChain(SJH::ResourceRegistry &reg,
	                      SJH::Scene::Director &dir,
	                      int width, int height, float aspect)
	{
		auto *quadMesh = reg.FindMesh(K::Meshes::ScreenQuad);
		if (!quadMesh)
		{
			spdlog::error(K::LogMsg::PostFXQuadMissing);
			std::exit(1);
		}

		for (std::size_t i = 0; i < kPostFXDefs.size(); ++i)
		{
			auto &def = kPostFXDefs[i];
			const auto layer = SJH::LayerBit(i);

			// Program — 이름충돌 방지 위해 prefix 부여.
			const std::string progKey = std::string(K::PostFXKey::ProgramPrefix) + def.Name;
			auto *prog = reg.CreateProgram(progKey, kPostFXVertFile, def.FragFile);
			if (!prog)
			{
				spdlog::error(K::LogMsg::PostFXShaderFailFmt, def.FragFile);
				std::exit(1);
			}

			// Material — sampler uScene 은 ApplyUIState 가 매 프레임 chain 의 input 으로 재바인딩.
			const std::string matKey = std::string(K::PostFXKey::MaterialPrefix) + def.Name;
			auto *mat = reg.CreateSharedMaterial(matKey);
			mat->SetProgram(prog);

			// Intermediate FB — 항상 생성. 활성 여부와 무관 (resize 시 재생성 단순화).
			auto outputFB = SJH::Framebuffer::Create(width, height);
			if (!outputFB)
			{
				spdlog::error(K::LogMsg::PostFXFBFailFmt, def.Name);
				std::exit(1);
			}

			// Quad Actor (PostFXCamera 의 culling mask 와 같은 layer bit).
			auto quad = std::make_unique<SJH::Scene::Actor>(
			    std::string(K::PostFXKey::QuadPrefix) + def.Name);
			quad->SetLayer(layer);
			auto *renderer = quad->AddComponent<SJH::Scene::MeshRenderer>(quadMesh, mat);
			dir.Root().AddChild(std::move(quad));

			// Camera Actor — depth 1..5 (SceneCamera 의 depth=0 다음).
			auto camActor = std::make_unique<SJH::Scene::Actor>(
			    std::string(K::PostFXKey::CameraPrefix) + def.Name);
			camActor->SetLayer(layer);
			auto *cam = camActor->AddComponent<SJH::Scene::Camera>(
			    45.0f, aspect, 0.1f, 100.0f);
			cam->Depth = static_cast<int>(i) + 1;
			cam->CullingMask = layer;
			// 초기 target — ApplyUIState 가 매 프레임 재배선. 임시로 자기 outputFB 가리킴.
			cam->SetTargetFramebuffer(outputFB.get());
			dir.Root().AddChild(std::move(camActor));

			PostFXPass &p = mPostFX[i];
			p.Name = def.Name;
			p.Material = mat;
			p.Camera = cam;
			p.Renderer = renderer;
			p.OutputFB = std::move(outputFB);
			p.Enabled = true;
		}
	}

	/// @brief 좌상단 UI 패널 — Lights / PostFX / Camera 섹션.
	void BuildUI()
	{
		if (!ImGui::Begin(K::UI::WindowTitle))
		{
			ImGui::End();
			return;
		}

		if (ImGui::CollapsingHeader(K::UI::HeaderLights, ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox(K::UI::DirLight,    &mUI.dirLightEnabled);
			ImGui::Checkbox(K::UI::PointLight0, &mUI.pointLightsEnabled[0]);
			ImGui::Checkbox(K::UI::PointLight1, &mUI.pointLightsEnabled[1]);
			ImGui::Checkbox(K::UI::SpotLight,   &mUI.spotLightEnabled);
			ImGui::Separator();
			ImGui::Checkbox(K::UI::FlashLightMode, &mUI.flashLight);
			if (mRefs.spotLight)
			{
				ImGui::DragFloat (K::UI::SpotCutoff,      &mRefs.spotLight->CutoffAngleDeg,      0.1f, 0.0f, 89.0f);
				ImGui::DragFloat (K::UI::SpotOuterCutoff, &mRefs.spotLight->OuterCutoffAngleDeg, 0.1f, 0.0f, 90.0f);
				ImGui::DragFloat (K::UI::SpotDistance,    &mRefs.spotLight->Distance,            0.5f, 1.0f, 3250.0f);
				ImGui::ColorEdit3(K::UI::SpotDiffuse,     reinterpret_cast<float *>(&mRefs.spotLight->Diffuse));
			}
		}

		if (ImGui::CollapsingHeader(K::UI::HeaderPostFX, ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (auto &p : mPostFX)
				ImGui::Checkbox(p.Name, &p.Enabled);
			ImGui::Separator();
			ImGui::SliderFloat(K::UI::GammaSlider, &mUI.gamma, 0.1f, 2.5f);
		}

		if (ImGui::CollapsingHeader(K::UI::HeaderCamera))
		{
			if (mSceneCameraActor)
			{
				auto &t = mSceneCameraActor->GetTransform();
				ImGui::DragFloat3(K::UI::Position, reinterpret_cast<float *>(&t.Translate), 0.05f);
				ImGui::DragFloat3(K::UI::EulerRot, reinterpret_cast<float *>(&t.EulerRot),  0.5f);
				if (ImGui::Button(K::UI::ResetCamera))
				{
					t.Translate = vmath::vec3(0.0f, 2.5f, 8.0f);
					t.EulerRot  = vmath::vec3(-20.0f, 0.0f, 0.0f);
				}
			}
			ImGui::ColorEdit3(K::UI::ClearColor, mUI.clearColor);
		}

		ImGui::End();
	}

	/// @brief UI 상태 -> 씬 반영. 매 프레임 호출.
	/// @details 핵심 작업:
	///   1) 광원 컴포넌트 enable 토글.
	///   2) FlashLight 모드 — 스포트 액터 transform 을 카메라 추종.
	///   3) gamma uniform 송신 (gamma.fs 만 사용 — Apply 가 cache miss 인 다른 mat 에는 skip).
	///   4) PostFX 체인 재배선 — 활성 패스만 추려 input/output chain wire.
	///   5) Clear color 갱신.
	void ApplyUIState()
	{
		// 1) 광원 enable 토글.
		if (mRefs.dirLight)
			mRefs.dirLight->SetEnabled(mUI.dirLightEnabled);
		if (mRefs.pointLights[0])
			mRefs.pointLights[0]->SetEnabled(mUI.pointLightsEnabled[0]);
		if (mRefs.pointLights[1])
			mRefs.pointLights[1]->SetEnabled(mUI.pointLightsEnabled[1]);
		if (mRefs.spotLight)
			mRefs.spotLight->SetEnabled(mUI.spotLightEnabled);

		// 2) FlashLight 모드.
		if (mUI.flashLight && mRefs.spotLightActor && mSceneCameraActor)
		{
			auto &camT = mSceneCameraActor->GetTransform();
			auto &spotT = mRefs.spotLightActor->GetTransform();
			spotT.Translate = camT.Translate;
			spotT.EulerRot = camT.EulerRot;
		}

		// 3) gamma uniform — gamma.fs 의 `gamma` uniform (소문자).
		for (auto &p : mPostFX)
		{
			if (p.Material)
				SJH::Uniforms::SetFloat(*p.Material, K::PostFXKey::GammaUniform, mUI.gamma);
		}

		// 4) PostFX chain 재배선.
		//    활성 패스만 추려 i 번째 활성 패스의 input = (이전 활성 패스 outputFB) | SceneFB,
		//    마지막 활성 패스만 backbuffer (nullptr) 로 출력. 비활성은 SetEnabled(false).
		std::vector<PostFXPass *> active;
		active.reserve(mPostFX.size());
		for (auto &p : mPostFX)
		{
			p.Camera->SetEnabled(p.Enabled);
			p.Renderer->SetEnabled(p.Enabled);
			if (p.Enabled)
				active.push_back(&p);
		}

		for (std::size_t k = 0; k < active.size(); ++k)
		{
			auto *p = active[k];
			auto *input = (k == 0) ? 
				mSceneFB.get() : 
				active[k - 1]->OutputFB.get();
			const bool isLast = (k + 1 == active.size());

			// uScene 재바인딩 — PropertyBlock 에 store 만 (실제 GL bind 는 PropertyBlockSetter::Set 시).
			p->Material->Properties.Textures[K::PostFXKey::USceneSampler] =
			    {input->GetColorAttachment().get(), 0};

			// 카메라 target — 마지막 활성 패스만 backbuffer.
			p->Camera->SetTargetFramebuffer(isLast ? nullptr : p->OutputFB.get());
		}

		// 5) Clear color — DeviceContext::BeginFrame 의 clear 가 GL state 의 glClearColor 사용.
		glClearColor(mUI.clearColor[0], mUI.clearColor[1], mUI.clearColor[2], 1.0f);
	}

	SJH::KeyboardInput<MigrateDemo::Controller::CameraController::Action> mKeyboard;
	SJH::MouseInput mMouse;
	SJH::SceneRenderer mRenderSys;
	SJH::Scene::Actor *mSceneCameraActor = nullptr;
	SJH::RenderTargetUPtr mDefaultTarget; 
	SJH::FramebufferUPtr mSceneFB;
	
	MigrateDemo::Scene::SceneRefs mRefs;
	
	std::array<PostFXPass, kPostFXDefs.size()> mPostFX;
	UIState mUI;
	ImGuiContext *mImGuiCtx = nullptr;
	bool mFKeyPrev = false; // F 키 edge detect.
};

DECLARE_MAIN(migrate_demo_app);
