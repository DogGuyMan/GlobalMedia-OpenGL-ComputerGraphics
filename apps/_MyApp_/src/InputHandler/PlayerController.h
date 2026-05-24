#ifndef _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
#define _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__

#include "input/keyboard_input.h"
#include "scene/actor.h"
#include <vmath.h>

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

		/// @brief 이동 속도 (월드 단위/프레임, default 0.05).
		PlayerController &SetMoveSpeed(float v);

		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override;

	  private:
		bool mIsInitialized                          = false;
		SJH::KeyboardInput<Action> *mKeyboardInput   = nullptr;
		vmath::vec3 mMoveDelta                       = vmath::vec3(0.0f, 0.0f, 0.0f); // 매 Update reset
		float mMoveSpeed                             = 0.05f;

		void RegisterBindings();
		void UnregisterBindings();
	};
} // namespace TopdownShooter::Controller

#endif //_TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
