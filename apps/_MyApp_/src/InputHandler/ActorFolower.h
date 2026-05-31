#ifndef _TOPDOWNSHOOTER_INPUT_TARGET_FOLLOWABLE_CAMERA_CONTROLLER__
#define _TOPDOWNSHOOTER_INPUT_TARGET_FOLLOWABLE_CAMERA_CONTROLLER__

#include "input/mouse_input.h"
#include "scene/actor.h"
#include "scene/camera.h"
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
		ActorFolower()                                                    = default;
		ActorFolower(const ActorFolower &)            = delete;
		ActorFolower &operator=(const ActorFolower &) = delete;

		bool SetUp();

		//  Builder Pattern — fluent setter (self 반환)
		/// @brief 마우스 입력 의존 주입. SetUp() 전에 호출 필수.
		ActorFolower &SetMouseInput(SJH::MouseInput *m);

		/// @brief 제어할 Camera Component 의존 주입. SetUp() 전에 호출 필수.
		/// @note Camera 가 Actor 미부착이면 Update no-op.
		ActorFolower &SetCamera(SJH::Scene::Camera *c);

		/// @brief Follow target Actor 설정. nullptr 이면 Update no-op.
		ActorFolower &SetFollowTarget(SJH::Scene::Actor *t);

		/// @brief Follow 시 target  camera offset (default = vec3(0, 5, 5)).
		ActorFolower &SetFollowOffset(vmath::vec3 offset);
		ActorFolower &SetFollowRotate(vmath::vec2 rot);

		/// @brief 마우스 감도 (default 0.1).
		ActorFolower &SetLookSensitivity(float v);

		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override;

	  private:
		bool mIsInitialized              = false;
		SJH::MouseInput *mMouseInput     = nullptr;
		SJH::Scene::Camera *mCamera      = nullptr;
		SJH::Scene::Actor *mFollowTarget = nullptr;
		vmath::vec3 mFollowOffset        = vmath::vec3(0.0f, 0.0f, 0.0f);

		float mYawDeg          = 0.0f;
		float mPitchDeg        = 0.0f; // 탑다운 기본 시점 — 아래를 향함
		float mLookSensitivity = 0.1f;

		void RegisterBindings();
		void UnregisterBindings();
	};
} // namespace TopdownShooter::Controller

#endif //_TOPDOWNSHOOTER_INPUT_TARGET_FOLLOWABLE_CAMERA_CONTROLLER__
