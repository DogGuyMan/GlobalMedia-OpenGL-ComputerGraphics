/**
 * @file CameraController.h
 * @brief WASD+EQ 이동 + 마우스 look 의 자유 비행(free-fly) 1인칭 카메라 Component.
 *
 * @details
 *  ### 책임
 *  - 키보드 입력을 받아 카메라 로컬 축(right/up/forward) 기준 이동 누적.
 *  - 마우스 입력 누적으로 yaw/pitch 회전 -> owner Transform.EulerRot 갱신.
 *  - Camera Component 의 owner Actor Transform 을 직접 조작 (Compound Actor 컨벤션).
 *
 *  ### 비-책임
 *  - [X] target 추종 - @c ActorFolower 가 담당 (본 클래스는 자유 비행 전용).
 *  - [X] 발사/게임 액션 - @c PlayerController 가 담당.
 *
 *  ### 정통 매핑
 *  - 디버그/에디터 free-fly 카메라 (Unreal viewport WASD navigation).
 *
 * @note 이동 속도는 *프레임당* 값이라 현재 dt 미사용 (실 dt 적용은 후속 개선).
 */
#ifndef _TOPDOWNSHOOTER_INPUT_CAMERA_CONTROLLER__
#define _TOPDOWNSHOOTER_INPUT_CAMERA_CONTROLLER__

#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::Controller
{
	/// @brief Transform-based 1인칭 카메라 컨트롤러.
	/// @details
	///   ### 동작 모드 (Compound Actor 컨벤션)
	///   - 제어 대상은 *Camera Component 의 owner Actor* 의 Transform.
	///   - mYawDeg / mPitchDeg 누적 -> owner.Transform.EulerRot 갱신.
	///   - WASD/EQ 누적 -> owner.Transform.Translate 의 카메라 로컬 축 투영 으로 갱신.
	///   - forward = owner.WorldMatrix 의 -Z 컬럼 (정통).
	class CameraController : public SJH::Scene::Component
	{
	  public:
		/// @brief 이동 액션 식별자 - KeyboardInput<Action> 의 키 바인딩 키.
		enum class Action : int
		{
			MoveForward = 1, ///< W - 전방(-Z).
			MoveBack,        ///< S - 후방(+Z).
			MoveLeft,        ///< A - 좌측(-X).
			MoveRight,       ///< D - 우측(+X).
			MoveUp,          ///< E (or Space) - 상승(+Y).
			MoveDown,        ///< Q (or LeftCtrl) - 하강(-Y).
		};

		// Actor::AddComponent<T>() 가 호출 - public default ctor 필수. 복사 금지(입력 바인딩 소유).
		CameraController() = default;
		CameraController(const CameraController &) = delete;
		CameraController &operator=(const CameraController &) = delete;

		/// @brief 주입된 의존을 검증하고 키보드/마우스 바인딩을 등록.
		/// @details keyboard/mouse/camera 중 하나라도 누락이면 error 로그 후 false.
		/// @return 셋업 성공 여부. false 면 Update no-op.
		bool SetUp();

		//  Builder Pattern — fluent setter (self 반환)
		/// @brief 키보드 입력 의존 주입. SetUp() 전에 호출 필수.
		CameraController &SetKeyboardInput(SJH::KeyboardInput<Action> *k);

		/// @brief 마우스 입력 의존 주입. SetUp() 전에 호출 필수.
		CameraController &SetMouseInput(SJH::MouseInput *m);

		/// @brief 제어할 Camera Component 의존 주입. SetUp() 전에 호출 필수.
		/// @note Camera 의 owner Actor 의 Transform 을 갱신. Camera 가 Actor 미부착이면 Update no-op.
		CameraController &SetCamera(SJH::Scene::Camera *c);

		/// @brief Component 진입 hook. 미초기화면 no-op.
		virtual void OnEnter() override;
		/// @brief Component 이탈 hook. 입력 바인딩 해제 + 의존 리셋.
		virtual void OnExit() override;
		/// @brief 매 프레임 회전/이동 누적값을 owner Transform 에 반영 후 누적 리셋.
		/// @param dt 직전 프레임 경과 시간(초). 현재 미사용 (이동이 프레임당 값).
		virtual void Update(float dt) override;

	  private:
		bool mIsInitialized = false;                            ///< SetUp 성공 여부 - 모든 hook 의 게이트.
		SJH::MouseInput *mMouseInput = nullptr;                  ///< 마우스 look 입력 소스 (비소유).
		SJH::KeyboardInput<Action> *mKeyboardInput = nullptr;    ///< 키보드 이동 입력 소스 (비소유).
		SJH::Scene::Camera *mCamera = nullptr;                   ///< 제어 대상 Camera Component (비소유).

		float mYawDeg = 0.0f;                                    ///< 누적 yaw 각도(degree).
		float mPitchDeg = 0.0f;                                  ///< 누적 pitch 각도(degree).
		vmath::vec3 mMoveDelta = vmath::vec3(0.0f, 0.0f, 0.0f);  ///< 이번 프레임 이동 누적 (held handler 가 더함, Update 끝에서 reset).

		float mLookSensitivity = 0.1f;                          ///< 마우스 이동량 -> 각도 변환 감도.
		float mMoveSpeed = 0.05f;                               ///< 프레임당 이동 거리.

		/// @brief WASD/EQ 키 + 마우스 look 핸들러를 입력 시스템에 등록 (SetUp 내부 호출).
		void RegisterBindings();
		/// @brief 등록된 키/look 핸들러를 해제 (OnExit 내부 호출).
		void UnregisterBindings();
	};
} // namespace TopdownShooter::Controller

#endif //_TOPDOWNSHOOTER_INPUT_CAMERA_CONTROLLER__