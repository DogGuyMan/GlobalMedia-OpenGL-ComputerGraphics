/**
 * @file CameraController.cpp
 * @brief CameraController 구현 - 키 held 누적 이동 + 마우스 look 누적 회전.
 *
 * @details
 *  ### 바인딩 패턴
 *  - @c BindKey 로 Action <-> GLFW 키 매핑, @c BindHeldHandler 로 매 프레임 mMoveDelta 누적.
 *  - 마우스 look 핸들러는 yaw/pitch 누적, pitch 는 gimbal lock 회피로 클램프.
 *
 *  ### Update 흐름
 *  1. 회전 누적값을 owner Transform.EulerRot 에 반영.
 *  2. 카메라 로컬 축(right/up/forward) 으로 mMoveDelta 투영해 Translate 갱신.
 *  3. mMoveDelta 리셋 (다음 프레임 held 누적 준비).
 */

#include "CameraController.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "object/transform.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include <GLFW/glfw3.h>
#include <cassert>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

namespace
{
	constexpr float kCameraPitchClampDeg = 89.0f; // gimbal lock 회피 pitch 상한(+/-)
}

namespace TopdownShooter::Controller
{
	void CameraController::RegisterBindings()
	{
		mKeyboardInput->BindKey(Action::MoveForward, GLFW_KEY_W);
		mKeyboardInput->BindKey(Action::MoveBack, GLFW_KEY_S);
		mKeyboardInput->BindKey(Action::MoveLeft, GLFW_KEY_A);
		mKeyboardInput->BindKey(Action::MoveRight, GLFW_KEY_D);
		mKeyboardInput->BindKey(Action::MoveUp, GLFW_KEY_E);
		mKeyboardInput->BindKey(Action::MoveDown, GLFW_KEY_Q);

		mKeyboardInput->BindHeldHandler(Action::MoveForward, [this] { mMoveDelta[2] -= mMoveSpeed; });
		mKeyboardInput->BindHeldHandler(Action::MoveBack, [this] { mMoveDelta[2] += mMoveSpeed; });
		mKeyboardInput->BindHeldHandler(Action::MoveLeft, [this] { mMoveDelta[0] -= mMoveSpeed; });
		mKeyboardInput->BindHeldHandler(Action::MoveRight, [this] { mMoveDelta[0] += mMoveSpeed; });
		mKeyboardInput->BindHeldHandler(Action::MoveUp, [this] { mMoveDelta[1] += mMoveSpeed; });
		mKeyboardInput->BindHeldHandler(Action::MoveDown, [this] { mMoveDelta[1] -= mMoveSpeed; });

		// MouseInput::BindLookHandler 시그니처는 std::function<void(double, double)>.
		mMouseInput->BindLookHandler([this](double dx, double dy) {
			mYawDeg -= static_cast<float>(dx) * mLookSensitivity;
			mPitchDeg -= static_cast<float>(dy) * mLookSensitivity;
			// pitch clamp - gimbal lock 회피
			if (mPitchDeg > kCameraPitchClampDeg)
				mPitchDeg = kCameraPitchClampDeg;
			if (mPitchDeg < -kCameraPitchClampDeg)
				mPitchDeg = -kCameraPitchClampDeg;
		});
	}

	void CameraController::UnregisterBindings()
	{
		// SetUp 이 성공한 경우만 호출됨 (OnExit 의 mIsInitialized 가드) - 입력 의존은 non-null 보장.
		assert(this->mKeyboardInput != nullptr);
		assert(this->mMouseInput != nullptr);

		mKeyboardInput->UnbindKey(GLFW_KEY_W);
		mKeyboardInput->UnbindKey(GLFW_KEY_S);
		mKeyboardInput->UnbindKey(GLFW_KEY_A);
		mKeyboardInput->UnbindKey(GLFW_KEY_D);
		mKeyboardInput->UnbindKey(GLFW_KEY_E);
		mKeyboardInput->UnbindKey(GLFW_KEY_Q);
		mMouseInput->UnbindLook();
	}

	bool CameraController::SetUp()
	{
		if (!mKeyboardInput || !mMouseInput || !mCamera)
		{
			spdlog::error("CameraController::SetUp - 의존 누락 (keyboard={}, mouse={}, camera={})",
			              static_cast<void *>(mKeyboardInput),
			              static_cast<void *>(mMouseInput),
			              static_cast<void *>(mCamera));
			return false;
		}
		RegisterBindings();
		mIsInitialized = true;
		return true;
	}

	CameraController &CameraController::SetKeyboardInput(SJH::KeyboardInput<Action> *k)
	{
		// 멱등 - 두 번째 호출은 무시. nullptr 검증은 SetUp() 한 곳에서 처리.
		if (mKeyboardInput == nullptr)
			mKeyboardInput = k;
		return *this;
	}

	CameraController &CameraController::SetMouseInput(SJH::MouseInput *m)
	{
		if (mMouseInput == nullptr)
			mMouseInput = m;
		return *this;
	}

	CameraController &CameraController::SetCamera(SJH::Scene::Camera *c)
	{
		if (mCamera == nullptr)
			mCamera = c;
		return *this;
	}

	void CameraController::OnEnter()
	{
		if (!mIsInitialized)
			return;
	}

	void CameraController::OnExit()
	{
		if (!mIsInitialized)
			return;
		UnregisterBindings();
		mMouseInput = nullptr;
		mKeyboardInput = nullptr;
		mCamera = nullptr;
		mIsInitialized = false;
	}

	void CameraController::Update(float dt)
	{
		(void)dt; // mMoveSpeed 가 *프레임당* 이라 dt 미사용. real dt 적용은 후속 개선.
		if (!mIsInitialized)
			return;

		// Camera Component 가 Actor 에 부착되지 않았으면 Transform 갱신 대상 없음.
		auto *owner = mCamera->GetOwner();
		if (!owner)
			return;
		auto &tr = owner->GetTransform();

		tr.EulerRot[0] = mPitchDeg;
		tr.EulerRot[1] = mYawDeg;

		const auto right   = tr.GetRight();
		const auto up      = tr.GetUp();
		const auto forward = tr.GetForward();

		tr.Translate = tr.Translate
		             + right   * mMoveDelta[0]
		             + up      * mMoveDelta[1]
		             + forward * (-mMoveDelta[2]);

		// 누적값 리셋.
		mMoveDelta = glm::vec3(0.0f);
	}
} // namespace TopdownShooter