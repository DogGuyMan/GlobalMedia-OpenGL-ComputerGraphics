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

#include "InputHandler/CameraController.h"
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

			// === Material — Pass::AlphaTest (sprite frag discard) + Properties ===
			auto *mat = reg.CreateSharedMaterial("billboard_player");
			mat->SetProgram(prog);
			mat->SetPass(SJH::Pass::Kind::AlphaTest); // depth ON, blend OFF, frag discard (spec §10.2)

			// Atlas texture (UniformAtlas 가 SJH::Texture 위탁)
			mat->Properties.Textures["uAtlas"] = {mAtlas.GetTexture(), /*unit=*/0};
			// frame 0 uv rect
			SJH::Uniforms::SetVec4(*mat, "uUvRect", mAtlas.GetUVRect(/*frameIdx=*/0));
			SJH::Uniforms::SetFloat(*mat, "uFlipX", 1.0f);
			SJH::Uniforms::SetVec4(*mat, "uTint", vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));

			// === Camera Actor (compound factory) ===
			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
			mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);

			glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

			auto camActor = SJH::Scene::CreateCameraActor("MainCamera", 45.0f, aspect, 0.1f, 100.0f);
			camActor->GetTransform().Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
			camActor->GetTransform().EulerRot = vmath::vec3(-45.0f, 0.0f, 0.0f); // pitch (위에서 내려다봄)
			auto *cam = camActor->GetComponent<SJH::Scene::Camera>();
			camActor->AddComponent<Controller::CameraController>()
			    ->SetKeyboardInput(&mKeyboard)
			    .SetMouseInput(&mMouse)
			    .SetCamera(cam)
			    .SetUp();
			cam->SetTargetFramebuffer(nullptr);

			mCameraActor = dir.Root().AddChild(std::move(camActor));
			mCamera = cam;
			dir.SetActiveCamera(cam);

			// === 6. Sprite Actor + MeshRenderer Component ===
			// Actor 의 Transform.Translate = 빌보드 center, Scale = 빌보드 size (셰이더 uModel 흡수)
			auto spriteActor = std::make_unique<SJH::Scene::Actor>("PlayerSprite");
			spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
			spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);
			spriteActor->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), mat);
			mSpriteActor = dir.Root().AddChild(std::move(spriteActor));

			// === 7. Director lifecycle (Camera + Sprite OnEnter 캐스케이드) ===
			dir.Enter();
		}

		void render(double currentTime) override
		{
			const float dt = static_cast<float>(SJH::DeltaTime(currentTime));

			// Backbuffer resize 안전 — physical framebuffer 기준
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
			spdlog::info("Pressed {} {} {}", 
				mCameraActor->GetTransform().Translate[0],
				mCameraActor->GetTransform().Translate[1],
				mCameraActor->GetTransform().Translate[2]
			);
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
		SJH::Scene::Actor *mCameraActor = nullptr; // 비소유 — Director root child
		SJH::Scene::Actor *mSpriteActor = nullptr; // 비소유
		SJH::Scene::Camera *mCamera = nullptr;     // 비소유 — Camera Component
		SJH::KeyboardInput<Controller::CameraController::Action> mKeyboard;
		SJH::MouseInput mMouse;
	};

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
