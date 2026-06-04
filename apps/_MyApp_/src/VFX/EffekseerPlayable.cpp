#include "EffekseerPlayable.h"

#include "diagnostics/effekseer_diagnostics.h"   // Effekseer 핸들 lifecycle 진단
#include "scene/actor.h"        // GetOwner() (FollowOwner 정책에서 Transform 조회)
#include <spdlog/spdlog.h>

namespace TopdownShooter::VFX
{
	EffekseerPlayable::EffekseerPlayable(::Effekseer::ManagerRef manager,
	                                     SJH::Effect            *effect,
	                                     const vmath::vec3      &spawnPos,
	                                     TrackPolicy             track,
	                                     float                   yawRad)
	    : mManager(manager), mEffect(effect), mSpawnPos(spawnPos), mTrack(track), mYaw(yawRad)
	{
		if (mManager.Get() == nullptr || mEffect == nullptr)
			spdlog::warn("[EffekseerPlayable] ctor: manager 또는 effect nullptr");
	}

	EffekseerPlayable::~EffekseerPlayable()
	{
		if (mManager.Get() != nullptr && mHandle >= 0)
		{
			mManager->StopEffect(mHandle);
			mHandle = -1;
		}
	}

	void EffekseerPlayable::StartHandle()
	{
		mHandle = mManager->Play(mEffect->Ref(),
		                         ::Effekseer::Vector3D(mSpawnPos[0], mSpawnPos[1], mSpawnPos[2]));
		// Y축 회전 주입(라디안 오일러) — 발사 방향 등. 0 이면 생략.
		if (mYaw != 0.0f && mHandle >= 0)
			mManager->SetRotation(mHandle, 0.0f, mYaw, 0.0f);
	}

	void EffekseerPlayable::OnPlay()
	{
		if (mManager.Get() == nullptr || mEffect == nullptr) return;
		StartHandle();
		// 진단 — Play 실패(-1) / Play 직후 즉시 종료(빈 이펙트·텍스처 전무) 감지.
		SJH::Diagnostics::EffekseerDiagnostics::CheckPlayHandle(mHandle, "effekseer");
		SJH::Diagnostics::EffekseerDiagnostics::CheckHandleAlive(
		    mHandle, mManager->Exists(mHandle), "effekseer");
	}

	void EffekseerPlayable::OnStop()
	{
		if (mManager.Get() != nullptr && mHandle >= 0)
		{
			mManager->StopEffect(mHandle);
			mHandle = -1;
		}
	}

	void EffekseerPlayable::OnUpdate(float /*dt*/)
	{
		if (mManager.Get() == nullptr) return;

		// 회전 — 매 프레임 yaw 갱신 (시작각 mYaw + 회전속도×경과). 궁극기 회전 레이저 등.
		if (mSpinRadPerSec != 0.0f && mHandle >= 0)
			mManager->SetRotation(mHandle, 0.0f, mYaw + mSpinRadPerSec * mElapsed, 0.0f);

		// FollowOwner — Actor 의 Transform.Translate 를 매 frame 추적
		if (mTrack == TrackPolicy::FollowOwner && mHandle >= 0)
		{
			if (auto *owner = GetOwner())
			{
				const auto &p = owner->GetTransform().Translate;
				mManager->SetLocation(mHandle, ::Effekseer::Vector3D(p[0], p[1], p[2]));
			}
		}

		// 최대 지속 경과 — 강제 종료 + finished (AutoDespawnOnFinish 가 sweep). 루프 재생보다 우선.
		if (mMaxDurationSec > 0.0f && mElapsed >= mMaxDurationSec)
		{
			if (mHandle >= 0)
			{
				mManager->StopEffect(mHandle);
				mHandle = -1;
			}
			mIsFinished = true;
			return;
		}

		// 자연 종료 처리 — Effect 가 더 이상 존재 안 함.
		if (mHandle >= 0 && !mManager->Exists(mHandle))
		{
			if (mIsLoop)
				StartHandle();          // 루프 — 재생 반복 (orbital 상시 회전 / 3초 미만 laser 반복 등)
			else
			{
				mHandle     = -1;
				mIsFinished = true;
			}
		}
	}
}
