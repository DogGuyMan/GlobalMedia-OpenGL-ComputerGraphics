/**
 * @file PlayerController.h
 * @brief Top-down 플레이어 입력 컨트롤러 - WASD 이동 + 마우스 조준/발사 + 대시/궁극기.
 *
 * @details
 *  ### 책임
 *  - WASD 입력을 모아 @c IMovable 타겟의 XZ 평면 이동 호출 (카메라 회전과 독립).
 *  - 매 프레임 마우스->Ground raycast 로 조준 정보(방향/지점/각도/거리/화면강도) 산출.
 *  - 좌클릭 발사(Weapon 경유 + 핀치 통지 + 콜백), Shift 대시, R 궁극기 레이저 발동.
 *  - 조준 결과로 facing/pose 를 단일 작성자로서 @c IActorPresentation sink 에 송신.
 *
 *  ### 비-책임
 *  - [X] 카메라 추종/이동 - @c ActorFolower / @c CameraController 담당.
 *  - [X] 발사체/연출 생성 - Weapon Component / 콜백 / Spawns 자유 함수 위임.
 *  - [X] 타이머 tick - @c BaseEntity 중앙 컨테이너가 유일 tick (여기선 핸들 Reset/조회만).
 *
 *  ### 입력 -> 액터 흐름
 *  키 held 누적(mInputValue) -> Update 에서 IMovable::DoForward 호출 + 마우스 raycast 로
 *  조준 산출 -> facing 회전(pivot Transform) + presentation sink 토글.
 *
 * @note 좌클릭 발사 + 마우스 조준 raycast 에 World 카메라가 필요하며, 미주입 시 raycast 생략.
 */
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
	class Camera; // 마우스->Ground raycast 용 (포인터 멤버 - 전방 선언으로 충분)
}

namespace SJH::Timer { class Timer; }
namespace TopdownShooter::Entity { class BaseEntity; }
namespace TopdownShooter::Entity { class PlayerHands; } // 발사 핀치 통지용 (포인터 멤버)

class b2World; // 궁극기(R) 회전 히트스캔 레이저 발동용 물리 월드 (포인터 멤버 - 전방 선언)

namespace TopdownShooter::Controller
{
	/// @brief Top-down 게임의 Player 이동 컨트롤러 - WASD  Owner Transform.Translate XZ 이동.
	/// @details
	///   ### 동작 (Compound Actor 컨벤션)
	///   - 제어 대상은 *Component 의 owner Actor* 의 Transform (sprite Actor).
	///   - WASD: W=앞=-Z, S=뒤=+Z, A=-X, D=+X (월드 축 기준).
	///   - 카메라 회전과 독립 - 탑다운 컨벤션 (회전 적용 없음).
	class PlayerController : public SJH::Scene::Component
	{
	  public:
		/// @brief 플레이어 액션 식별자 - KeyboardInput<Action> 의 키 바인딩 키.
		enum class Action : int
		{
			MoveForward = 1, ///< W - 전방(-Z).
			MoveBack,        ///< S - 후방(+Z).
			MoveLeft,        ///< A - 좌측(-X).
			MoveRight,       ///< D - 우측(+X).
			DashImpulse,     ///< Shift - 대시 임펄스.
			Ultimate,        ///< R - 회전 히트스캔 레이저 궁극기.
		};

		// Actor::AddComponent<T>() 가 호출 - public default ctor 필수. 복사 금지(입력 바인딩 소유).
		PlayerController()                                    = default;
		PlayerController(const PlayerController &)            = delete;
		PlayerController &operator=(const PlayerController &) = delete;

		/// @brief 주입된 의존을 검증하고 키/마우스 바인딩을 등록.
		/// @details KeyboardInput 미주입이면 error 로그 후 false. 그 외 의존은 선택적.
		/// @return 셋업 성공 여부. false 면 Update no-op.
		bool SetUp();

		//  Builder Pattern - fluent setter (self 반환)
		/// @brief 키보드 입력 의존 주입. SetUp() 전에 호출 필수.
		PlayerController &SetKeyboardInput(SJH::KeyboardInput<Action> *k);

		/// @brief IMovable 구현체 (Entity 자체 또는 Movement Component) 주입. SetUp() 전에 호출 필수.
		PlayerController &SetMovableTarget(Entity::IMovable* target);

		/// @brief 마우스 입력 의존 주입 (선택적 - 좌클릭 Fire 바인딩용). SetUp() 전에 호출 필수.
		PlayerController &SetMouseInput(SJH::MouseInput *m);

		/// @brief World 카메라 주입 (선택적 - 좌클릭 시 마우스->Ground raycast 용). 미주입이면 raycast 생략.
		PlayerController &SetWorldCamera(SJH::Scene::Camera *cam);

		/// @brief 좌클릭 시 실행할 콜백 (Shot Composite 등). 미주입이면 좌클릭 무시.
		PlayerController &SetFireCallback(std::function<void()> cb);

