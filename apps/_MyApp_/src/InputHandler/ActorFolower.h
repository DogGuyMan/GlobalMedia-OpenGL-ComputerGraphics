/**
 * @file ActorFolower.h
 * @brief Target Actor 를 부드럽게 추종하는 카메라 follow Component (탑다운 전용).
 *
 * @details
 *  ### 책임
 *  - 매 Update 마다 owner Camera 의 Transform 을 target Actor 위치 + offset 으로 추종.
 *  - dt 기반 ease 보간 으로 카메라 catch-up (튐/떨림 제거).
 *  - 마우스 입력 누적으로 yaw/pitch 회전 (target 머리 위 상대 회전).
 *
 *  ### 비-책임
 *  - [X] WASD 키보드 이동 - Player 가 별도 Component(@c PlayerController)로 담당.
 *  - [X] free-fly 모드 - 본 클래스는 전용 follow 컨트롤러이며 fallback 자유 비행 없음.
 *
 *  ### 정통 매핑
 *  - Unity Cinemachine `FollowTarget` - target 추종 + smoothing.
 *  - Cocos2D `Follow` Action - 노드 추종.
 *
 * @note 파일/클래스명의 "Folower" 철자는 의도된 표기로 유지 (이름 변경 금지).
 */
#ifndef _TOPDOWNSHOOTER_INPUT_TARGET_FOLLOWABLE_CAMERA_CONTROLLER__
#define _TOPDOWNSHOOTER_INPUT_TARGET_FOLLOWABLE_CAMERA_CONTROLLER__

#include "input/mouse_input.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include <functional>
#include <vmath.h>

namespace TopdownShooter::Controller
{
	/// @brief Target Actor 를 추적하는 카메라 컨트롤러 (탑다운 컨벤션).
	/// @details
	///   ### 동작
	///   - 매 Update: owner.Transform.Translate = target.Transform.Translate + mFollowOffset.
	///   - Mouse 누적 yaw/pitch -> owner.Transform.EulerRot 갱신 (target 머리 위 상대 회전).
	///   - WASD 이동 없음 — Player 가 별도 Component (PlayerController) 로 담당.
	///   ### CameraController 와의 차이
	///   - free-fly 모드를 갖지 않는 *전용* follow 컨트롤러.
	///   - KeyboardInput 의존 없음 (Mouse only).
	class ActorFolower : public SJH::Scene::Component
	{
	  public:
		/// @brief 이동 보간 커브 시그니처 — (position[0..1], start, end) -> 보간값.
		/// @details Tweeny 의 생성함수 커브(`tweeny::easing::*.run`)를 그대로 감싸 주입 가능.
		///          예: `[](float p, float a, float b){ return tweeny::easing::quadraticOut.run(p, a, b); }`
		using EaseFn = std::function<float(float, float, float)>;

		// Actor::AddComponent<T>() 가 호출 - public default ctor 필수. 복사 금지(입력 바인딩 소유).
		ActorFolower()                                                    = default;
		ActorFolower(const ActorFolower &)            = delete;
		ActorFolower &operator=(const ActorFolower &) = delete;

		/// @brief 주입된 의존을 검증하고 마우스 look 바인딩을 등록.
		/// @details mMouseInput/mCamera 누락 시 error 로그 후 false. ease 커브 미주입이면 default(cubicOut) 주입.
		/// @return 셋업 성공 여부. false 면 Update no-op.
		bool SetUp();

		//  Builder Pattern — fluent setter (self 반환)
		/// @brief 마우스 입력 의존 주입. SetUp() 전에 호출 필수.
		ActorFolower &SetMouseInput(SJH::MouseInput *m);

		/// @brief 제어할 Camera Component 의존 주입. SetUp() 전에 호출 필수.
		/// @note Camera 가 Actor 미부착이면 Update no-op.
		ActorFolower &SetCamera(SJH::Scene::Camera *c);

		/// @brief Follow target Actor 설정. nullptr 이면 Update no-op.
		ActorFolower &SetFollowTarget(SJH::Scene::Actor *t);

