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

#include "InputHandler/PlayerController.h"
#include "InputHandler/TargetFollowableCameraController.h"
#include "common/common.h"
#include "material/material.h"
#include "material/material_uniforms.h"
#include "object/mesh.h"
#include "render/mesh_renderer.h"
#include "render/render_target.h"
#include "render/scene_renderer.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"
#include "sprite/uniform_atlas.h"

#include <cstring>
#include <memory>

namespace TopdownShooter
{

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
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();

			if (!mAtlas.LoadFromPNG("resources/texture/TestPattern.png", /*tilePx=*/128))
			{
				spdlog::error("[M1] atlas load failed");
				return;
			}

			auto *prog = reg.CreateProgram(
			    "billboard_atlas",
			    "resources/shaders/billboard_atlas.vs",
			    "resources/shaders/billboard_atlas.fs");
			if (!prog)
			{
				spdlog::error("[M1] program create failed");
				return;
			}

			mPlane = SJH::Mesh::CreatePlane();
			if (!mPlane)
			{
				spdlog::error("[M1] mesh create failed");
				return;
			}

			auto *mat = reg.CreateSharedMaterial("billboard_player");
			mat->SetProgram(prog);
			mat->SetPass(SJH::Pass::Kind::AlphaTest);

			mat->Properties.Textures["uAtlas"] = {mAtlas.GetTexture(), /*unit=*/0};

			SJH::Uniforms::SetVec4(*mat, "uUvRect", mAtlas.GetUVRect(/*frameIdx=*/0));
			SJH::Uniforms::SetFloat(*mat, "uFlipX", 1.0f);
			SJH::Uniforms::SetVec4(*mat, "uTint", vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));

			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
			mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);

			glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

			auto camActor                      = SJH::Scene::CreateCameraActor("MainCamera", 45.0f, aspect, 0.1f, 100.0f);
			camActor->GetTransform().Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
			camActor->GetTransform().EulerRot  = vmath::vec3(-45.0f, 0.0f, 0.0f);
			auto *cam                          = camActor->GetComponent<SJH::Scene::Camera>();
			// Camera Actor 의 follow 컨트롤러는 sprite 셋업 *후* SetFollowTarget 호출이 필요 — 변수 보관.
			auto *camCtrl = camActor->AddComponent<Controller::TargetFollowableCameraController>();
			camCtrl->SetMouseInput(&mMouse)
			    .SetCamera(cam)
			    .SetUp();
			cam->SetTargetFramebuffer(nullptr);

			mCameraActor = dir.Root().AddChild(std::move(camActor));
			mCamera      = cam;
			dir.SetActiveCamera(cam);

			auto spriteActor                      = std::make_unique<SJH::Scene::Actor>("PlayerSprite");
			spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
			spriteActor->GetTransform().Scale     = vmath::vec3(1.0f, 1.0f, 1.0f);
			spriteActor->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), mat);
			spriteActor->AddComponent<Controller::PlayerController>()
			    ->SetKeyboardInput(&mKeyboard)
			    .SetMoveSpeed(0.05f)
			    .SetUp();
			mSpriteActor = dir.Root().AddChild(std::move(spriteActor));

			// Camera follow target — sprite Actor 가 root 의 child 로 등록된 후.
			camCtrl->SetFollowTarget(mSpriteActor)
			    .SetFollowOffset(vmath::vec3(0.0f, 5.0f, 5.0f));

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
			}

			mKeyboard.PollHeld(window);
			SJH::Scene::Director::Get().Update(dt);
			mRenderSys.Render(*mDefaultTarget);
		}

		void shutdown() override
		{
			SJH::Scene::Director::Get().SetActiveCamera(nullptr);
			SJH::Scene::Director::Get().Exit();
			mCamera = nullptr;
			mCameraActor = nullptr;
			mSpriteActor = nullptr;
			mPlane.reset();
			mDefaultTarget.reset();
			mAtlas.Release();
		}

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
		}

	  private:
		SJH::Sprite::UniformAtlas mAtlas;
		SJH::MeshUPtr mPlane;
		SJH::SceneRenderer mRenderSys;
		SJH::RenderTargetUPtr mDefaultTarget;
		SJH::Scene::Actor *mCameraActor = nullptr;
		SJH::Scene::Actor *mSpriteActor = nullptr;
		SJH::Scene::Camera *mCamera = nullptr;
		SJH::KeyboardInput<Controller::PlayerController::Action> mKeyboard;
		SJH::MouseInput mMouse;
	};

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
