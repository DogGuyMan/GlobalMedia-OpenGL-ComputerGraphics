#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__

#include "Entity/Components/LifeComponents.h"
#include "Entity/Components/MovementComponents.h"
#include "InputHandler/PlayerController.h"
#include "input/keyboard_input.h"
#include "scene/actor.h"
#include <memory>
#include <string>

namespace TopdownShooter::Entity::Player
{
	/// @brief PlayerActor 생성 매개변수 — PoD config (Pattern C — Config Struct + Factory).
	/// @details
	///   ### 설계 의도
	///   - 각 Component 의 생성 인자를 *nested struct* 로 응집 — Component 추가 시 *PlayerActorConfig 의 nested 멤버 추가* 만으로 확장 (OCP).
	///   - Config 자체는 *Value Object* — 불변 의미. 호출 측이 필드 setter 없이 직접 대입 또는 aggregate init.
	///   - Component 간 의존 주입 (Controller → Movement) 은 *Factory 함수 안에서 한 자리* 에 wiring.
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
		};

		LifeCfg       life;
		MovementCfg   movement;
		ControllerCfg controller;
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
		auto *movement = actor->AddComponent<Components::Movement>(cfg.movement.speed);

		if (cfg.controller.keyboard != nullptr)
		{
			auto *controller = actor->AddComponent<Controller::PlayerController>();
			controller->SetKeyboardInput(cfg.controller.keyboard);
			controller->SetMovableTarget(movement);
			controller->SetUp();
		}

		return actor;
	}
} // namespace TopdownShooter::Entity::Player

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__
