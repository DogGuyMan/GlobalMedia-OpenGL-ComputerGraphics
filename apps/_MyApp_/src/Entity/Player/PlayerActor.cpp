#include <GL/gl3w.h> // 반드시 최상단 — resource_registry.h→framebuffer.h→render_target.h→gl3w.h 보다 먼저.

#include "Entity/Player/PlayerActor.h"
#include "Entity/Player/PlayerEntity.h"

#include "Entity/Components/PlayerLifeComponents.h" // 플레이어 전용 Life (i-frame 0.8s + 자동 회복)

#include "Physics/Constants.h"
#include "Physics/PhysicsImpulse.h"
#include "Playable/SpriteLayerFactory.h" // AttachSpriteLayer — 3-빌더 공유 sprite-layer 부착 헬퍼

#include "resource_registry/resource_registry.h"

#include <box2d/box2d.h>
#include <string>

namespace TopdownShooter::Entity::Player
{
	namespace
	{
		// ════════════ [1] 컴포넌트 초기화 (AddComponent 전용 — Set* 없음) ════════════

		/// @brief Life — 항상. 플레이어는 전용 PlayerLifeComponent (i-frame 0.8s + 초당 5 자동 회복).
		///        GetComponent<Components::Life> 는 slow-path dynamic_cast 로 본 파생을 그대로 찾는다.
		void InitLife(SJH::Scene::Actor &a, const PlayerActorConfig &cfg)
		{
			a.AddComponent<Components::PlayerLifeComponent>(cfg.life.hp);
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
			a.AddComponent<Physics::Impulse>(TopdownShooter::Physics::IMPULSE_PLAYER_FORCE); // Dash/Knockback 속도버스트 (dash 입력 미배선이라 dormant)
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
				// child(DrawOrder 네이밍) 생성 후 공유 헬퍼로 atlas+SpriteRenderer(+애니) 부착.
				auto child = std::make_unique<SJH::Scene::Actor>(
				    cfg.name + "_L" + std::to_string(t.DrawOrder));
				SJH::Scene::Actor *childPtr = a.AddChild(std::move(child));
				auto *spr = TopdownShooter::Playable::AttachSpriteLayer(*childPtr, reg, t, cfg.sprite.fps);
				if (!spr)
					continue; // atlas 실패 — PlayerBuilder 8그룹과 동일 에러 처리(빈 child 무해, 렌더 0)
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

			// facade verb(PlayerEntity::DoForward) 경유 — 이동에 묶인 부수효과(dust FX)가 함께 발화.
			// PlayerEntity 가 내부에서 PhysicsMovement/Movement 로 위임. facade 미부착 시 직접 구현체로 fallback.
			Entity::IMovable *movable = a.GetComponent<PlayerEntity>();
			if (movable == nullptr)
			{
				movable = a.GetComponent<Physics::PhysicsMovement>(); // 물리 분기
				if (movable == nullptr)
					movable = a.GetComponent<Components::Movement>(); // 비물리 분기
			}

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

		// facade 먼저 — WireController 가 PlayerEntity(facade verb)를 IMovable 타겟으로 주입하므로
		// (PlayerEntity::DoForward 경유 시 dust FX 등 이동 부수효과 발화). OnEnter 캐시는 씬 진입 시이므로 부착 순서 무관.
		InitFacade(a);

		// [2] 컴포넌트간 의존성 연결 (초기화 끝난 뒤 Set*)
		WireWeapon(a, cfg);
		WireController(a, cfg);

		return actor;
	}
} // namespace TopdownShooter::Entity::Player
