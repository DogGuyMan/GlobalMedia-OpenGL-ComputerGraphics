#include "FmodPlayable.h"

#include <fmod/fmod.hpp>
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
		if (mChannel) { mChannel->stop(); mChannel = nullptr; }
	}

	void FmodPlayable::Pause()
	{
		SJH::Playable::PlayableBase::Pause();
		if (mChannel) mChannel->setPaused(true);
	}

	void FmodPlayable::OnPlay()
	{
		if (!mSys || !mSound) return;
		mSys->playSound(mSound->Raw(), nullptr, /*paused=*/false, &mChannel);
	}

	void FmodPlayable::OnStop()
	{
		if (mChannel) { mChannel->stop(); mChannel = nullptr; }
	}

	void FmodPlayable::OnUpdate(float /*dt*/)
	{
		if (!mChannel) return;
		bool playing = false;
		mChannel->isPlaying(&playing);
		if (!playing && !mIsLoop) mIsFinished = true;
	}
}
