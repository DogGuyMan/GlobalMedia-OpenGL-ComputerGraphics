/**
 * @file main.cpp
 * @brief migrate_demo — OpenGL-With-CMake/src/context/context.cpp 를 SJH Actor/Component
 *        그래프 + RenderSystem 으로 마이그레이트.
 *
 * @details
 *  ### 씬 (레퍼런스 회귀)
 *  - Plane / Box1 / Box2 — Phong 텍스쳐 머티리얼 (sampler2D material.diffuse/specular)
 *  - Outline (Box2 자식, scale 1.05) — simple yellow 셸
 *  - Window × 3 — alpha discard 투명 텍스쳐
 *  - DirLight + PointLight × 2 + SpotLight (각 점/스포트 광원 위치에 마커 큐브)
 *  - Camera Actor (Compound) + CameraController — WASD/QE + 우클릭 드래그
 *
 *  ### 멀티 패스 (PostFX)
 *  - SceneCamera (depth=0, target=SceneFB, mask=LAYER_SCENE) — 본체 씬을 FBO 에 렌더.
 *  - PostFXCamera (depth=1, target=backbuffer, mask=LAYER_POSTFX) — 화면 quad 에
 *    `uScene` sampler 로 SceneFB color 를 읽어 7-tap blur 출력.
 *
 *  ### 조작
 *  - WASD : 평면 이동, E/Q : 상승/하강
 *  - 우클릭 드래그 : 시점 회전
 *
 *  ### 의도된 단순화
 *  - Stencil outline 없음 — RenderSystem 이 per-actor GL 상태 미지원이라 shell scale 트릭으로 근사.
 *  - ImGui / FlashLight 토글 / depth func combo / 광원 enable 체크박스 없음.
 */

// sb7.h 가 GLFW + gl3w + glcorearb 의 macro/typedef 순서를 잡아주므로 반드시 *맨 앞*.
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <sb7.h>

// stb_image 정의 책임은 SJH::resource_registry 가 이미 보유 (image.cpp).

