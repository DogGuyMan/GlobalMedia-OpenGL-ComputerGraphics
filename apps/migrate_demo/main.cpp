/**
 * @file main.cpp
 * @brief migrate_demo — OpenGL-With-CMake/src/context/context.cpp 를 SJH Actor/Component
 *        그래프 + RenderSystem 으로 마이그레이트 (ImGui 컨트롤 + Stencil Outline + FlashLight 포함).
 *
 * @details
 *  ### 씬 (레퍼런스 회귀)
 *  - Plane / Box1 / Box2 — Phong 텍스쳐 머티리얼 (sampler2D material.diffuse/specular)
 *  - Outline — Box2 자식, scale 1.05, **stencil!=1 + depth off** 로 rim 만 노출
 *  - Windows × 3 — alpha discard 투명 텍스쳐
 *  - DirLight + PointLight × 2 + SpotLight (각 점/스포트 광원 위치에 마커 큐브)
 *
 *  ### 멀티 패스
 *  - SceneCamera (depth=0, SceneFB) — 본체 + 아웃라인 (stencil) 통합 렌더
 *  - PostFXCamera (depth=1, backbuffer) — 7-tap 블러
 *  - ImGui 패널은 ImGui::Render 가 별도 backbuffer 에 그림 (블러 *위* 에 overlay)
 *
 *  ### 조작
 *  - WASD/EQ : 이동, 우클릭 드래그 : 시점 회전
 *  - ImGui 패널 (좌상): 광원 enable / FlashLight / Gamma / 카메라 reset
 *  - F : FlashLight 모드 토글 (스포트 광원이 카메라 추종)
 *
 *  ### ImGui = Client 코드 (Core Module 아님)
 *  - apps/migrate_demo/CMakeLists.txt 가 extern/imgui v1.53 의 4 cpp 를 직접 컴파일.
 *  - sb7 의 입력 콜백 (onKey/onMouseButton) 에서 ImGui 콜백으로 *수동 forward*.
 */

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <sb7.h>

// stb_image 정의 책임은 SJH::resource_registry 가 이미 보유 (image.cpp).

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include "buffer/framebuffer.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "material/material_uniforms.h"
#include "object/light.h"
#include "render/render_context.h"
#include "render/render_system.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/components.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"

#include <cstdlib>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>
#include <vmath.h>

#ifdef __APPLE__
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#endif

#include "client.h"

class migrate_demo_app : public sb7::application
{
  public:
	void init() override
	{
		sb7::application::init();
		info.majorVersion = 4;
		info.minorVersion = 1; // GLSL 410 정통 (memory: glsl_410_project_policy)

#ifdef __APPLE__
		char exePath[PATH_MAX] = {};
		uint32_t exeSize = static_cast<uint32_t>(sizeof(exePath));
		if (_NSGetExecutablePath(exePath, &exeSize) == 0)
		{
			char exePathCopy[PATH_MAX] = {};
			strncpy(exePathCopy, exePath, PATH_MAX - 1);
			chdir(dirname(exePathCopy));
		}
#endif
	}

