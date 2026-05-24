
#include "TargetFollowableCameraController.h"
#include "input/mouse_input.h"
#include "object/transform.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include <cassert>
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::Controller
{
	void TargetFollowableCameraController::RegisterBindings()
	{
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

	void TargetFollowableCameraController::UnregisterBindings()
	{
		// SetUp 이 성공한 경우만 호출됨 (OnExit 의 mIsInitialized 가드).
		assert(this->mMouseInput != nullptr);
		mMouseInput->UnbindLook();
	}

	bool TargetFollowableCameraController::SetUp()
	{
		if (!mMouseInput || !mCamera)
		{
			spdlog::error("TargetFollowableCameraController::SetUp — 의존 누락 (mouse={}, camera={})",
			              static_cast<void *>(mMouseInput),
			              static_cast<void *>(mCamera));
			return false;
		}
		RegisterBindings();
		mIsInitialized = true;
		return true;
	}

	TargetFollowableCameraController &TargetFollowableCameraController::SetMouseInput(SJH::MouseInput *m)
	{
		if (mMouseInput == nullptr)
			mMouseInput = m;
		return *this;
	}

	TargetFollowableCameraController &TargetFollowableCameraController::SetCamera(SJH::Scene::Camera *c)
	{
		if (mCamera == nullptr)
			mCamera = c;
		return *this;
	}

	TargetFollowableCameraController &TargetFollowableCameraController::SetFollowTarget(SJH::Scene::Actor *t)
	{
		mFollowTarget = t;
		return *this;
	}

	TargetFollowableCameraController &TargetFollowableCameraController::SetFollowOffset(vmath::vec3 offset)
	{
		mFollowOffset = offset;
		return *this;
	}

	TargetFollowableCameraController &TargetFollowableCameraController::SetLookSensitivity(float v)
	{
		mLookSensitivity = v;
		return *this;
	}

	void TargetFollowableCameraController::OnEnter()
	{
		if (!mIsInitialized)
			return;
	}

	void TargetFollowableCameraController::OnExit()
	{
		if (!mIsInitialized)
			return;
		UnregisterBindings();
		mMouseInput    = nullptr;
		mCamera        = nullptr;
		mFollowTarget  = nullptr;
		mIsInitialized = false;
	}

	void TargetFollowableCameraController::Update(float dt)
	{
		(void)dt;
		if (!mIsInitialized)
			return;
		if (!mFollowTarget)
			return; // follow target 미설정 — no-op (no free-fly fallback by design)

		// Camera Component 가 Actor 미부착이면 Transform 갱신 대상 없음.
		auto *owner = mCamera->GetOwner();
		if (!owner)
			return;
		auto &tr = owner->GetTransform();

		// target 위치 + offset 으로 카메라 위치 갱신.
		const auto &targetTr = mFollowTarget->GetTransform();
		tr.Translate         = targetTr.Translate + mFollowOffset;

		// Mouse 누적 yaw/pitch -> EulerRot (target 머리 위 상대 회전).
		tr.EulerRot[0] = mPitchDeg;
		tr.EulerRot[1] = mYawDeg;
	}
} // namespace TopdownShooter::Controller
