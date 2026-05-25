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

#include "Entity/Player/PlayerActor.h"
#include "InputHandler/PlayerController.h"
#include "InputHandler/TargetFollowableCameraController.h"
#include "Physics/filter.h"
#include "Physics/physics_system.h"
#include "Physics/pickup_factory.h"
#include "Physics/wall_factory.h"
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
#include "sprite/sprite_animator.h"
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

			// Fluent Builder — PNG 로드 + grid 명시 분리. SetGrid(cols, rows) 또는 SetTileSize(px) 택일.
			mAtlas.LoadFromPNG("resources/texture/TestPattern.png")
				.SetGrid(2, 2);
			if (!mAtlas.IsValid())
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
			mAtlasMaterial = mat;

			mat->Properties.Textures["uAtlas"] = {mAtlas.GetTexture(), /*unit=*/0};

			SJH::Uniforms::SetVec4(*mat, "uUvRect", mAtlas.GetUVRect(/*frameIdx=*/0));
			SJH::Uniforms::SetFloat(*mat, "uFlipX", 1.0f);
			SJH::Uniforms::SetVec4(*mat, "uTint", vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));

			int fbW = 0, fbH = 0;
			glfwGetFramebufferSize(window, &fbW, &fbH);
			const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
			mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);

			glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

			auto camActor = SJH::Scene::CreateCameraActor("MainCamera", 45.0f, aspect, 0.1f, 100.0f);
			camActor->GetTransform().Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
			camActor->GetTransform().EulerRot = vmath::vec3(-45.0f, 0.0f, 0.0f);
			auto *cam = camActor->GetComponent<SJH::Scene::Camera>();
			// Camera Actor 의 follow 컨트롤러는 sprite 셋업 *후* SetFollowTarget 호출이 필요 — 변수 보관.
			auto *camCtrl = camActor->AddComponent<Controller::TargetFollowableCameraController>();
			camCtrl->SetMouseInput(&mMouse)
			    .SetCamera(cam)
			    .SetUp();
			cam->SetTargetRenderTarget(nullptr);

			mCameraActor = dir.Root().AddChild(std::move(camActor));
			mCamera = cam;
			dir.SetActiveCamera(cam);

			// === Physics 초기화 ===
			mPhysics.Init();

			// === wall/pickup 시각화 material — simple.vs/fs (MVP + baseColor) 공유 ===
			auto *solidProg = reg.CreateProgram(
			    "solid_plane",
			    "resources/shaders/simple.vs",
			    "resources/shaders/simple.fs");
			auto *wallMat = reg.CreateSharedMaterial("solid_wall");
			wallMat->SetProgram(solidProg);
			wallMat->SetPass(SJH::Pass::Kind::Opaque);
			SJH::Uniforms::SetVec4(*wallMat, "baseColor", vmath::vec4(0.55f, 0.55f, 0.60f, 1.0f));

			auto *pickupMat = reg.CreateSharedMaterial("solid_pickup");
			pickupMat->SetProgram(solidProg);
			pickupMat->SetPass(SJH::Pass::Kind::Opaque);
			SJH::Uniforms::SetVec4(*pickupMat, "baseColor", vmath::vec4(1.0f, 0.85f, 0.2f, 1.0f));

			// 벽 4개 — 약 10×10 단위 arena. Mesh::CreatePlane 은 XZ 평면 1×1 → Scale 로 half×2 매칭.
			const float arena = 10.0f;
			const float wallH = 0.5f;
			auto spawnWall = [&](const char *name, vmath::vec2 center, vmath::vec2 half) {
				auto a = TopdownShooter::Physics::CreateWallActor(name, mPhysics.World(), center, half);
				a->GetTransform().Scale = vmath::vec3(half[0] * 2.0f, 1.0f, half[1] * 2.0f);
				a->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), wallMat);
				dir.Root().AddChild(std::move(a));
			};
			spawnWall("WallTop",    vmath::vec2(0.0f,   +arena), vmath::vec2(arena, wallH));
			spawnWall("WallBottom", vmath::vec2(0.0f,   -arena), vmath::vec2(arena, wallH));
			spawnWall("WallLeft",   vmath::vec2(-arena, 0.0f),   vmath::vec2(wallH, arena));
			spawnWall("WallRight",  vmath::vec2(+arena, 0.0f),   vmath::vec2(wallH, arena));

			// Pickup Sensor — (0, +3) 위치. Player 가 W 키로 진입 시 OnTriggerEnter 로그 검증.
			{
				auto p = TopdownShooter::Physics::CreatePickupActor(
				    "PickupTest", mPhysics.World(), vmath::vec2(0.0f, 3.0f), vmath::vec2(0.8f, 0.8f));
				p->GetTransform().Scale = vmath::vec3(1.6f, 1.0f, 1.6f);
				p->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), pickupMat);
				dir.Root().AddChild(std::move(p));
			}

			pac.name = "PlayerSprite";
			pac.life.hp = 100;
			pac.movement.speed = 3.0f;
			pac.controller.keyboard = &mKeyboard;
			pac.physics.world         = &mPhysics.World();
			pac.physics.size          = vmath::vec2(1.0f, 1.0f);
			pac.physics.startPosition = vmath::vec2(0.0f, 0.0f);
			pac.physics.density       = 1.0f;
			pac.physics.linearDamping = 5.0f;
			pac.physics.categoryBits  = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Player);
			pac.physics.maskBits      = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PlayerMask);

			auto spriteActor = TopdownShooter::Entity::Player::CreatePlayerActor(pac);

			spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
			spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);
			spriteActor->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), mat);

			// SpriteAnimator — atlas FrameCount 만큼 fps default (4×4 = 16fps, 2×2 = 4fps).
			// 매 frame uUvRect 갱신은 render() 안에서.
			mAnimator = spriteActor->AddComponent<SJH::Sprite::SpriteAnimator>();
			mAnimator->SetFps(4.0f);
			mAnimator->SetAtlas(&mAtlas);

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

			mPhysics.Step(dt);
			mPhysics.SyncToTransform(SJH::Scene::Director::Get().Root());

			// Animator 가 Update 단계에서 frameIdx 갱신 완료 → Material 의 uUvRect 송신.
			// (Render 전 단계라 그 프레임에 즉시 반영.)
			if (mAnimator && mAtlasMaterial)
			{
				SJH::Uniforms::SetVec4(*mAtlasMaterial, "uUvRect", mAtlas.GetUVRect(mAnimator->GetCurrentFrame()));
			}

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
			mPhysics.Shutdown();
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
		SJH::Sprite::SpriteAnimator *mAnimator = nullptr;   // render() 매 frame uUvRect 갱신용
		SJH::Material *mAtlasMaterial = nullptr;            // 매 frame uUvRect 갱신 대상
		SJH::KeyboardInput<Controller::PlayerController::Action> mKeyboard;
		SJH::MouseInput mMouse;
		TopdownShooter::Entity::Player::PlayerActorConfig pac;
		TopdownShooter::Physics::PhysicsSystem mPhysics;
	};

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