	void startup() override
	{
		auto &reg = SJH::ResourceRegistry::Get();

		// === 1) Programs ===
		MigrateDemo::Scene::Programs progs;
		progs.phong = reg.CreateProgram("phong_tex",
		                                 "resources/shaders/phong_tex.vs",
		                                 "resources/shaders/phong_tex.fs");
		progs.simple = reg.CreateProgram("simple",
		                                  "resources/shaders/simple.vs",
		                                  "resources/shaders/simple.fs");
		progs.window = reg.CreateProgram("window",
		                                  "resources/shaders/window.vs",
		                                  "resources/shaders/window.fs");
		auto *postfxProg = reg.CreateProgram("postfx_blur",
		                                      "resources/shaders/postprocess/postprocess.vs",
		                                      "resources/shaders/postprocess/blurring.fs");
		if (!progs.phong || !progs.simple || !progs.window || !postfxProg)
		{
			spdlog::error("migrate_demo shader 로드 실패");
			std::exit(1);
		}

		int fbW = 0, fbH = 0;
		glfwGetFramebufferSize(window, &fbW, &fbH);
		onResize(fbW, fbH);

		MigrateDemo::Scene::WarmupAssets(reg);

		// === SceneFB + PostFX 머티리얼 ===
		mSceneFB = SJH::Framebuffer::Create(fbW, fbH);
		if (!mSceneFB)
		{
			spdlog::error("SceneFB 생성 실패");
			std::exit(1);
		}

		auto *postfxMat = reg.CreateMaterial("mat_postfx");
		postfxMat->SetProgram(postfxProg);
		postfxMat->Textures["uScene"] = {mSceneFB->GetColorAttachment().get(), 0};
		SJH::Uniforms::SetFloat(*postfxMat, "uGamma", mUI.gamma);

		// === 씬 워밍업 ===
		auto &dir = SJH::Scene::Director::Get();
		MigrateDemo::Scene::WarmupActors(progs, dir);
		mRefs = MigrateDemo::Scene::WarmupLights(progs, dir);

		// === ScreenQuad ===
		{
			auto quad = std::make_unique<SJH::Scene::Actor>("PostFXQuad");
			quad->SetLayer(MigrateDemo::Scene::LAYER_POSTFX);
			quad->AddComponent<SJH::Scene::MeshRenderer>(
			    reg.FindMesh("mesh_screen_quad"), postfxMat,
			    MigrateDemo::Scene::QUEUE_OPAQUE);
			dir.Root().AddChild(std::move(quad));
		}

		// === SceneCamera + Controller ===
		const float aspect = static_cast<float>(info.windowWidth) /
		                     static_cast<float>(info.windowHeight);
		auto sceneCamActor = SJH::Scene::CreateCameraActor("SceneCamera",
		                                                    45.0f, aspect, 0.1f, 100.0f);
		sceneCamActor->SetLayer(MigrateDemo::Scene::LAYER_SCENE);
		sceneCamActor->GetTransform().Translate = vmath::vec3(0.0f, 2.5f, 8.0f);
		sceneCamActor->GetTransform().EulerRot  = vmath::vec3(-20.0f, 0.0f, 0.0f);

		auto *sceneCam = sceneCamActor->GetComponent<SJH::Scene::Camera>();
		sceneCam->Depth       = 0;
		sceneCam->CullingMask = MigrateDemo::Scene::LAYER_SCENE;
		sceneCam->SetTargetFramebuffer(mSceneFB.get());

		auto *ctrl = sceneCamActor->AddComponent<MigrateDemo::Controller::CameraController>();
		ctrl->SetKeyboardInput(&mKeyboard)
		    .SetMouseInput(&mMouse)
		    .SetCamera(sceneCam)
		    .SetUp();

		mSceneCameraActor = sceneCamActor.get();
		dir.Root().AddChild(std::move(sceneCamActor));

		// === PostFXCamera ===
		{
			auto fxCamActor = std::make_unique<SJH::Scene::Actor>("PostFXCamera");
			fxCamActor->SetLayer(MigrateDemo::Scene::LAYER_POSTFX);
			auto *fxCam = fxCamActor->AddComponent<SJH::Scene::Camera>(
			    45.0f, aspect, 0.1f, 100.0f);
			fxCam->Depth       = 1;
			fxCam->CullingMask = MigrateDemo::Scene::LAYER_POSTFX;
			dir.Root().AddChild(std::move(fxCamActor));
		}

		dir.SetActiveCamera(sceneCam);
		dir.Enter();
		mLastTime = 0.0;

		// === ImGui v1.53 init ===
		// install_callbacks=false — sb7 가 GLFW key/mouseButton 콜백을 이미 소유. 수동 forward.
		mImGuiCtx = ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplGlfwGL3_Init(window, /*install_callbacks*/ false);
		// 사용 안 하는 채널 (scroll/char) 은 ImGui 가 직접 받도록 — sb7 는 미사용.
		glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
		glfwSetCharCallback (window, ImGui_ImplGlfwGL3_CharCallback);

		spdlog::info("migrate_demo startup 완료. WASD/QE 이동, 우클릭 드래그 회전, F=FlashLight.");
	}

