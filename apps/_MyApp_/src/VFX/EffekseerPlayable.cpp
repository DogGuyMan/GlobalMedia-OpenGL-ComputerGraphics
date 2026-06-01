#include "EffekseerPlayable.h"

#include "diagnostics/effekseer_diagnostics.h"   // Effekseer 핸들 lifecycle 진단
#include "scene/actor.h"        // GetOwner() (FollowOwner 정책에서 Transform 조회)
#include <spdlog/spdlog.h>

namespace TopdownShooter::VFX
{
	EffekseerPlayable::EffekseerPlayable(::Effekseer::ManagerRef manager,
	                                     SJH::Effect            *effect,
	                                     const vmath::vec3      &spawnPos,
	                                     TrackPolicy             track)
	    : mManager(manager), mEffect(effect), mSpawnPos(spawnPos), mTrack(track)
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

	void EffekseerPlayable::OnPlay()
	{
		if (mManager.Get() == nullptr || mEffect == nullptr) return;
		mHandle = mManager->Play(mEffect->Ref(),
		                         ::Effekseer::Vector3D(mSpawnPos[0], mSpawnPos[1], mSpawnPos[2]));
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

		// FollowOwner — Actor 의 Transform.Translate 를 매 frame 추적
		if (mTrack == TrackPolicy::FollowOwner && mHandle >= 0)
		{
			if (auto *owner = GetOwner())
			{
				const auto &p = owner->GetTransform().Translate;
				mManager->SetLocation(mHandle, ::Effekseer::Vector3D(p[0], p[1], p[2]));
			}
		}

		// 자연 종료 — Effect 가 더 이상 존재 안 하면 finished
		if (mHandle >= 0 && !mManager->Exists(mHandle) && !isLoop_)
		{
			mHandle   = -1;
			finished_ = true;
		}
	}
}
