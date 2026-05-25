#ifndef _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
#define _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__

#include "Entity/Components/Components.Interfaces.h"
#include "input/keyboard_input.h"
#include "scene/actor.h"
#include <vmath.h>
#include "Entity/Components/MovementComponents.h"

namespace TopdownShooter::Controller
{
	/// @brief Top-down 게임의 Player 이동 컨트롤러 — WASD → Owner Transform.Translate XZ 이동.
	/// @details
	///   ### 동작 (Compound Actor 컨벤션)
	///   - 제어 대상은 *Component 의 owner Actor* 의 Transform (sprite Actor).
	///   - WASD: W=앞=-Z, S=뒤=+Z, A=-X, D=+X (월드 축 기준).
	///   - 카메라 회전과 독립 — 탑다운 컨벤션 (회전 적용 없음).
	class PlayerController : public SJH::Scene::Component
	{
	  public:
		enum class Action : int
		{
			MoveForward = 1, // W
			MoveBack,        // S
			MoveLeft,        // A
			MoveRight,       // D
		};

		PlayerController()                                    = default;
		PlayerController(const PlayerController &)            = delete;
		PlayerController &operator=(const PlayerController &) = delete;

		bool SetUp();

		//  Builder Pattern — fluent setter (self 반환)
		/// @brief 키보드 입력 의존 주입. SetUp() 전에 호출 필수.
		PlayerController &SetKeyboardInput(SJH::KeyboardInput<Action> *k);

		PlayerController &SetPlayerMovement(Entity::Components::Movement* m);

		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override;

	  private:
		bool mIsInitialized                          = false;
		SJH::KeyboardInput<Action> *mKeyboardInput   = nullptr;
		Entity::IMovable* mMovementPtr = nullptr;

		vmath::vec3 mInputValue {0.0f};

		void RegisterBindings();
		void UnregisterBindings();
	};
} // namespace TopdownShooter::Controller

#endif //_TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
