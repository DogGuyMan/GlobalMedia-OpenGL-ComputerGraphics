#include "FmodStudioPlayable.h"

#include <fmod/fmod_common.h>
#include <fmod/fmod_studio.hpp>
#include <spdlog/spdlog.h>
#include <optional>
#include <vmath.h>

namespace TopdownShooter::Audio
{
	FmodStudioPlayable::FmodStudioPlayable(::FMOD::Studio::EventDescription *desc,
	                                       std::optional<vmath::vec3> worldPos)
	    : mDesc(desc), mWorldPos(worldPos)
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
		if (mInstance)
		{
			if (mWorldPos)
			{
				FMOD_3D_ATTRIBUTES attr = {};
				attr.position = {(*mWorldPos)[0], (*mWorldPos)[1], (*mWorldPos)[2]};
				attr.forward  = {0.0f, 0.0f, -1.0f};
				attr.up       = {0.0f, 1.0f, 0.0f};
				mInstance->set3DAttributes(&attr);
			}
			mInstance->start();
		}
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
		    && state == FMOD_STUDIO_PLAYBACK_STOPPED && !mIsLoop)
		{
			mIsFinished = true;
		}
	}
}