		/// @brief G키 press 시 실행할 콜백 (Damage Composite 등). 미주입이면 G키 무시.
		PlayerController &SetDamageCallback(std::function<void()> cb);

		/// @brief facing 회전(EulerRot[1]=aimAngleY)을 적용할 pivot 주입 (선택적, 멱등).
		///        미주입이면 owner(root) 회전(하위호환). 데칼 spin 분리용 - root는 비회전,
		///        aimPivot만 회전시켜 손 궤도는 aimPivot 상속으로 유지.
		PlayerController &SetFacingPivot(SJH::Scene::Actor *pivot);

		/// @brief 궁극기(R) 회전 히트스캔 레이저 발동용 물리 월드 주입 (선택적, 멱등). 미주입이면 R 궁극기 no-op.
		PlayerController &SetWorld(b2World *world);


		/// @brief 조준 정보 - 매 프레임 마우스->Ground raycast 로 갱신 (PlayerActor(owner)->커서 Ground).
		/// @details `mAimDirection` 은 XZ 평면 정규화 방향(발사/회전 방향). `mAimPoint` 는 커서 월드 좌표.
		///          `mAimAngleY` 는 facing Y각(degree). 유효 교차 없으면 직전값 유지.
		const vmath::vec3 &GetAimDirection() const { return mAimDirection; }
		const vmath::vec3 &GetAimPoint() const { return mAimPoint; }
		float GetAimAngleY() const { return mAimAngleY; }
		/// @brief player(owner) 중심 -> 커서 Ground 의 XZ 거리(world). *거리 의존* 소비자용.
		float GetAimDistance() const { return mAimDistance; }
		/// @brief 플레이어 화면위치<->커서의 NDC 거리 - 0(겹침/중심)~1(화면 가장자리, 포화). 손 spread 보간용.
		float GetAimScreenT() const { return mAimScreenT; }

		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override;

	  private:
		bool mIsInitialized                          = false;
		SJH::KeyboardInput<Action> *mKeyboardInput   = nullptr;
		SJH::MouseInput *mMouseInput                 = nullptr;
		SJH::Scene::Camera *mCamera                  = nullptr; // 마우스->Ground raycast 용 (비소유)
		b2World *mWorld                              = nullptr; // 궁극기(R) 레이캐스트용 물리 월드 (비소유)
		SJH::Scene::Actor *mFacingPivot = nullptr; // facing 회전 대상 (미주입 시 owner) - 데칼 spin 분리
		Entity::IMovable* mMovementPtr = nullptr;
		TopdownShooter::Entity::BaseEntity* mEntity = nullptr; // 대시 중 이동 suppress 게이트(IsImpulseActive)

		std::function<void()> mFireCallback;   // 좌클릭

		vmath::vec3 mInputValue {0.0f};
		vmath::vec3 mPrevInputValue {0.0f};

		// 매 프레임 마우스->Ground raycast 로 갱신되는 조준 정보 - PlayerActor(owner) 위치 + 커서 Ground 좌표.
		vmath::vec3 mAimPoint {0.0f};                  // 커서 Ground 월드 좌표 (y~=0)
		vmath::vec3 mAimDirection {0.0f, 0.0f, -1.0f}; // player -> 커서 방향 (XZ 평면, 정규화)
		float       mAimAngleY = 0.0f;                 // facing Y각 (degree) = degrees(atan2(-dir.x,-dir.z))
		float       mAimDistance = 0.0f;               // player(owner)->커서 Ground 의 XZ 거리 (world)
		float       mAimScreenT = 0.0f;                // 플레이어 화면위치<->커서 NDC 거리 0(중심)~1(가장자리) - 손 spread 보간용
		bool        mAimValid  = false;                // 이번 프레임 유효 교차 여부 (false 면 직전값 유지)

		// === RD5: facing/pose 단일 작성자 sink (인터페이스로만 - 구체 PlayableDirector 미참조). ===
		TopdownShooter::Entity::IActorPresentation *mSink = nullptr; // = PlayableDirector(인터페이스로만, lazy)
		TopdownShooter::Entity::EFacing             mLastFacing = TopdownShooter::Entity::EFacing::Front;
		float                                       mAttackWindowSec = 0.15f; // 발사 후 "조준 응시" 윈도 (Register base)
		SJH::Timer::Timer*                          mAttackTimer     = nullptr; // 중앙 컨테이너 핸들 (비소유)

		void RegisterBindings();
		void UnregisterBindings();

		// 마우스 커서 -> 카메라 ray -> y=0 평면 교차 -> 조준 멤버(mAimPoint/Direction/AngleY) 갱신.
		// 매 프레임(Update) + 좌클릭 직전 호출. 유효 교차 없으면 mAimValid=false + 직전값 유지 (silent).
		bool UpdateAim();
		// 좌클릭 액션 - UpdateAim 갱신 -> Weapon 발사 + onFire + 디버그 마커 + GroundClick 콜백.
		void OnFirePressed();
		void SpawnGroundMarker(const vmath::vec3 &worldPos);
	};
} // namespace TopdownShooter::Controller

#endif //_TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
