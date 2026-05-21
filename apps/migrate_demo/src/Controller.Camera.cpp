#include "Controller.Camera.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "object/transform.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace MigrateDemo::Controller
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
			// pitch clamp — gimbal lock 회피
			if (mPitchDeg > 89.0f)
				mPitchDeg = 89.0f;
			if (mPitchDeg < -89.0f)
				mPitchDeg = -89.0f;
		});
	}

	void CameraController::UnregisterBindings()
	{
		if (mKeyboardInput)
		{
			mKeyboardInput->UnbindKey(GLFW_KEY_W);
			mKeyboardInput->UnbindKey(GLFW_KEY_S);
			mKeyboardInput->UnbindKey(GLFW_KEY_A);
			mKeyboardInput->UnbindKey(GLFW_KEY_D);
			mKeyboardInput->UnbindKey(GLFW_KEY_E);
			mKeyboardInput->UnbindKey(GLFW_KEY_Q);
		}
		if (mMouseInput)
		{
			mMouseInput->UnbindLook();
		}
	}

	bool CameraController::SetUp()
	{
		if (!mKeyboardInput || !mMouseInput || !mCamera)
		{
			spdlog::error("CameraController::SetUp — 의존 누락 (keyboard={}, mouse={}, camera={})",
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
		// 멱등 — 두 번째 호출은 무시. nullptr 검증은 SetUp() 한 곳에서 처리.
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

		// 1) 마우스 누적 -> owner.Transform.EulerRot 갱신.
		//    EulerRot 은 degree 단위, ZYX 순서. X=pitch, Y=yaw.
		tr.EulerRot[0] = mPitchDeg;
		tr.EulerRot[1] = mYawDeg;

		// 2) 카메라 로컬 축 — Transform 의 6 방향 vector (OpenGL 오른손 정통).
		const auto right   = tr.GetRight();
		const auto up      = tr.GetUp();
		const auto forward = tr.GetForward();

		// 3) WASD/EQ 누적 -> Translate 갱신. mMoveDelta[2] 가 음수면 전진 (held handler 가 그렇게 누적).
		tr.Translate = tr.Translate
		             + right   * mMoveDelta[0]
		             + up      * mMoveDelta[1]
		             + forward * (-mMoveDelta[2]);

		// 4) 다음 프레임을 위해 누적값 리셋.
		mMoveDelta = vmath::vec3(0.0f);
	}
} // namespace MigrateDemo