	void render(double currentTime) override
	{
		const float dt = static_cast<float>(currentTime - mLastTime);
		mLastTime = currentTime;

		SJH::RenderContext::Get().SetDefaultTargetSize(info.windowWidth, info.windowHeight);

		// ImGui NewFrame 우선 — io.WantCaptureMouse/Keyboard 가 입력 디스패치에 영향.
		ImGui_ImplGlfwGL3_NewFrame();

		// F 키 — FlashLight 토글 (KeyboardInput<TAction> 이 enum 단일 고정이라 직접 폴링).
		{
			const bool fNow = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
			if (fNow && !mFKeyPrev && !ImGui::GetIO().WantCaptureKeyboard)
				mUI.flashLight = !mUI.flashLight;
			mFKeyPrev = fNow;
		}

		// UI 패널 빌드 + 상태  씬 반영.
		BuildUI();
		ApplyUIState();

		// 키보드 held 핸들러 폴링 — ImGui 가 키보드 캡쳐 중이 아니면.
		ImGuiIO &io = ImGui::GetIO();
		if (!io.WantCaptureKeyboard)
			mKeyboard.PollHeld(window);

		// Scene tick + 멀티 카메라 렌더 (SceneFB  PostFX  backbuffer).
		SJH::Scene::Director::Get().Update(dt);
		mRenderSys.Render();

		// ImGui draws 는 v1.53 의 io.RenderDrawListsFn 콜백을 통해 자동 (현재 backbuffer 에).
		ImGui::Render();
	}

	void shutdown() override
	{
		SJH::Scene::Director::Get().Exit();
		ImGui_ImplGlfwGL3_Shutdown();
		ImGui::DestroyContext(mImGuiCtx);
	}

	// === sb7 입력 콜백  ImGui forward + 게임 디스패치 ===
	void onKey(int key, int action) override
	{
		ImGui_ImplGlfwGL3_KeyCallback(window, key, /*scancode*/ 0, action, /*mods*/ 0);
		// ImGui 가 키보드 캡쳐 중이면 게임 dispatch 차단.
		if (ImGui::GetIO().WantCaptureKeyboard) return;
		mKeyboard.Dispatch(key, action);
	}

	void onMouseButton(int button, int action) override
	{
		ImGui_ImplGlfwGL3_MouseButtonCallback(window, button, action, /*mods*/ 0);
		if (ImGui::GetIO().WantCaptureMouse) return;
		double x = 0.0, y = 0.0;
		glfwGetCursorPos(window, &x, &y);
		mMouse.HandleButton(button, action, x, y);
	}

	void onMouseMove(int x, int y) override
	{
		// ImGui v1.53 은 cursor pos 를 NewFrame 시 직접 glfwGetCursorPos 로 폴링 — forward 불필요.
		if (ImGui::GetIO().WantCaptureMouse) return;
		mMouse.HandleMove(static_cast<double>(x), static_cast<double>(y));
	}

	void onResize(int w, int h) override
	{
		sb7::application::onResize(w, h);
		glViewport(0, 0, w, h);
		SJH::RenderContext::Get().SetDefaultTargetSize(w, h);

		auto &dir = SJH::Scene::Director::Get();
		if (dir.GetActiveCamera())
			dir.GetActiveCamera()->Aspect = static_cast<float>(w) / static_cast<float>(h);

		if (mSceneFB && w > 0 && h > 0)
		{
			auto newFB = SJH::Framebuffer::Create(w, h);
			if (newFB)
			{
				auto *postfxMat = SJH::ResourceRegistry::Get().FindMaterial("mat_postfx");
				if (postfxMat)
					postfxMat->Textures["uScene"] = {newFB->GetColorAttachment().get(), 0};
				for (auto &child : dir.Root().GetChildren())
				{
					if (auto *cam = child->GetComponent<SJH::Scene::Camera>())
					{
						if (cam->GetTargetFramebuffer() == mSceneFB.get())
							cam->SetTargetFramebuffer(newFB.get());
					}
				}
				mSceneFB = std::move(newFB);
			}
		}
	}

  private:
	struct UIState
	{
		bool  dirLightEnabled       = true;
		bool  pointLightsEnabled[2] = {true, true};
		bool  spotLightEnabled      = true;
		bool  flashLight            = false; // 스포트가 카메라 추종 모드.
		float gamma                 = 1.0f;
		float clearColor[3]         = {0.0f, 0.1f, 0.2f};
	};

