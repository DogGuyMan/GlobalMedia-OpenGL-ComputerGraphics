#include "FmodStudioPlayable.h"

#ifdef SJH_HAS_FMOD
#include <fmod/fmod_common.h>
#include <fmod/fmod_studio.hpp>
#endif
#include <spdlog/spdlog.h>
#include <optional>
#include <string>
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
#ifdef SJH_HAS_FMOD
		if (mInstance)
		{
			mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
			mInstance->release();
			mInstance = nullptr;
		}
#endif
	}

	void FmodStudioPlayable::Pause()
	{
		SJH::Playable::PlayableBase::Pause();
#ifdef SJH_HAS_FMOD
		if (mInstance) mInstance->setPaused(true);
#endif
	}

	void FmodStudioPlayable::SetParameter(const std::string &name, float value)
	{
#ifdef SJH_HAS_FMOD
		if (mInstance) mInstance->setParameterByName(name.c_str(), value);
#else
		(void)name;
		(void)value;
#endif
	}

	void FmodStudioPlayable::SetPaused(bool paused)
	{
#ifdef SJH_HAS_FMOD
		if (mInstance) mInstance->setPaused(paused);
#else
		(void)paused;
#endif
	}

	void FmodStudioPlayable::OnPlay()
	{
#ifdef SJH_HAS_FMOD
		if (!mDesc) return;
		// 이미 재생 중이면 먼저 정리 — Play() 재호출 시 instance 가 겹쳐 쌓이는 것 방지.
		if (mInstance)
		{
			mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
			mInstance->release();
			mInstance = nullptr;
		}
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
#endif
	}

	void FmodStudioPlayable::OnStop()
	{
#ifdef SJH_HAS_FMOD
		if (mInstance)
		{
			mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
			mInstance->release();
			mInstance = nullptr;
		}
#endif
	}

	void FmodStudioPlayable::OnUpdate(float /*dt*/)
	{
#ifdef SJH_HAS_FMOD
		if (!mInstance) return;
		FMOD_STUDIO_PLAYBACK_STATE state;
		if (mInstance->getPlaybackState(&state) == FMOD_OK
		    && state == FMOD_STUDIO_PLAYBACK_STOPPED && !mIsLoop)
		{
			mIsFinished = true;
		}
#else
		if (!mIsLoop) mIsFinished = true;   // FMOD 미빌드 — 즉시 종료(시퀀스 행 방지)
#endif
	}
}
