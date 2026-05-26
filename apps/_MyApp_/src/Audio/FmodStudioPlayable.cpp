#include "FmodStudioPlayable.h"

#include <fmod/fmod_studio.hpp>
#include <spdlog/spdlog.h>

namespace TopdownShooter::Audio
{
	FmodStudioPlayable::FmodStudioPlayable(::FMOD::Studio::EventDescription *desc)
	    : mDesc(desc)
	{
		if (!mDesc) spdlog::warn("[FmodStudioPlayable] ctor: desc=nullptr (event 미존재?)");
	}

	FmodStudioPlayable::~FmodStudioPlayable()
	{
		if (mInstance)
		{
			mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
			mInstance->release();
			mInstance = nullptr;
		}
	}

	void FmodStudioPlayable::Pause()
	{
		SJH::Playable::PlayableBase::Pause();
		if (mInstance) mInstance->setPaused(true);
	}

	void FmodStudioPlayable::OnPlay()
	{
		if (!mDesc) return;
		mDesc->createInstance(&mInstance);
		if (mInstance) mInstance->start();
	}

	void FmodStudioPlayable::OnStop()
	{
		if (mInstance)
		{
			mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
			mInstance->release();
			mInstance = nullptr;
		}
	}

	void FmodStudioPlayable::OnUpdate(float /*dt*/)
	{
		if (!mInstance) return;
		FMOD_STUDIO_PLAYBACK_STATE state;
		if (mInstance->getPlaybackState(&state) == FMOD_OK
		    && state == FMOD_STUDIO_PLAYBACK_STOPPED && !isLoop_)
		{
			finished_ = true;
		}
	}
}
