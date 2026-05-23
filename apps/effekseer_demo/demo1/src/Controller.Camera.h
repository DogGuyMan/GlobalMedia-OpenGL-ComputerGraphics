#pragma once

#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace Controller
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
		enum class Action : int
		{
			MoveForward = 1, // W
			MoveBack,        // S
			MoveLeft,        // A
			MoveRight,       // D
			MoveUp,          // E (or Space)
			MoveDown,        // Q (or LeftCtrl)
		};

		// Actor::AddComponent<T>() 가 호출 — public default ctor 필수.
		CameraController() = default;
		CameraController(const CameraController &) = delete;
		CameraController &operator=(const CameraController &) = delete;

		bool SetUp();

		//  Builder Pattern — fluent setter (self 반환)
		/// @brief 키보드 입력 의존 주입. SetUp() 전에 호출 필수.
		CameraController &SetKeyboardInput(SJH::KeyboardInput<Action> *k);

		/// @brief 마우스 입력 의존 주입. SetUp() 전에 호출 필수.
		CameraController &SetMouseInput(SJH::MouseInput *m);

		/// @brief 제어할 Camera Component 의존 주입. SetUp() 전에 호출 필수.
		/// @note Camera 의 owner Actor 의 Transform 을 갱신. Camera 가 Actor 미부착이면 Update no-op.
		CameraController &SetCamera(SJH::Scene::Camera *c);

		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override;

	  private:
		bool mIsInitialized = false;
		SJH::MouseInput *mMouseInput = nullptr;
		SJH::KeyboardInput<Action> *mKeyboardInput = nullptr;
		SJH::Scene::Camera *mCamera = nullptr;

		float mYawDeg = 0.0f;
		float mPitchDeg = 0.0f;
		vmath::vec3 mMoveDelta = vmath::vec3(0.0f, 0.0f, 0.0f); // 매 Update reset -> held handler 누적

		float mLookSensitivity = 0.1f;
		float mMoveSpeed = 0.05f;

		void RegisterBindings();
		void UnregisterBindings();
	};
} // namespace MigrateDemo
