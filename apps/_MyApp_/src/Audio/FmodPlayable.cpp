#include "FmodPlayable.h"

#ifdef SJH_HAS_FMOD
#include <fmod/fmod.hpp>
#endif
#include <spdlog/spdlog.h>

namespace TopdownShooter::Audio
{
	FmodPlayable::FmodPlayable(::FMOD::System *sys, SJH::Sound *sound)
	    : mSys(sys), mSound(sound)
	{
		if (!mSys || !mSound) spdlog::warn("[FmodPlayable] ctor: sys 또는 sound nullptr");
	}

	FmodPlayable::~FmodPlayable()
	{
#ifdef SJH_HAS_FMOD
		if (mChannel) { mChannel->stop(); mChannel = nullptr; }
#endif
	}

	void FmodPlayable::Pause()
	{
		SJH::Playable::PlayableBase::Pause();
#ifdef SJH_HAS_FMOD
		if (mChannel) mChannel->setPaused(true);
#endif
	}

	void FmodPlayable::OnPlay()
	{
#ifdef SJH_HAS_FMOD
		if (!mSys || !mSound) return;
		mSys->playSound(mSound->Raw(), nullptr, /*paused=*/false, &mChannel);
#endif
	}

	void FmodPlayable::OnStop()
	{
#ifdef SJH_HAS_FMOD
		if (mChannel) { mChannel->stop(); mChannel = nullptr; }
#endif
	}

	void FmodPlayable::OnUpdate(float /*dt*/)
	{
#ifdef SJH_HAS_FMOD
		if (!mChannel) return;
		bool playing = false;
		mChannel->isPlaying(&playing);
		if (!playing && !mIsLoop) mIsFinished = true;
#else
		if (!mIsLoop) mIsFinished = true;   // FMOD 미빌드 — 즉시 종료(시퀀스 행 방지)
#endif
	}
}
