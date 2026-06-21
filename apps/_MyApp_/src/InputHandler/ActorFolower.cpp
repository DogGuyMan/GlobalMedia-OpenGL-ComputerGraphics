/**
 * @file ActorFolower.cpp
 * @brief ActorFolower 구현 - dt 기반 ease 보간 추종 + 마우스 look 누적.
 *
 * @details
 *  ### 추종 흐름 (Update)
 *  1. target 위치 + offset 으로 이번 프레임 goal 계산.
 *  2. 첫 프레임은 보간 없이 즉시 스냅 (먼 초기 위치에서의 스월-인 방지).
 *  3. goal 이 EPS 넘게 이동하면 현재 카메라 위치에서 새 ease 구간 시작 (튐 제거).
 *  4. dt 로 진행도를 올리고 생성함수 커브로 보간, 도착하면 goal 로 스냅 (떨림 제거).
 *
 * @note 마우스 look 은 yaw/pitch 만 누적하며 위치 추종과 독립.
 */

#include "ActorFolower.h"
#include "input/mouse_input.h"
#include "object/transform.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include <algorithm>
#include <cassert>
#include <spdlog/spdlog.h>
#include <tweeny/easing.h>
#include <utility>
#include <glm/glm.hpp>

namespace TopdownShooter::Controller
{
	namespace
	{
		// 생성함수 커브로 vec3 를 축별 보간 (t: [0,1] 진행도).
		glm::vec3 EaseVec3(const ActorFolower::EaseFn &fn, float t, const glm::vec3 &from, const glm::vec3 &to)
		{
			return glm::vec3(fn(t, from[0], to[0]),
			                   fn(t, from[1], to[1]),
			                   fn(t, from[2], to[2]));
		}
	} // namespace

	void ActorFolower::RegisterBindings()
	{
		// MouseInput::BindLookHandler 시그니처는 std::function<void(double, double)>.
		mMouseInput->BindLookHandler([this](double dx, double dy) {
			mYawDeg -= static_cast<float>(dx) * mLookSensitivity;
			mPitchDeg -= static_cast<float>(dy) * mLookSensitivity;
			// pitch clamp - gimbal lock 회피
			if (mPitchDeg > 89.0f)
				mPitchDeg = 89.0f;
			if (mPitchDeg < -89.0f)
				mPitchDeg = -89.0f;
		});
	}

	void ActorFolower::UnregisterBindings()
	{
		// SetUp 이 성공한 경우만 호출됨 (OnExit 의 mIsInitialized 가드).
		assert(this->mMouseInput != nullptr);
		mMouseInput->UnbindLook();
	}

	bool ActorFolower::SetUp()
	{
		if (!mMouseInput || !mCamera)
		{
			spdlog::error("ActorFolower::SetUp - 의존 누락 (mouse={}, camera={})",
			              static_cast<void *>(mMouseInput),
			              static_cast<void *>(mCamera));
			return false;
		}
		// 보간 커브 미주입 시 default - cubicOut (빠르게 출발 후 목표에서 감속, follow 정통).
		if (!mEaseFn)
			mEaseFn = [](float p, float a, float b) { return tweeny::easing::cubicOut.run(p, a, b); };
		RegisterBindings();
		mIsInitialized = true;
		return true;
	}

	ActorFolower &ActorFolower::SetMouseInput(SJH::MouseInput *m)
	{
		if (mMouseInput == nullptr)
			mMouseInput = m;
		return *this;
	}

	ActorFolower &ActorFolower::SetCamera(SJH::Scene::Camera *c)
	{
		if (mCamera == nullptr)
			mCamera = c;
		return *this;
	}

	ActorFolower &ActorFolower::SetFollowTarget(SJH::Scene::Actor *t)
	{
		mFollowTarget = t;
		return *this;
	}

	ActorFolower &ActorFolower::SetFollowOffset(glm::vec3 offset)
	{
		mFollowOffset = offset;
		return *this;
	}
	ActorFolower &ActorFolower::SetFollowRotate(glm::vec2 rot)
	{
		mYawDeg = rot[0];
		mPitchDeg = rot[1];
		return *this;
	}

	ActorFolower &ActorFolower::SetLookSensitivity(float v)
	{
		mLookSensitivity = v;
		return *this;
	}

	ActorFolower &ActorFolower::SetEaseFunction(EaseFn fn)
	{
		if (fn) // 빈 함수는 무시 - Update 에서 null 호출 방지
			mEaseFn = std::move(fn);
		return *this;
	}

	ActorFolower &ActorFolower::SetFollowDuration(float seconds)
	{
		if (seconds > 0.0f) // 0/음수 -> 0 나눗셈/역진행 방지
			mFollowDuration = seconds;
		return *this;
	}

	ActorFolower &ActorFolower::SetArriveEps(float eps)
	{
		if (eps > 0.0f)
			mArriveEps = eps;
		return *this;
	}

	void ActorFolower::OnEnter()
	{
		if (!mIsInitialized)
			return;
	}

	void ActorFolower::OnExit()
	{
		if (!mIsInitialized)
			return;
		UnregisterBindings();
		mMouseInput    = nullptr;
		mCamera        = nullptr;
		mFollowTarget  = nullptr;
		mIsInitialized = false;
		// 보간 상태 리셋 - 재진입 시 첫 프레임 즉시 스냅.
		mHasGoal = false;
	}
	
	// CLAUDE ASSIST
	void ActorFolower::Update(float dt)
	{
		if (!mIsInitialized)
			return;
		if (!mFollowTarget)
			return; // follow target 미설정 - no-op (no free-fly fallback by design)

		// Camera Component 가 Actor 미부착이면 Transform 갱신 대상 없음.
		auto *owner = mCamera->GetOwner();
		if (!owner)
			return;
		auto &tr               = owner->GetTransform();
		const glm::vec3 goal = mFollowTarget->GetTransform().Translate + mFollowOffset; // 이번 프레임 목표

		// 첫 프레임: 보간 없이 즉시 정렬 - 먼 초기 위치에서의 스월-인 방지.
		if (!mHasGoal)
		{
			tr.Translate = goal;
			mEaseStart   = goal;
			mEaseGoal    = goal;
			mHasGoal     = true;
			return;
		}

		// 목표가 EPS 넘게 이동하면 *현재 카메라 위치*에서 새 ease 구간 시작 (이어붙여 튐 제거).
		const bool targetMoved = glm::length(goal - mEaseGoal) > mArriveEps;
		if (targetMoved)
		{
			mEaseStart    = tr.Translate;
			mEaseGoal     = goal;
			mEaseProgress = 0.0f;
		}

		// dt 기반 진행(fps independent) 후 생성함수 커브로 보간.
		mEaseProgress          = std::min(1.0f, mEaseProgress + dt / mFollowDuration);
		const glm::vec3 next = EaseVec3(mEaseFn, mEaseProgress, mEaseStart, mEaseGoal);

		// progress 완료거나 목표와 EPS 이내면 스냅(잔여 크롤/부동소수 노이즈 = 떨림 제거), 아니면 진행.
		const bool arrived = mEaseProgress >= 1.0f || glm::length(mEaseGoal - next) <= mArriveEps;
		tr.Translate       = arrived ? mEaseGoal : next;
	}
} // namespace TopdownShooter::Controller
