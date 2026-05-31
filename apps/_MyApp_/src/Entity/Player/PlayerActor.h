#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__

#include "Entity/Components/LifeComponents.h"
#include "Entity/Components/MovementComponents.h"
#include "InputHandler/PlayerController.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/physics_movement.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Entity::Player
{
	/// @brief PlayerActor 생성 매개변수 — PoD config (Pattern C — Config Struct + Factory).
	/// @details
	///   ### 설계 의도
	///   - 각 Component 의 생성 인자를 *nested struct* 로 응집 — Component 추가 시 *PlayerActorConfig 의 nested 멤버 추가* 만으로 확장 (OCP).
	///   - Config 자체는 *Value Object* — 불변 의미. 호출 측이 필드 setter 없이 직접 대입 또는 aggregate init.
	///   - Component 간 의존 주입 (Controller  Movement) 은 *Factory 함수 안에서 한 자리* 에 wiring.
	///
	///   ### 사용 예
	///   @code
	///   PlayerActorConfig cfg;
	///   cfg.name             = "Player1";
	///   cfg.life.hp          = 100;
	///   cfg.movement.speed   = 5.0f;
	///   cfg.controller.keyboard = &myKeyboard;
	///   auto playerActor = CreatePlayerActor(cfg);
	///   @endcode
	struct PlayerActorConfig
	{
		std::string name = "Player";

		struct LifeCfg
		{
			int hp = 100;
		};
		struct MovementCfg
		{
			float speed = 5.0f;
		};
		struct ControllerCfg
		{
			SJH::KeyboardInput<Controller::PlayerController::Action> *keyboard = nullptr;
			SJH::MouseInput      *mouse    = nullptr;   // 좌클릭 Fire 바인딩용 (선택)
			SJH::Scene::Camera   *camera   = nullptr;   // 좌클릭 마우스→Ground raycast 용 (선택)
			std::function<void()> onFire;               // 좌클릭 콜백 (선택)
			std::function<void()> onDamage;             // G키 콜백 (선택)
		};

		struct PhysicsCfg
		{
			b2World*    world         = nullptr;
			vmath::vec2 size          = vmath::vec2(1.0f, 1.0f);
			vmath::vec2 startPosition = vmath::vec2(0.0f, 0.0f);
			float       density       = 1.0f;
			float       friction      = 0.3f;
			float       linearDamping = 5.0f;
			uint16_t    categoryBits  = 0;
			uint16_t    maskBits      = 0;
			bool        isSensor      = false;
		};

		LifeCfg       life;
		MovementCfg   movement;
		ControllerCfg controller;
		PhysicsCfg    physics;
	};

	/// @brief PlayerActor 생성 — Compound Actor 컨벤션 (Actor 비상속) + Component 부착.
	/// @details
	///   ### Component 부착 순서
	///   1. Life
	///   2. Movement
	///   3. (선택) PlayerController — `cfg.controller.keyboard` 가 비-null 일 때만.
	///      내부에서 SetKeyboardInput + SetMovableTarget(movement) + SetUp() 자동 wiring.
	///
	///   ### 위치 선택 (Clean Architecture 일관성)
	///   본 함수는 *클라이언트 도메인* (TopdownShooter) 영역 — `src/scene/compound_actor.h` (엔진 코어)
	///   가 *클라이언트 도메인을 모르는* 의존 방향 보존. 범용 Compound (Camera/Light) 는 엔진 코어,
	///   도메인 Compound (PlayerActor) 는 도메인 영역에 위치.
	inline std::unique_ptr<SJH::Scene::Actor> CreatePlayerActor(const PlayerActorConfig &cfg)
	{
		auto actor = std::make_unique<SJH::Scene::Actor>(cfg.name);

		actor->AddComponent<Components::Life>(cfg.life.hp);

		if (cfg.physics.world != nullptr)
		{
			// Physics body 생성 — b2World 가 lifetime 소유.
			b2BodyDef bd;
			bd.type     = b2_dynamicBody;
			bd.position.Set(cfg.physics.startPosition[0], cfg.physics.startPosition[1]);
			bd.linearDamping = cfg.physics.linearDamping;
			b2Body *body = cfg.physics.world->CreateBody(&bd);

			b2PolygonShape box;
			box.SetAsBox(cfg.physics.size[0] * 0.5f, cfg.physics.size[1] * 0.5f);

			b2FixtureDef fd;
			fd.shape             = &box;
			fd.density           = cfg.physics.density;
			fd.friction          = cfg.physics.friction;
			fd.isSensor          = cfg.physics.isSensor;
			fd.filter.categoryBits = cfg.physics.categoryBits;
			fd.filter.maskBits     = cfg.physics.maskBits;
			body->CreateFixture(&fd);

			auto *pb = actor->AddComponent<Physics::Components::BoxBody>();
			pb->SetBody(body);
			pb->SetSensor(cfg.physics.isSensor);

			auto *pm = actor->AddComponent<Physics::PhysicsMovement>(cfg.movement.speed);

			if (cfg.controller.keyboard != nullptr)
			{
				auto *controller = actor->AddComponent<Controller::PlayerController>();
				controller->SetKeyboardInput(cfg.controller.keyboard);
				controller->SetMouseInput(cfg.controller.mouse);
				controller->SetWorldCamera(cfg.controller.camera);
				controller->SetMovableTarget(pm);
				controller->SetFireCallback(cfg.controller.onFire);
				controller->SetDamageCallback(cfg.controller.onDamage);
				controller->SetUp();
			}
		}
		else
		{
			// physics 미사용 — 기존 Movement (Transform 직접 조작).
			auto *movement = actor->AddComponent<Components::Movement>(cfg.movement.speed);

			if (cfg.controller.keyboard != nullptr)
			{
				auto *controller = actor->AddComponent<Controller::PlayerController>();
				controller->SetKeyboardInput(cfg.controller.keyboard);
				controller->SetMouseInput(cfg.controller.mouse);
				controller->SetWorldCamera(cfg.controller.camera);
				controller->SetMovableTarget(movement);
				controller->SetFireCallback(cfg.controller.onFire);
				controller->SetDamageCallback(cfg.controller.onDamage);
				controller->SetUp();
			}
		}

		return actor;
	}
} // namespace TopdownShooter::Entity::Player

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__
