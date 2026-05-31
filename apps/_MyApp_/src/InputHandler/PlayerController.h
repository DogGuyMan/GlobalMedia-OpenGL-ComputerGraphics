#ifndef _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
#define _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__

#include "Entity/Components/Components.Interfaces.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "scene/actor.h"
#include <functional>
#include <vmath.h>
#include "Entity/Components/Components.Interfaces.h"

namespace SJH::Scene
{
	class Camera; // 마우스→Ground raycast 용 (포인터 멤버 — 전방 선언으로 충분)
}

namespace TopdownShooter::Controller
{
	/// @brief Top-down 게임의 Player 이동 컨트롤러 — WASD  Owner Transform.Translate XZ 이동.
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
			Damage,          // G (이산 press — Damage Composite 트리거)
		};

		PlayerController()                                    = default;
		PlayerController(const PlayerController &)            = delete;
		PlayerController &operator=(const PlayerController &) = delete;

		bool SetUp();

		//  Builder Pattern — fluent setter (self 반환)
		/// @brief 키보드 입력 의존 주입. SetUp() 전에 호출 필수.
		PlayerController &SetKeyboardInput(SJH::KeyboardInput<Action> *k);

		/// @brief IMovable 구현체 (Entity 자체 또는 Movement Component) 주입. SetUp() 전에 호출 필수.
		PlayerController &SetMovableTarget(Entity::IMovable* target);

		/// @brief 마우스 입력 의존 주입 (선택적 — 좌클릭 Fire 바인딩용). SetUp() 전에 호출 필수.
		PlayerController &SetMouseInput(SJH::MouseInput *m);

		/// @brief World 카메라 주입 (선택적 — 좌클릭 시 마우스→Ground raycast 용). 미주입이면 raycast 생략.
		PlayerController &SetWorldCamera(SJH::Scene::Camera *cam);

		/// @brief 좌클릭 시 실행할 콜백 (Shot Composite 등). 미주입이면 좌클릭 무시.
		PlayerController &SetFireCallback(std::function<void()> cb);

		/// @brief G키 press 시 실행할 콜백 (Damage Composite 등). 미주입이면 G키 무시.
		PlayerController &SetDamageCallback(std::function<void()> cb);

		/// @brief 마지막 좌클릭의 조준 정보 — PlayerActor(owner)→클릭 Ground 좌표.
		/// @details `mAimDirection` 은 XZ 평면 정규화 방향(발사/회전 방향). `mAimPoint` 는 클릭된 월드 좌표.
		const vmath::vec3 &GetAimDirection() const { return mAimDirection; }
		const vmath::vec3 &GetAimPoint() const { return mAimPoint; }

		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override;

	  private:
		bool mIsInitialized                          = false;
		SJH::KeyboardInput<Action> *mKeyboardInput   = nullptr;
		SJH::MouseInput *mMouseInput                 = nullptr;
		SJH::Scene::Camera *mCamera                  = nullptr; // 마우스→Ground raycast 용 (비소유)
		Entity::IMovable* mMovementPtr = nullptr;

		std::function<void()> mFireCallback;   // 좌클릭
		std::function<void()> mDamageCallback; // G키

		vmath::vec3 mInputValue {0.0f};

		// 좌클릭 시 추출되는 조준 정보 — PlayerActor(owner) 위치 + 클릭된 Ground 좌표로 산출.
		vmath::vec3 mAimPoint {0.0f};                  // 클릭된 Ground 월드 좌표 (y≈0)
		vmath::vec3 mAimDirection {0.0f, 0.0f, -1.0f}; // player → click 방향 (XZ 평면, 정규화)

		void RegisterBindings();
		void UnregisterBindings();

		// [TEST] 마우스 클릭 화면좌표 → 카메라 ray → y=0 평면 교차 → Ground 월드 좌표.
		//        결과를 로그 + 그 위치에 노란 박스 MeshRenderer Actor 스폰 (raycast 시각 검증).
		void TestPickGroundAndSpawnMarker();
		void SpawnGroundMarker(const vmath::vec3 &worldPos);
	};
} // namespace TopdownShooter::Controller

#endif //_TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
