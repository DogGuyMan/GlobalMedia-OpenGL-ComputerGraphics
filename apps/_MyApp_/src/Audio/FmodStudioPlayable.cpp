/**
 * @file FmodStudioPlayable.cpp
 * @brief FmodStudioPlayable -- FMOD Studio EventInstance 생명주기 구현.
 *
 * @details
 *  ### 책임
 *  - @c OnPlay: 이전 @c mInstance 정리 -> @c createInstance -> (3D) @c set3DAttributes -> @c start.
 *  - @c OnStop / dtor: @c stop(FMOD_STUDIO_STOP_IMMEDIATE) + @c release 로 즉시 정리.
 *  - @c OnUpdate: @c getPlaybackState 폴링 -- @c FMOD_STUDIO_PLAYBACK_STOPPED 감지 시 @c mIsFinished.
 *  - @c SetParameter: @c mInstance->setParameterByName 래퍼 (BGM_STATE 전환).
 *  - @c SetPaused: @c mInstance->setPaused 직접 토글 (PlayableBase 상태 비연동).
 *
 *  ### SJH_HAS_FMOD 가드 구조
 *  FMOD API 를 호출하는 모든 경로는 @c #ifdef SJH_HAS_FMOD 블록 안에만 존재한다.
 *  FMOD SDK 미설치 환경에서는 대부분 no-op 이 되며, @c OnUpdate 에서만 비루프 시 즉시
 *  @c mIsFinished = true 로 처리해 SequencePlayable 의 다음 단계 진행을 보장한다(행 방지).
 *
 * @note
 *  - @c EventDescription 은 Bank 소유. 본 구현에서 @c mDesc->release() 를 호출하지 않는다.
 *  - @c mInstance 는 @c ~FmodStudioPlayable / @c OnStop 에서 반드시 정리.
 *    @c OnPlay 재진입 시에도 선행 정리(stop+release)를 수행해 instance 중복 적재를 방지.
 */
#include "FmodStudioPlayable.h"

#ifdef SJH_HAS_FMOD
#include <fmod/fmod_common.h>
#include <fmod/fmod_studio.hpp>
#endif
#include <spdlog/spdlog.h>
#include <optional>
#include <string>
#include <glm/glm.hpp>

namespace TopdownShooter::Audio
{
	/// @note @p desc=nullptr 이면 경고만 출력 후 no-op 상태로 유지.
	///       @c SpawnAudioInstance 는 이 경우를 정상 경로로 허용 (bank 누락 시 게임 진행 보장).
	FmodStudioPlayable::FmodStudioPlayable(::FMOD::Studio::EventDescription *desc,
	                                       std::optional<glm::vec3> worldPos)
	    : mDesc(desc), mWorldPos(worldPos)
	{
		if (!mDesc) spdlog::warn("[FmodStudioPlayable] ctor: desc=nullptr (event 미존재?)");
	}

	/// @note Actor 가 씬에서 제거될 때 dtor 가 호출된다.
	///       @c stop(FMOD_STUDIO_STOP_IMMEDIATE) 로 fade-out 없이 즉시 정지 후 release.
	///       @c mDesc 는 Bank 소유이므로 건드리지 않는다.
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

	/// @note 부모 @c Pause() 가 @c mPaused = true 를 설정한 뒤 FMOD 도 일시정지.
	///       재개는 @c PlayableBase::Resume() 에서 처리해야 한다(현재 override 없음 -- 필요 시 추가).
	void FmodStudioPlayable::Pause()
	{
		SJH::Playable::PlayableBase::Pause();
#ifdef SJH_HAS_FMOD
		if (mInstance) mInstance->setPaused(true);
#endif
	}

	/// @note @c mInstance 가 nullptr (재생 전/후)이면 silently no-op.
	///       BGM_STATE 파라미터는 event-instance scope -- System 이 아닌 instance 에 설정.
	///       Global 파라미터라면 @c AudioSystem::mStudioSystem->setParameterByName 을 써야 한다.
	void FmodStudioPlayable::SetParameter(const std::string &name, float value)
	{
#ifdef SJH_HAS_FMOD
		if (mInstance) mInstance->setParameterByName(name.c_str(), value);
#else
		(void)name;
		(void)value;
#endif
	}

	/// @note @c Pause() 와 달리 @c mPaused 를 건드리지 않으므로 Stage FSM 상태 복귀 시
	///       PlayableBase 계층이 의도치 않게 PAUSED 상태로 굳어지는 것을 방지한다.
	void FmodStudioPlayable::SetPaused(bool paused)
	{
#ifdef SJH_HAS_FMOD
		if (mInstance) mInstance->setPaused(paused);
#else
		(void)paused;
#endif
	}

	/// @note @c mWorldPos 가 있으면 @c set3DAttributes 로 리스너(카메라) 기준 panning/감쇠 적용.
	///       forward={0,0,-1}, up={0,1,0} 은 RH 좌표계 기본값 -- 탑다운 고정 시점에 적합.
	///       velocity=0 -> 도플러 효과 없음.
	void FmodStudioPlayable::OnPlay()
	{
#ifdef SJH_HAS_FMOD
		if (!mDesc) return;
		// 이미 재생 중이면 먼저 정리 -- Play() 재호출 시 instance 가 겹쳐 쌓이는 것 방지.
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

	/// @note @c Stop() -> @c OnStop 경로. 즉시 정지 + release 는 dtor 와 동일한 규칙.
	///       이후 @c OnPlay 재진입 시 새 instance 가 생성되므로 restart 가 가능하다.
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

	/// @note @c FMOD_STUDIO_PLAYBACK_STATE: PLAYING / SUSTAINING / STOPPED / STARTING / STOPPING.
	///       STOPPED 상태를 감지해야 비루프 one-shot SFX 의 @c IsFinished 를 올바르게 마킹할 수 있다.
	///       SUSTAINING 상태(sustain point 에서 대기 중) 는 STOPPED 가 아니므로 종료로 오판하지 않는다.
	///       루프 이벤트(BGM)는 @c mIsLoop=true 이므로 이 분기에 진입하지 않는다.
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
		if (!mIsLoop) mIsFinished = true;   // FMOD 미빌드 -- 즉시 종료(시퀀스 행 방지)
#endif
	}
}
