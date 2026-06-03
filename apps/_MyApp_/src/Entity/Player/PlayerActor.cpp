#include <GL/gl3w.h> // 반드시 최상단 — resource_registry.h→framebuffer.h→render_target.h→gl3w.h 보다 먼저.

#include "Entity/Player/PlayerActor.h"
#include "Entity/Player/PlayerEntity.h"

#include "Physics/PhysicsImpulse.h"

#include "resource_registry/resource_registry.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_frame_clip.h"
#include "sprite/sprite_sequence_playable.h"

#include <box2d/box2d.h>
#include <spdlog/spdlog.h>
#include <string>

namespace TopdownShooter::Entity::Player
{
	namespace
	{
		// ════════════ [1] 컴포넌트 초기화 (AddComponent 전용 — Set* 없음) ════════════

		/// @brief Life — 항상.
		void InitLife(SJH::Scene::Actor &a, const PlayerActorConfig &cfg)
		{
			a.AddComponent<Components::Life>(cfg.life.hp);
		}

		/// @brief [if/else 분할] 물리 분기 — BoxBody(eager) + PhysicsMovement + Impulse + Weapon.
		///        world 미주입이면 no-op (분기 guard). Weapon 의 SetWorld 는 [2] WireWeapon 담당.
		void InitPhysicsBranch(SJH::Scene::Actor &a, const PlayerActorConfig &cfg)
		{
			if (cfg.physics.world == nullptr)
				return;

			// Physics body 생성은 BoxBody ctor 가 담당(eager) — b2World 가 lifetime 소유.
			Physics::Components::BodyConfig bc;
			bc.world         = cfg.physics.world;
			bc.startPosition = cfg.physics.startPosition;
			bc.linearDamping = cfg.physics.linearDamping;
			bc.density       = cfg.physics.density;
			bc.friction      = cfg.physics.friction;
			bc.isSensor      = cfg.physics.isSensor;
			bc.categoryBits  = cfg.physics.categoryBits;
			bc.maskBits      = cfg.physics.maskBits;
			a.AddComponent<Physics::Components::BoxBody>(bc, cfg.physics.size);

			a.AddComponent<Physics::PhysicsMovement>(cfg.movement.speed);
			a.AddComponent<Physics::Impulse>(); // Dash/Knockback 속도버스트 (dash 입력 미배선이라 dormant)
			a.AddComponent<Components::Weapon>(cfg.weapon.damage, "default"); // bullet 스폰 box2d 의존 → physics 분기만
		}

		/// @brief [if/else 분할] 비물리 분기 — 기존 Movement(Transform 직접). physics 면 no-op (분기 guard).
		void InitMovementBranch(SJH::Scene::Actor &a, const PlayerActorConfig &cfg)
		{
			if (cfg.physics.world != nullptr)
				return;
			a.AddComponent<Components::Movement>(cfg.movement.speed);
		}

		/// @brief [if 분할] PlayerController 생성만 (keyboard 미주입이면 no-op). 의존 주입은 [2] WireController.
		void InitController(SJH::Scene::Actor &a, const PlayerActorConfig &cfg)
		{
			if (cfg.controller.keyboard == nullptr)
				return;
			a.AddComponent<Controller::PlayerController>();
		}

		/// @brief [if 분할] 4-레이어 스프라이트 합성 (spec §6.3) — direction 지정 시에만.
		/// @details 각 파트 PNG = 자기 UniformAtlas. DrawOrder 별 child Actor + QueueOffset painter 합성.
		void InitSprite(SJH::Scene::Actor &a, const PlayerActorConfig &cfg)
		{
			if (cfg.sprite.direction == nullptr)
				return;

			auto &reg = SJH::ResourceRegistry::Get();
			for (const auto &t : *cfg.sprite.direction)
			{
				// atlas 먼저 (child 생성 전) — key=path. 있으면 재사용(Find), 없으면 생성(Create).
				auto *atlas = reg.FindUniformAtlas(t.TexturePath);
				if (!atlas)
					atlas = reg.CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
				if (!atlas)
				{
					spdlog::error("[4layer] atlas load 실패: {}", t.TexturePath);
					continue; // child 미생성 — 빈 child 를 트리에 남기지 않음
				}

				auto child = std::make_unique<SJH::Scene::Actor>(
				    cfg.name + "_L" + std::to_string(t.DrawOrder));
				SJH::Scene::Actor *childPtr = a.AddChild(std::move(child));

				auto *spr        = childPtr->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
				spr->flipX       = t.Flip;
				spr->QueueOffset = t.DrawOrder; // 2450+DrawOrder → distinct 층

				if (t.ColCount > 1) // 애니 파트 (가로 N프레임 스트립)
				{
					auto *seq = childPtr->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
					    spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, cfg.sprite.fps});
					seq->SetIsLoop(true);
					seq->Play();
				}
			}
		}

		/// @brief facade — 모든 형제 부착 후 (OnEnter 가 형제 캐시).
		void InitFacade(SJH::Scene::Actor &a)
		{
			a.AddComponent<PlayerEntity>();
		}

		// ════════════ [2] 컴포넌트간 의존성 연결 (Set* 전용) ════════════

		/// @brief Weapon → bullet 스폰 물리 월드 주입 (Weapon 있을 때만).
		void WireWeapon(SJH::Scene::Actor &a, const PlayerActorConfig &cfg)
		{
			if (auto *weapon = a.GetComponent<Components::Weapon>())
				weapon->SetWorld(cfg.weapon.world);
		}

		/// @brief Controller ← 입력/카메라/IMovable 타깃 주입 + SetUp (Controller 있을 때만).
		/// @details IMovable 타깃 = 분기 결과 (PhysicsMovement 우선, 없으면 Movement). setter 는 fluent 체인.
		void WireController(SJH::Scene::Actor &a, const PlayerActorConfig &cfg)
		{
			auto *controller = a.GetComponent<Controller::PlayerController>();
			if (controller == nullptr)
				return;

			Entity::IMovable *movable = a.GetComponent<Physics::PhysicsMovement>(); // 물리 분기
			if (movable == nullptr)
				movable = a.GetComponent<Components::Movement>();                    // 비물리 분기

			controller->SetKeyboardInput(cfg.controller.keyboard)
			    .SetMouseInput(cfg.controller.mouse)
			    .SetWorldCamera(cfg.controller.camera)
			    .SetMovableTarget(movable)
			    .SetFireCallback(cfg.controller.onFire)
			    .SetDamageCallback(cfg.controller.onDamage);
			controller->SetUp();
		}
	} // namespace

	std::unique_ptr<SJH::Scene::Actor> CreatePlayerActor(const PlayerActorConfig &cfg)
	{
		auto  actor = std::make_unique<SJH::Scene::Actor>(cfg.name);
		auto &a     = *actor;

		// [1] 컴포넌트 초기화 (if/else → 분기 함수, 각자 내부 guard 로 한쪽만 실효)
		InitLife(a, cfg);
		InitPhysicsBranch(a, cfg);
		InitMovementBranch(a, cfg);
		InitController(a, cfg);
		InitSprite(a, cfg);

		// [2] 컴포넌트간 의존성 연결 (초기화 끝난 뒤 Set*)
		WireWeapon(a, cfg);
		WireController(a, cfg);

		// facade (모든 형제 후 — OnEnter 캐시)
		InitFacade(a);

		return actor;
	}
} // namespace TopdownShooter::Entity::Player