		/// @brief Follow 시 target 기준 camera offset (default = vec3(0, 0, 0)).
		/// @param offset target Translate 에 더해질 카메라 위치 오프셋.
		ActorFolower &SetFollowOffset(vmath::vec3 offset);

		/// @brief 초기 yaw/pitch 회전 각도(degree) 설정.
		/// @param rot (yaw, pitch) 쌍. 탑다운 시점 초기화용.
		ActorFolower &SetFollowRotate(vmath::vec2 rot);

		/// @brief 마우스 감도 (default 0.1).
		ActorFolower &SetLookSensitivity(float v);

		/// @brief 이동 보간 커브 주입 (default = `tweeny::easing::cubicOut`).
		/// @note 빈 함수 전달은 무시 (기존 커브 유지).
		ActorFolower &SetEaseFunction(EaseFn fn);

		/// @brief ease 한 구간(현재→목표)의 길이(초). 작을수록 빠르게 따라붙음 (default 0.18).
		/// @note 0 이하는 무시.
		ActorFolower &SetFollowDuration(float seconds);

		/// @brief 도착/retarget 판정 EPS. 작을수록 더 끝까지 따라감 (default 0.01).
		/// @note 0 이하는 무시.
		ActorFolower &SetArriveEps(float eps);

		/// @brief Component 진입 hook. 미초기화면 no-op.
		virtual void OnEnter() override;
		/// @brief Component 이탈 hook. look 바인딩 해제 + 의존/보간 상태 리셋.
		virtual void OnExit() override;
		/// @brief 매 프레임 target 추종 위치 ease 보간 갱신.
		/// @param dt 직전 프레임 경과 시간(초). fps 독립 진행에 사용.
		virtual void Update(float dt) override;

	  private:
		bool mIsInitialized              = false;       ///< SetUp 성공 여부 - 모든 hook 의 게이트.
		SJH::MouseInput *mMouseInput     = nullptr;      ///< 마우스 look 입력 소스 (비소유).
		SJH::Scene::Camera *mCamera      = nullptr;      ///< 제어 대상 Camera Component (비소유).
		SJH::Scene::Actor *mFollowTarget = nullptr;      ///< 추종 target Actor (비소유). nullptr 이면 Update no-op.
		vmath::vec3 mFollowOffset        = vmath::vec3(0.0f, 0.0f, 0.0f);  ///< target 기준 카메라 위치 오프셋.

		float mYawDeg          = 0.0f;    ///< 누적 yaw 각도(degree).
		float mPitchDeg        = 0.0f;    ///< 누적 pitch 각도(degree). 탑다운 기본 시점 - 아래를 향함.
		float mLookSensitivity = 0.1f;    ///< 마우스 이동량 -> 각도 변환 감도.

		// 이동 보간 상태 (카메라 떨림 방지)
		EaseFn      mEaseFn;                                         ///< 생성함수 커브 (SetUp 에서 default 주입).
		vmath::vec3 mEaseStart      = vmath::vec3(0.0f, 0.0f, 0.0f); ///< 현재 ease 구간 시작 위치.
		vmath::vec3 mEaseGoal       = vmath::vec3(0.0f, 0.0f, 0.0f); ///< 현재 ease 구간 목표 위치.
		float       mEaseProgress   = 1.0f;                          ///< [0,1] ease 진행도.
		float       mFollowDuration = 0.18f;                         ///< ease 구간 길이(초) - catch-up 속도.
		float       mArriveEps      = 0.01f;                         ///< EPS - retarget/도착 스레숄드.
		bool        mHasGoal        = false;                         ///< 첫 프레임 즉시 스냅 가드.

		/// @brief 마우스 look 핸들러를 MouseInput 에 등록 (SetUp 내부 호출).
		void RegisterBindings();
		/// @brief 등록된 look 핸들러를 해제 (OnExit 내부 호출).
		void UnregisterBindings();
	};
} // namespace TopdownShooter::Controller

#endif //_TOPDOWNSHOOTER_INPUT_TARGET_FOLLOWABLE_CAMERA_CONTROLLER__
