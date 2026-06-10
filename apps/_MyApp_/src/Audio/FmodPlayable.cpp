/**
 * @file FmodPlayable.cpp
 * @brief FmodPlayable 구현 - Core playSound/Channel 로 .wav/.ogg 재생 + 종료 폴링.
 *
 * @details 모든 FMOD 호출은 @c SJH_HAS_FMOD 가드 안에 있다. 미빌드 환경에서는 비루프 시 즉시
 *  @c IsFinished 로 마킹해 상위 시퀀스가 무한 대기하지 않게 한다.
 */
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
		mChannel->isPlaying(&playing);                  // 종료 폴링 - 끝나면 FMOD 가 Channel 재활용
		if (!playing && !mIsLoop) mIsFinished = true;   // 비루프 + 재생 종료 -> Playable 완료 마킹
#else
		if (!mIsLoop) mIsFinished = true;   // FMOD 미빌드 — 즉시 종료(시퀀스 행 방지)
#endif
	}
}
