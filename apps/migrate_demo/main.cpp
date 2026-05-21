/**
 * @file main.cpp
 * @brief migrate_demo — SP5 (Light) + SP6 (Material) + SP7 (Camera/Input) 통합 검증 챕터.
 *
 * @details
 *  ### 시나리오
 *  - 박스 Actor 1개 (회전 없이 원점)
 *  - DirLight Actor (방향 = (-0.2, -1.0, -0.3) — LearnOpenGL 정통 태양)
 *  - PointLight Actor (위치 = (3, 2, 3))
 *  - Camera Actor (Compound) + CameraController — WASD/마우스 1인칭 시점
 *
 *  ### 조작
 *  - WASD : 평면 이동, E/Q : 상승/하강
 *  - 우클릭 드래그 : 시점 회전
 *
 *  ### Compound Actor 컨벤션 검증
 *  - `Scene::CreateCameraActor / CreateDirLightActor / CreatePointLightActor` 사용.
 *  - Actor 비상속 — 특수 속성은 Component 부착으로만.
 */

// sb7.h 가 GLFW + gl3w + glcorearb 의 macro/typedef 순서를 잡아주므로 반드시 *맨 앞*.
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <sb7.h>


#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "render/render_context.h"
#include "render/render_system.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
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

		// 1) Program — Phong 색상 기반 셰이더.
		auto *prog = reg.CreateProgram("phong_color",
		                               "resources/shaders/phong_color.vs",
		                               "resources/shaders/phong_color.fs");
		if (!prog)
		{
			spdlog::error("phong_color shader 로드 실패");
			std::exit(1);
		}

		int fbW= 0, fbH = 0;
		glfwGetFramebufferSize(window, &fbW, &fbH);
		onResize(fbW, fbH);
		
		auto &dir = SJH::Scene::Director::Get();
		
		const float aspect = static_cast<float>(info.windowWidth) /
		                     static_cast<float>(info.windowHeight);
		auto camActor = SJH::Scene::CreateCameraActor("MainCam", 45.0f, aspect, 0.1f, 100.0f);
		camActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 5.0f);

		auto *cam = camActor->GetComponent<SJH::Scene::Camera>();
		auto *ctrl = camActor->AddComponent<MigrateDemo::Controller::CameraController>();
		ctrl->SetKeyboardInput(&mKeyboard)
		    .SetMouseInput(&mMouse)
		    .SetCamera(cam)
		    .SetUp();

		dir.Root().AddChild(std::move(camActor));

		// 8) 활성 Camera 지정 + Scene Enter.
		dir.SetActiveCamera(cam);
		dir.Enter();
		mLastTime = 0.0;

		MigrateDemo::Scene::WarmupActors(prog, dir);
		MigrateDemo::Scene::WarmupLights(prog, dir);
	}

	void render(double currentTime) override
	{
		const float dt = static_cast<float>(currentTime - mLastTime);
		mLastTime = currentTime;

		SJH::RenderContext::Get().SetDefaultTargetSize(info.windowWidth, info.windowHeight);

		// 키보드 held 핸들러 폴링 — 매 프레임 mMoveDelta 누적.
		mKeyboard.PollHeld(window);

		// Scene tick + 렌더.
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
		// Keyboard 의 press/release 핸들러로 위임 (held 는 PollHeld 가 매 프레임).
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
	}

  private:
	SJH::KeyboardInput<MigrateDemo::Controller::CameraController::Action> mKeyboard;
	SJH::MouseInput mMouse;
	SJH::RenderSystem mRenderSys;
	double mLastTime = 0.0;
};

DECLARE_MAIN(migrate_demo_app);
