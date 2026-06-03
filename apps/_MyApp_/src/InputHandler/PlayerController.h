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
	class Camera; // 마우스->Ground raycast 용 (포인터 멤버 — 전방 선언으로 충분)
}

namespace SJH::Timer { class Timer; }
namespace TopdownShooter::Entity { class BaseEntity; }

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

		/// @brief World 카메라 주입 (선택적 — 좌클릭 시 마우스->Ground raycast 용). 미주입이면 raycast 생략.
		PlayerController &SetWorldCamera(SJH::Scene::Camera *cam);

		/// @brief 좌클릭 시 실행할 콜백 (Shot Composite 등). 미주입이면 좌클릭 무시.
		PlayerController &SetFireCallback(std::function<void()> cb);

		/// @brief G키 press 시 실행할 콜백 (Damage Composite 등). 미주입이면 G키 무시.
		PlayerController &SetDamageCallback(std::function<void()> cb);

		/// @brief 좌클릭으로 Ground 좌표가 추출됐을 때 실행할 콜백 (월드 좌표 전달).
		/// @details VFX 테스트(선택 이펙트 소환) 등 *클릭 위치 소비자* 용. 미주입이면 무시.
		PlayerController &SetGroundClickCallback(std::function<void(const vmath::vec3 &worldPos)> cb);

		/// @brief 조준 정보 — 매 프레임 마우스->Ground raycast 로 갱신 (PlayerActor(owner)->커서 Ground).
		/// @details `mAimDirection` 은 XZ 평면 정규화 방향(발사/회전 방향). `mAimPoint` 는 커서 월드 좌표.
		///          `mAimAngleY` 는 facing Y각(degree). 유효 교차 없으면 직전값 유지.
		const vmath::vec3 &GetAimDirection() const { return mAimDirection; }
		const vmath::vec3 &GetAimPoint() const { return mAimPoint; }
		float GetAimAngleY() const { return mAimAngleY; }

		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override;

	  private:
		bool mIsInitialized                          = false;
		SJH::KeyboardInput<Action> *mKeyboardInput   = nullptr;
		SJH::MouseInput *mMouseInput                 = nullptr;
		SJH::Scene::Camera *mCamera                  = nullptr; // 마우스->Ground raycast 용 (비소유)
		Entity::IMovable* mMovementPtr = nullptr;
		TopdownShooter::Entity::BaseEntity* mEntity = nullptr; // 대시 중 이동 suppress 게이트(IsImpulseActive)

		std::function<void()> mFireCallback;   // 좌클릭
		std::function<void()> mDamageCallback; // G키
		std::function<void(const vmath::vec3 &)> mGroundClickCallback; // 좌클릭 Ground 좌표 소비자(VFX 소환 등)

		vmath::vec3 mInputValue {0.0f};

		// 매 프레임 마우스->Ground raycast 로 갱신되는 조준 정보 — PlayerActor(owner) 위치 + 커서 Ground 좌표.
		vmath::vec3 mAimPoint {0.0f};                  // 커서 Ground 월드 좌표 (y≈0)
		vmath::vec3 mAimDirection {0.0f, 0.0f, -1.0f}; // player -> 커서 방향 (XZ 평면, 정규화)
		float       mAimAngleY = 0.0f;                 // facing Y각 (degree) = degrees(atan2(-dir.x,-dir.z))
		bool        mAimValid  = false;                // 이번 프레임 유효 교차 여부 (false 면 직전값 유지)

		// === RD5: facing/pose 단일 작성자 sink (인터페이스로만 — 구체 PlayableDirector 미참조). ===
		TopdownShooter::Entity::IActorPresentation *mSink = nullptr; // = PlayableDirector(인터페이스로만, lazy)
		TopdownShooter::Entity::EFacing             mLastFacing = TopdownShooter::Entity::EFacing::Front;
		float                                       mAttackWindowSec = 0.15f; // 발사 후 "조준 응시" 윈도 (Register base)
		SJH::Timer::Timer*                          mAttackTimer     = nullptr; // 중앙 컨테이너 핸들 (비소유)

		void RegisterBindings();
		void UnregisterBindings();

		// 마우스 커서 -> 카메라 ray -> y=0 평면 교차 -> 조준 멤버(mAimPoint/Direction/AngleY) 갱신.
		// 매 프레임(Update) + 좌클릭 직전 호출. 유효 교차 없으면 mAimValid=false + 직전값 유지 (silent).
		bool UpdateAim();
		// 좌클릭 액션 — UpdateAim 갱신 -> Weapon 발사 + onFire + 디버그 마커 + GroundClick 콜백.
		void OnFirePressed();
		void SpawnGroundMarker(const vmath::vec3 &worldPos);
	};
} // namespace TopdownShooter::Controller

#endif //_TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
