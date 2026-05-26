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
#include "Audio/FmodPlayable.h"
#include "Audio/FmodStudioPlayable.h"
#include "Director.h"
#include "Tween/TweenPlayable.h"
#include "VFX/EffekseerPlayable.h"
#include "playable/composite_playable.h"

#include <tweeny/tweeny.h>
#include <cmath>
#include "Stage/StageBuilder.h"
#include "common/common.h"
#include "render/render_target.h"
#include "render/scene_renderer.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_frame_clip.h"
#include "sprite/sprite_sequence_playable.h"

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

			// Atlas — registry 가 LoadFromPNG + SetGrid 일괄. SpriteRenderer ctor 에 주입.
			auto *atlas = reg.CreateUniformAtlas(
				"test_pattern", 
				"resources/texture/TestPattern.png",
				 4, 4);
			if (!atlas)
			{
				spdlog::error("[M1] atlas load failed");
				return;
			}

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

			// === M5 — Director 가 Audio + VFX + Physics 일괄 초기화 ===
			TopdownShooter::Director::Get().Init();
			auto &phys = TopdownShooter::Director::Get().Physics();

			// === M5 T3 — BGM (FMOD Studio) ===
			{
				auto &audio = TopdownShooter::Director::Get().Audio();
				audio.LoadBank("resources/banks/Master.strings.bank");   // strings 먼저 (event 이름 lookup 위해)
				audio.LoadBank("resources/banks/Master.bank");

				auto *bgmEvent = audio.LoadEvent("event:/BGM");
				if (bgmEvent)
				{
					auto *bgmActor = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("BgmActor"));
					auto *bgm = bgmActor->AddComponent<TopdownShooter::Audio::FmodStudioPlayable>(bgmEvent);
					bgm->SetIsLoop(true);
					bgm->Play();
				}

				// === M5 T1 — Shot SFX 로드 (마우스 클릭 시 재생) ===
				reg.CreateSound(audio.GetSystem(), "shot", "resources/audio/Laser.wav");
			}

			// === M5 T2 — Muzzle VFX 로드 (마우스 클릭 시 spawn) ===
			{
				auto mgr = TopdownShooter::Director::Get().VFX().GetManager();
				reg.CreateEffect(mgr, "muzzle", u"resources/vfx/distortion.efk");
			}

			// === Stage 형성 — walls + pickups + StageState Component 가 child/component 인 일반 Actor 반환.
			//     plane mesh / wallMat / pickupMat / simple.vs/fs Program 은 Stage Builder 가 registry 에 자동 등록.
			auto stage = TopdownShooter::Stage::CreateStageActor({
			    /*world=*/    &phys.World(),
			    /*registry=*/ &reg,
			});
			dir.Root().AddChild(std::move(stage));

			pac.name = "PlayerSprite";
			pac.life.hp = 100;
			pac.movement.speed = 3.0f;
			pac.controller.keyboard = &mKeyboard;
			pac.physics.world = &phys.World();
			pac.physics.size = vmath::vec2(1.0f, 1.0f);
			pac.physics.startPosition = vmath::vec2(0.0f, 0.0f);
			pac.physics.density = 1.0f;
			pac.physics.linearDamping = 5.0f;
			pac.physics.categoryBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Player);
			pac.physics.maskBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PlayerMask);

			auto spriteActor = TopdownShooter::Entity::Player::CreatePlayerActor(pac);

			spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
			spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);

			// SpriteRenderer — Unity 정통. MeshRenderer 상속 + billboard plane / material 자동 셋업.
			// per-Update 마다 uUvRect / uTint / uFlipX 도 내부에서 송신 — main render() 무동작.
			mSprite = spriteActor->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);

			// SpriteSequencePlayable — M3.5 정착 (SpriteAnimator 폐기). atlas 전체 16-frame 을
			// 4fps 로 loop sweep. clip 은 main 멤버로 보관 (playable 은 raw ptr 만 보유).
			mWholeAtlasClip = SJH::SpriteSequence::SpriteFrameClip{
			    /*startFrame*/ 0,
			    /*frameCount*/ atlas->FrameCount(),
			    /*fps*/        4.0f};
			mSpriteSeq = spriteActor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
			    mSprite, &mWholeAtlasClip);
			mSpriteSeq->SetIsLoop(true);
			mSpriteSeq->Play();

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
			// M5 — Director::Update 가 Audio + VFX + Physics.Step 일괄.
			TopdownShooter::Director::Get().Update(dt);
			SJH::Scene::Director::Get().Update(dt);

			TopdownShooter::Director::Get().Physics().SyncToTransform(SJH::Scene::Director::Get().Root());

			// SpriteRenderer.Update 가 uUvRect / uTint / uFlipX 자동 송신 — main 무동작.
			mRenderSys.Render(*mDefaultTarget);

			// === M5 — Effekseer 렌더 (SceneRenderer 직후, swap 전) ===
			if (mCamera)
			{
				vmath::mat4 view = mCamera->GetViewMatrix();
				vmath::mat4 proj = mCamera->GetProjectionMatrix();
				TopdownShooter::Director::Get().VFX().Draw(&view[0][0], &proj[0][0]);
			}
		}

		void shutdown() override
		{
			SJH::Scene::Director::Get().Exit();
			mCamera = nullptr;
			mCameraActor = nullptr;
			mSpriteActor = nullptr;
			mDefaultTarget.reset();
			mSprite = nullptr;     // 컴포넌트는 spriteActor 가 소유 — Director::Exit 가 정리. atlas 는 ResourceRegistry::Clear 가 담당
			mSpriteSeq = nullptr;  // 동일 — Director::Exit 가 spriteActor 정리 시 함께 소멸
			// M5 — Director 가 Physics + VFX + Audio 일괄 정리
			TopdownShooter::Director::Get().Shutdown();
		}

		void onKey(int key, int action) override
		{
			mKeyboard.Dispatch(key, action);

			// === M5 CO2 — G 키: Parallel( TweenShake ∥ FmodStudio.Damaged ) ===
			if (key == GLFW_KEY_G && action == GLFW_PRESS)
			{
				auto &audio = TopdownShooter::Director::Get().Audio();
				auto *damagedEvt = audio.LoadEvent("event:/Damaged");

				auto *dActor = SJH::Scene::Director::Get().Root().AddChild(
				    std::make_unique<SJH::Scene::Actor>("DamageComposite"));
				auto *par = dActor->AddComponent<SJH::Playable::ParallelPlayable>();

				auto tween = tweeny::from(0.0f).to(1.0f).during(100).via(tweeny::easing::sinusoidalInOut);
				par->Join(std::make_unique<TopdownShooter::Tween::TweenPlayable<float>>(
				    std::move(tween),
				    [](float v) {
					    float offset = std::sin(v * 8.0f * 3.14159f) * 5.0f;
					    spdlog::info("[shake] v={:.3f} offset={:.3f}", v, offset);
				    }));
				if (damagedEvt)
					par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(damagedEvt));

				par->Play();
			}
		}

		void onMouseButton(int button, int action) override
		{
			double x = 0.0, y = 0.0;
			glfwGetCursorPos(window, &x, &y);
			mMouse.HandleButton(button, action, x, y);

			// === M5 CO1 — 마우스 좌클릭: Sequence( Effekseer.distortion  Parallel( Fmod.Laser ∥ FmodStudio.Slash ) ) ===
			if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
			{
				auto &reg   = SJH::ResourceRegistry::Get();
				auto &audio = TopdownShooter::Director::Get().Audio();
				auto &vfx   = TopdownShooter::Director::Get().VFX();

				auto *shot     = reg.FindSound("shot");
				auto *muzzle   = reg.FindEffect("muzzle");
				auto *slashEvt = audio.LoadEvent("event:/Slash");

				if (shot && muzzle && slashEvt)
				{
					auto *cActor = SJH::Scene::Director::Get().Root().AddChild(
					    std::make_unique<SJH::Scene::Actor>("ShotComposite"));
					auto *seq = cActor->AddComponent<SJH::Playable::SequencePlayable>();

					// 1: muzzle VFX (자연 종료 대기)
					seq->Append(std::make_unique<TopdownShooter::VFX::EffekseerPlayable>(
					    vfx.GetManager(), muzzle, vmath::vec3(0.0f),
					    TopdownShooter::VFX::TrackPolicy::Static));

					// 2: Parallel( Laser.wav ∥ Slash event )
					auto par = std::make_unique<SJH::Playable::ParallelPlayable>();
					par->Join(std::make_unique<TopdownShooter::Audio::FmodPlayable>(audio.GetSystem(), shot));
					par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(slashEvt));
					seq->Append(std::move(par));

					seq->Play();
				}
			}
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
		SJH::SceneRenderer mRenderSys;
		SJH::RenderTargetUPtr mDefaultTarget;
		SJH::Scene::Actor *mCameraActor = nullptr;
		SJH::Scene::Actor *mSpriteActor = nullptr;
		SJH::Scene::Camera *mCamera = nullptr;
		SJH::SpriteSequence::SpriteSequencePlayable *mSpriteSeq = nullptr;   // Update 마다 mSprite->frameIdx 송신 (M3.5 — SpriteAnimator 후속)
		SJH::SpriteSequence::SpriteFrameClip          mWholeAtlasClip{};    // atlas 전체 sweep 정의 (mSpriteSeq 가 raw ptr 로 참조 — 멤버 생존 필수)
		SJH::Sprite::SpriteRenderer *mSprite = nullptr;     // sprite 데이터 (atlas + frameIdx + tint 등). spriteActor 소유
		SJH::KeyboardInput<Controller::PlayerController::Action> mKeyboard;
		SJH::MouseInput mMouse;
		TopdownShooter::Entity::Player::PlayerActorConfig pac;
		// M5 — mPhysics 폐기. TopdownShooter::Director::Get().Physics() 가 그 자리.
	};

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
