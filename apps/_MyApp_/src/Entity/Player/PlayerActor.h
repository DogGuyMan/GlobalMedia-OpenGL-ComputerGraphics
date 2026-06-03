#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__

#include "Entity/Components/LifeComponents.h"
#include "Entity/Components/MovementComponents.h"
#include "Entity/Components/WeaponComponents.h"
#include "InputHandler/PlayerController.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/PhysicsMovement.h"
#include "Playable/Constants.h"   // TopdownShooter::Playable::PlayerTextureConfig / FRONT_MOVE 등
#include "Entity/Constants.h"      // TopdownShooter::Entity::PLAYER_* / HAND_* (튜닝 단일 소스)
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
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
			int hp = PLAYER_HP;
		};
		struct MovementCfg
		{
			float speed = PLAYER_MOVE_SPEED;
		};
		struct ControllerCfg
		{
			SJH::KeyboardInput<Controller::PlayerController::Action> *keyboard = nullptr;
			SJH::MouseInput      *mouse    = nullptr;   // 좌클릭 Fire 바인딩용 (선택)
			SJH::Scene::Camera   *camera   = nullptr;   // 좌클릭 마우스->Ground raycast 용 (선택)
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
			float       linearDamping = PLAYER_LINEAR_DAMPING;
			uint16_t    categoryBits  = 0;
			uint16_t    maskBits      = 0;
			bool        isSensor      = false;
		};

		struct WeaponCfg
		{
			int      damage = PLAYER_WEAPON_DAMAGE; // bullet 데미지 (Stat base)
			b2World* world  = nullptr;  // bullet body 생성용 물리 월드 (physics 분기에서만 부착)
		};

		/// @brief 방향 텍스처 합성 설정 (spec §6.1). M6 Task7 의 SpriteCfg{atlas,clips} 를 대체.
		struct SpriteCfg
		{
			/// @brief 방향 텍스처 세트(예: Playable::FRONT_MOVE). nullptr -> 스프라이트 없음(게임플레이-only).
			const std::vector<TopdownShooter::Playable::EntityTextureConfig> *direction = nullptr;
			float fps = PLAYER_SPRITE_FPS; ///< 애니 파트(ColCount>1) 의 초당 프레임
		};

		LifeCfg       life;
		MovementCfg   movement;
		ControllerCfg controller;
		PhysicsCfg    physics;
		WeaponCfg     weapon;
		SpriteCfg     sprite;
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
	std::unique_ptr<SJH::Scene::Actor> CreatePlayerActor(const PlayerActorConfig &cfg);
} // namespace TopdownShooter::Entity::Player

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__
