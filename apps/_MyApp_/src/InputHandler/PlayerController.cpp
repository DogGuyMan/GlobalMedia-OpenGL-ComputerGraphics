
#include "PlayerController.h"
#include "input/keyboard_input.h"
#include "object/transform.h"
#include "scene/actor.h"
#include <GLFW/glfw3.h>
#include <cassert>
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::Controller
{
	void PlayerController::RegisterBindings()
	{
		// CameraController.cpp 의 BindKey + BindHeldHandler 2단 패턴을 그대로 모방.
		mKeyboardInput->BindKey(Action::MoveForward, GLFW_KEY_W);
		mKeyboardInput->BindKey(Action::MoveBack, GLFW_KEY_S);
		mKeyboardInput->BindKey(Action::MoveLeft, GLFW_KEY_A);
		mKeyboardInput->BindKey(Action::MoveRight, GLFW_KEY_D);

		// W = 앞 = -Z (OpenGL forward 컨벤션). held handler 가 mMoveSpeed 직접 곱.
		mKeyboardInput->BindHeldHandler(Action::MoveForward, [this] { mInputValue = {0.0f, 0.0f, 1.0f}; });
		mKeyboardInput->BindHeldHandler(Action::MoveBack, [this] { mInputValue ={0.0f, 0.0f, -1.0f}; });
		mKeyboardInput->BindHeldHandler(Action::MoveLeft, [this] { mInputValue ={-1.0f, 0.0f, 0.0f}; });
		mKeyboardInput->BindHeldHandler(Action::MoveRight, [this] { mInputValue ={1.0f, 0.0f, 1.0f}; });
	}

	void PlayerController::UnregisterBindings()
	{
		// SetUp 이 성공한 경우만 호출됨 (OnExit 의 mIsInitialized 가드) — 입력 의존은 non-null 보장.
		assert(this->mKeyboardInput != nullptr);

		mKeyboardInput->UnbindKey(GLFW_KEY_W);
		mKeyboardInput->UnbindKey(GLFW_KEY_S);
		mKeyboardInput->UnbindKey(GLFW_KEY_A);
		mKeyboardInput->UnbindKey(GLFW_KEY_D);
	}

	bool PlayerController::SetUp()
	{
		if (!mKeyboardInput)
		{
			spdlog::error("PlayerController::SetUp — KeyboardInput 미주입");
			return false;
		}
		RegisterBindings();
		mIsInitialized = true;
		return true;
	}

	PlayerController &PlayerController::SetKeyboardInput(SJH::KeyboardInput<Action> *k)
	{
		// 멱등 — 두 번째 호출은 무시. nullptr 검증은 SetUp() 한 곳에서.
		if (mKeyboardInput == nullptr)
			mKeyboardInput = k;
		return *this;
	}

	PlayerController &PlayerController::SetPlayerMovement(Entity::Components::Movement* m)
	{
		if(mMovementPtr == nullptr)
			mMovementPtr = m;
		return *this;
	}

	void PlayerController::OnEnter()
	{
		if (!mIsInitialized)
			return;
	}

	void PlayerController::OnExit()
	{
		if (!mIsInitialized)
			return;
		UnregisterBindings();
		mKeyboardInput = nullptr;
		mIsInitialized = false;
	}

	void PlayerController::Update(float dt)
	{
		(void)dt; // mMoveSpeed 가 *프레임당* — CameraController 와 동일. real dt 적용은 후속.
		if (!mIsInitialized)
			return;
		mMovementPtr->DoForward({mInputValue[0], mInputValue[2]});
		// 누적값 리셋.
		mInputValue = vmath::vec3(0.0f);
	}
} // namespace TopdownShooter::Controller
