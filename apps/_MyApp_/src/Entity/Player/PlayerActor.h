/**
 * @file PlayerActor.h
 * @brief 플레이어 Actor 팩토리 - PoD Config(Pattern C) + CreatePlayerActor 선언.
 *
 * @details
 *  ### 책임
 *  - @c PlayerActorConfig (nested PoD struct) 로 각 Component 생성 인자를 응집해 노출.
 *  - @c CreatePlayerActor 로 Component 부착 + 의존 wiring 을 한 자리에 묶는다.
 *
 *  ### 비-책임
 *  - [X] Component 구현 - Life/Movement/Weapon/Controller 각 모듈 담당.
 *  - [X] 튜닝 수치 - @c Entity/Constants.h / @c Playable/Constants.h 단일 소스.
 *  - [X] facade verb 구현 - @c PlayerEntity 담당.
 *
 *  ### 정통 매핑
 *  - Pattern C (Config Struct + Factory) - Config 는 Value Object, Factory 가 조립/wiring.
 *  - Compound Actor 컨벤션 (Actor 비상속, 특수 속성은 Component 로만).
 *
 * @note 도메인 Compound 라 엔진 코어(src/scene/compound_actor.h) 가 아닌 클라이언트 도메인에 위치한다
 *       (의존 방향 보존).
 */
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
	/// @brief PlayerActor 생성 매개변수 - PoD config (Pattern C - Config Struct + Factory).
	/// @details
	///   ### 설계 의도
	///   - 각 Component 의 생성 인자를 *nested struct* 로 응집 - Component 추가 시 *PlayerActorConfig 의 nested 멤버 추가* 만으로 확장 (OCP).
	///   - Config 자체는 *Value Object* - 불변 의미. 호출 측이 필드 setter 없이 직접 대입 또는 aggregate init.
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

		/// @brief Life Component 생성 인자.
		struct LifeCfg
		{
			int hp = PLAYER_HP; ///< 초기 체력.
		};
		/// @brief Movement/PhysicsMovement 공통 이동 인자.
		struct MovementCfg
		{
			float speed = PLAYER_MOVE_SPEED; ///< 이동 속도 (단위/초).
		};
		/// @brief PlayerController 입력/조준/콜백 wiring 인자.
		struct ControllerCfg
		{
			SJH::KeyboardInput<Controller::PlayerController::Action> *keyboard = nullptr; ///< WASD 등 키 입력 소스 (null 이면 Controller 미부착).
			SJH::MouseInput      *mouse    = nullptr;   // 좌클릭 Fire 바인딩용 (선택)
			SJH::Scene::Camera   *camera   = nullptr;   // 좌클릭 마우스->Ground raycast 용 (선택)
			std::function<void()> onFire;               // 좌클릭 콜백 (선택)
			std::function<void()> onDamage;             // G키 콜백 (선택)
		};

		/// @brief BoxBody 물리 인자. @c world 가 null 이면 비물리 분기(Movement)로 조립된다.
		struct PhysicsCfg
		{
			b2World*    world         = nullptr;                 ///< 물리 월드 (null 이면 비물리 분기).
			vmath::vec2 size          = vmath::vec2(1.0f, 1.0f); ///< BoxBody 크기.
			vmath::vec2 startPosition = vmath::vec2(0.0f, 0.0f); ///< 스폰 위치 (Box2D XY).
			float       density       = 1.0f;                    ///< 밀도.
			float       friction      = 0.3f;                    ///< 마찰 계수.
			float       linearDamping = PLAYER_LINEAR_DAMPING;   ///< 선형 감쇠 (관성 정지감).
			uint16_t    categoryBits  = 0;                       ///< 충돌 카테고리 비트.
			uint16_t    maskBits      = 0;                        ///< 충돌 마스크 비트.
			bool        isSensor      = false;                   ///< 센서(트리거) 여부.
		};

		struct WeaponCfg
		{
			int      damage = PLAYER_WEAPON_DAMAGE; // bullet 데미지 (Stat base)
			b2World* world  = nullptr;  // bullet body 생성용 물리 월드 (physics 분기에서만 부착)
		};

		/// @brief 방향 텍스처 합성 설정 (spec sec.6.1). M6 Task7 의 SpriteCfg{atlas,clips} 를 대체.
		struct SpriteCfg
		{
			/// @brief 방향 텍스처 세트(예: Playable::FRONT_MOVE). nullptr -> 스프라이트 없음(게임플레이-only).
			const std::vector<TopdownShooter::Playable::EntityTextureConfig> *direction = nullptr;
			float fps = PLAYER_SPRITE_FPS; ///< 애니 파트(ColCount>1) 의 초당 프레임
		};

		LifeCfg       life;       ///< Life Component 인자.
		MovementCfg   movement;   ///< 이동 속도 인자.
		ControllerCfg controller; ///< 입력/조준/콜백 인자 (keyboard null 이면 Controller 미부착).
		PhysicsCfg    physics;    ///< 물리 인자 (world null 이면 비물리 분기).
		WeaponCfg     weapon;     ///< 무기/발사 인자 (물리 분기에서만 실효).
		SpriteCfg     sprite;     ///< 방향 텍스처 합성 인자 (direction null 이면 게임플레이-only).
	};

	/// @brief PlayerActor 생성 - Compound Actor 컨벤션 (Actor 비상속) + Component 부착.
	/// @details
	///   ### Component 부착 순서
	///   1. Life
	///   2. Movement
	///   3. (선택) PlayerController - `cfg.controller.keyboard` 가 비-null 일 때만.
	///      내부에서 SetKeyboardInput + SetMovableTarget(movement) + SetUp() 자동 wiring.
	///
	///   ### 위치 선택 (Clean Architecture 일관성)
	///   본 함수는 *클라이언트 도메인* (TopdownShooter) 영역 - `src/scene/compound_actor.h` (엔진 코어)
	///   가 *클라이언트 도메인을 모르는* 의존 방향 보존. 범용 Compound (Camera/Light) 는 엔진 코어,
	///   도메인 Compound (PlayerActor) 는 도메인 영역에 위치.
	std::unique_ptr<SJH::Scene::Actor> CreatePlayerActor(const PlayerActorConfig &cfg);
} // namespace TopdownShooter::Entity::Player

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_ACTOR__