#include "buffer/framebuffer.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "material/material_uniforms.h"
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
		// macOS GLFW _GLFW_USE_CHDIR fix — 셰이더 상대경로 로딩.
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

		// === 1) Programs (4종) ===
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

		// === 2) Viewport seed ===
		int fbW = 0, fbH = 0;
		glfwGetFramebufferSize(window, &fbW, &fbH);
		onResize(fbW, fbH);

		// === 3) Asset 워밍업 (Image/Texture/Mesh) ===
		MigrateDemo::Scene::WarmupAssets(reg);

		// === 4) SceneFB — 본체 패스의 오프스크린 타겟 ===
		mSceneFB = SJH::Framebuffer::Create(fbW, fbH);
		if (!mSceneFB)
		{
			spdlog::error("SceneFB 생성 실패");
			std::exit(1);
		}

		// === 5) PostFX 머티리얼 — uScene sampler 로 SceneFB color 읽음 ===
		auto *postfxMat = reg.CreateMaterial("mat_postfx");
		postfxMat->SetProgram(postfxProg);
		postfxMat->Textures["uScene"] = {mSceneFB->GetColorAttachment().get(), /*unit*/ 0};
		SJH::Uniforms::SetFloat(*postfxMat, "uGamma", 1.0f);

		// === 6) 씬 워밍업 (Plane / Box / Outline / Windows / Lights) ===
		auto &dir = SJH::Scene::Director::Get();
		MigrateDemo::Scene::WarmupActors(progs, dir);
		MigrateDemo::Scene::WarmupLights(progs, dir);

		// === 7) ScreenQuad Actor (PostFX 카메라가 backbuffer 에 그림) ===
		{
			auto quad = std::make_unique<SJH::Scene::Actor>("PostFXQuad");
			quad->SetLayer(MigrateDemo::Scene::LAYER_POSTFX);
			quad->AddComponent<SJH::Scene::MeshRenderer>(
			    reg.FindMesh("mesh_screen_quad"), postfxMat,
			    MigrateDemo::Scene::QUEUE_OPAQUE);
			dir.Root().AddChild(std::move(quad));
		}

		// === 8) SceneCamera (depth=0, target=SceneFB) — 사용자 시점 ===
		const float aspect = static_cast<float>(info.windowWidth) /
		                     static_cast<float>(info.windowHeight);
		auto sceneCamActor = SJH::Scene::CreateCameraActor("SceneCamera",
		                                                    45.0f, aspect, 0.1f, 100.0f);
		sceneCamActor->SetLayer(MigrateDemo::Scene::LAYER_SCENE);
		// 레퍼런스 카메라 위치 (translate {0, 2.5, 8}, rot {-20, 0, 0}).
		sceneCamActor->GetTransform().Translate = vmath::vec3(0.0f, 2.5f, 8.0f);
		sceneCamActor->GetTransform().EulerRot  = vmath::vec3(-20.0f, 0.0f, 0.0f);

		auto *sceneCam = sceneCamActor->GetComponent<SJH::Scene::Camera>();
		sceneCam->Depth       = 0;
		sceneCam->CullingMask = MigrateDemo::Scene::LAYER_SCENE;
		sceneCam->SetTargetFramebuffer(mSceneFB.get());

		// CameraController — SceneCamera 액터의 transform 갱신.
		auto *ctrl = sceneCamActor->AddComponent<MigrateDemo::Controller::CameraController>();
		ctrl->SetKeyboardInput(&mKeyboard)
		    .SetMouseInput(&mMouse)
		    .SetCamera(sceneCam)
		    .SetUp();

		dir.Root().AddChild(std::move(sceneCamActor));

		// === 9) PostFXCamera (depth=1, target=backbuffer) — 화면 quad 만 그림 ===
		{
			auto fxCamActor = std::make_unique<SJH::Scene::Actor>("PostFXCamera");
			fxCamActor->SetLayer(MigrateDemo::Scene::LAYER_POSTFX);
			auto *fxCam = fxCamActor->AddComponent<SJH::Scene::Camera>(
			    45.0f, aspect, 0.1f, 100.0f);
			fxCam->Depth       = 1;
			fxCam->CullingMask = MigrateDemo::Scene::LAYER_POSTFX;
			// SetTargetFramebuffer 미호출 → nullptr = default backbuffer.
			dir.Root().AddChild(std::move(fxCamActor));
		}

		// === 10) 활성 Camera 지정 + Scene Enter ===
		dir.SetActiveCamera(sceneCam);
		dir.Enter();
		mLastTime = 0.0;

		spdlog::info("migrate_demo startup 완료. WASD/QE + 우클릭 드래그.");
	}

	void render(double currentTime) override
	{
		const float dt = static_cast<float>(currentTime - mLastTime);
		mLastTime = currentTime;

		SJH::RenderContext::Get().SetDefaultTargetSize(info.windowWidth, info.windowHeight);

		// 키보드 held 핸들러 폴링 — 매 프레임 mMoveDelta 누적.
		mKeyboard.PollHeld(window);

		// Scene tick + 렌더 (SceneCamera → SceneFB, PostFXCamera → backbuffer).
		SJH::Scene::Director::Get().Update(dt);
		mRenderSys.Render();
	}

	void shutdown() override
	{
		SJH::Scene::Director::Get().Exit();
	}

	// === sb7 입력 콜백 위임 ===
	void onKey(int key, int action) override
	{
		mKeyboard.Dispatch(key, action);
	}

	void onMouseButton(int button, int action) override
	{
		double x = 0.0, y = 0.0;
		glfwGetCursorPos(window, &x, &y);
		mMouse.HandleButton(button, action, x, y);
	}

	void onMouseMove(int x, int y) override
	{
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

		// SceneFB 도 새 크기로 재생성 (이미 startup 후라면).
		if (mSceneFB && w > 0 && h > 0)
		{
			auto newFB = SJH::Framebuffer::Create(w, h);
			if (newFB)
			{
				auto *postfxMat = SJH::ResourceRegistry::Get().FindMaterial("mat_postfx");
				if (postfxMat)
				{
					postfxMat->Textures["uScene"] = {newFB->GetColorAttachment().get(), 0};
				}
				// SceneFB 를 가리키던 모든 카메라 target 도 교체.
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
	SJH::KeyboardInput<MigrateDemo::Controller::CameraController::Action> mKeyboard;
	SJH::MouseInput mMouse;
	SJH::RenderSystem mRenderSys;
	SJH::FramebufferUPtr mSceneFB;
	double mLastTime = 0.0;
};

DECLARE_MAIN(migrate_demo_app);