	/// @brief 좌상단 UI 패널 — 레퍼런스 OpenGL-With-CMake/context.cpp 의 ImGui 패널 회귀.
	void BuildUI()
	{
		if (!ImGui::Begin("migrate_demo controls"))
		{
			ImGui::End();
			return;
		}

		if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("DirLight",      &mUI.dirLightEnabled);
			ImGui::Checkbox("PointLight 0",  &mUI.pointLightsEnabled[0]);
			ImGui::Checkbox("PointLight 1",  &mUI.pointLightsEnabled[1]);
			ImGui::Checkbox("SpotLight",     &mUI.spotLightEnabled);
			ImGui::Separator();
			ImGui::Checkbox("FlashLight mode (F)", &mUI.flashLight);
			if (mRefs.spotLight)
			{
				ImGui::DragFloat("Spot cutoff (deg)",       &mRefs.spotLight->CutoffAngleDeg,      0.1f, 0.0f, 89.0f);
				ImGui::DragFloat("Spot outerCutoff (deg)",  &mRefs.spotLight->OuterCutoffAngleDeg, 0.1f, 0.0f, 90.0f);
				ImGui::DragFloat("Spot distance",           &mRefs.spotLight->Distance,            0.5f, 1.0f, 3250.0f);
				ImGui::ColorEdit3("Spot diffuse",           reinterpret_cast<float*>(&mRefs.spotLight->Diffuse));
			}
		}

		if (ImGui::CollapsingHeader("PostFX"))
		{
			ImGui::SliderFloat("Gamma (blur exp)", &mUI.gamma, 0.1f, 2.5f);
		}

		if (ImGui::CollapsingHeader("Camera"))
		{
			if (mSceneCameraActor)
			{
				auto &t = mSceneCameraActor->GetTransform();
				ImGui::DragFloat3("Position", reinterpret_cast<float*>(&t.Translate), 0.05f);
				ImGui::DragFloat3("EulerRot", reinterpret_cast<float*>(&t.EulerRot),  0.5f);
				if (ImGui::Button("Reset camera"))
				{
					t.Translate = vmath::vec3(0.0f, 2.5f, 8.0f);
					t.EulerRot  = vmath::vec3(-20.0f, 0.0f, 0.0f);
				}
			}
			ImGui::ColorEdit3("Clear color (FB)", mUI.clearColor);
		}

		ImGui::End();
	}

	/// @brief UI 상태  씬에 반영. 매 프레임 ApplyUIState 호출.
	void ApplyUIState()
	{
		// 1) 광원 enable 토글 — Component::SetEnabled 가 RenderSystem::CollectLights 에서 필터.
		if (mRefs.dirLight)        mRefs.dirLight->SetEnabled(mUI.dirLightEnabled);
		if (mRefs.pointLights[0])  mRefs.pointLights[0]->SetEnabled(mUI.pointLightsEnabled[0]);
		if (mRefs.pointLights[1])  mRefs.pointLights[1]->SetEnabled(mUI.pointLightsEnabled[1]);
		if (mRefs.spotLight)       mRefs.spotLight->SetEnabled(mUI.spotLightEnabled);

		// 2) FlashLight 모드 — 스포트 액터가 카메라 transform 추종.
		if (mUI.flashLight && mRefs.spotLightActor && mSceneCameraActor)
		{
			auto &camT  = mSceneCameraActor->GetTransform();
			auto &spotT = mRefs.spotLightActor->GetTransform();
			spotT.Translate = camT.Translate;
			spotT.EulerRot  = camT.EulerRot;
			// Scale 0.1 (마커 크기) 보존.
		}

		// 3) PostFX uGamma 갱신.
		if (auto *postfxMat = SJH::ResourceRegistry::Get().FindMaterial("mat_postfx"))
			SJH::Uniforms::SetFloat(*postfxMat, "uGamma", mUI.gamma);

		// 4) Clear color — RenderContext::BeginFrame 의 clear 가 GL state 의 glClearColor 사용.
		glClearColor(mUI.clearColor[0], mUI.clearColor[1], mUI.clearColor[2], 1.0f);
	}

	SJH::KeyboardInput<MigrateDemo::Controller::CameraController::Action> mKeyboard;
	SJH::MouseInput mMouse;
	SJH::RenderSystem mRenderSys;
	SJH::FramebufferUPtr mSceneFB;
	MigrateDemo::Scene::SceneRefs mRefs;
	SJH::Scene::Actor *mSceneCameraActor = nullptr;
	UIState mUI;
	ImGuiContext *mImGuiCtx = nullptr;
	bool mFKeyPrev = false; // F 키 edge detect (FlashLight 토글).
	double mLastTime = 0.0;
};

DECLARE_MAIN(migrate_demo_app);
