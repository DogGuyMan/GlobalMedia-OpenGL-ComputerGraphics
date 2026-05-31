#include <GL/gl3w.h> // 반드시 최상단 — Manager.h→VFXSystem.h→EffekseerRendererGL.h(시스템 gl3.h)보다 먼저.

#include "Bootstrap/PlayerBuilder.h"

#include "Audio/AudioSystem.h"
#include "Audio/FmodPlayable.h"
#include "Audio/FmodStudioPlayable.h"
#include "Entity/Player/PlayerActor.h"
#include "InputHandler/ActorFolower.h"
#include "Manager.h"
#include "Physics/filter.h"
#include "Tween/TweenPlayable.h"
#include "VFX/EffekseerPlayable.h"
#include "playable/composite_playable.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/scene.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_frame_clip.h"
#include "sprite/sprite_sequence_playable.h"

#include <spdlog/spdlog.h>
#include <tweeny/tweeny.h>
#include <cmath>
#include <memory>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Bootstrap
{
	PlayerResult BuildPlayer(const PlayerDeps &deps)
	{
		auto &reg = SJH::ResourceRegistry::Get();
		auto &dir = SJH::Scene::Director::Get();

		PlayerResult result;

		TopdownShooter::Entity::Player::PlayerActorConfig pac;
		pac.name = "PlayerSprite";
		pac.life.hp = 100;
		pac.movement.speed = 3.0f;
		pac.controller.keyboard = deps.keyboard;
		pac.controller.mouse    = deps.mouse;
		pac.controller.camera   = deps.worldCamera; // 좌클릭 마우스→Ground raycast 용 (World 카메라)
		// 좌클릭 — M5 CO1: Sequence( Effekseer.distortion → Parallel( Fmod.Laser ∥ FmodStudio.Slash ) ).
		// 의존은 전부 싱글턴이라 캡처 없는 자기완결 람다 (PlayerController 는 audio/vfx 를 모름).
		pac.controller.onFire = [] {
			auto &reg = SJH::ResourceRegistry::Get();
			auto &audio = TopdownShooter::Manager::Get().Audio();
			auto &vfx = TopdownShooter::Manager::Get().VFX();

			auto *shot = reg.FindSound("shot");
			auto *muzzle = reg.FindEffect("muzzle");
			auto *slashEvt = audio.LoadEvent("event:/Slash");

			if (shot && muzzle && slashEvt)
			{
				auto *cActor = SJH::Scene::Director::Get().Root().AddChild(
				    std::make_unique<SJH::Scene::Actor>("ShotComposite"));
				auto *seq = cActor->AddComponent<SJH::Playable::SequencePlayable>();

				seq->Append(std::make_unique<TopdownShooter::VFX::EffekseerPlayable>(
				    vfx.GetManager(), muzzle, vmath::vec3(0.0f),
				    TopdownShooter::VFX::TrackPolicy::Static));

				auto par = std::make_unique<SJH::Playable::ParallelPlayable>();
				par->Join(std::make_unique<TopdownShooter::Audio::FmodPlayable>(audio.GetSystem(), shot));
				par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(slashEvt));
				seq->Append(std::move(par));

				seq->Play();
			}
		};
		// G키 — M5 CO2: Parallel( TweenShake ∥ FmodStudio.Damaged ).
		pac.controller.onDamage = [] {
			auto &audio = TopdownShooter::Manager::Get().Audio();
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
		};
		pac.physics.world = deps.physicsWorld;
		pac.physics.size = vmath::vec2(1.0f, 1.0f);
		pac.physics.startPosition = vmath::vec2(0.0f, 0.0f);
		pac.physics.density = 1.0f;
		pac.physics.linearDamping = 5.0f;
		pac.physics.categoryBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Player);
		pac.physics.maskBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PlayerMask);

		auto spriteActor = TopdownShooter::Entity::Player::CreatePlayerActor(pac);
		spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
		spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);

		// Atlas — registry 가 LoadFromPNG + SetGrid 일괄.
		auto *atlas = reg.CreateUniformAtlas(
		    "test_pattern",
		    "resources/texture/TestPattern.png",
		    4, 4);
		if (!atlas)
		{
			spdlog::error("[M1] atlas load failed");
			return result; // SpriteActor=nullptr — caller early-return 보존.
		}

		result.Sprite = spriteActor->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);

		// clip 은 caller-owned 저장소에 기록 — SpriteSequencePlayable 가 포인터로만 보유 (lifetime 함정).
		*deps.clipStorage = SJH::SpriteSequence::SpriteFrameClip{0, atlas->FrameCount(), 4.0f};
		result.SpriteSeq = spriteActor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
		    result.Sprite, deps.clipStorage);
		result.SpriteSeq->SetIsLoop(true);
		result.SpriteSeq->Play();

		result.SpriteActor = dir.Root().AddChild(std::move(spriteActor));
		deps.worldCamera
		    ->GetOwner()
		    ->GetComponent<Controller::ActorFolower>()
		    ->SetFollowTarget(result.SpriteActor);

		return result;
	}
}
