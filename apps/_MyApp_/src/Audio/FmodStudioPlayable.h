/**
 * @file FmodStudioPlayable.h
 * @brief FMOD Studio EventInstance 를 @c SJH::Playable::PlayableBase leaf 로 감싼 컴포넌트.
 *
 * @details
 *  ### 책임
 *  - @c EventDescription (외부 owner) 으로부터 @c EventInstance 를 생성/소유/해제.
 *  - @c Play/Pause/Stop 인터페이스를 통해 이벤트 재생 생명주기 관리.
 *  - 3D 공간음향 지원 -- @p worldPos 가 있으면 @c set3DAttributes 로 이벤트 위치 설정.
 *  - @c OnUpdate 마다 @c getPlaybackState 로 재생 종료를 폴링해 @c mIsFinished 마킹.
 *  - FMOD Studio 파라미터 실시간 변경 (@c SetParameter -- BGM_STATE 전환 등).
 *
 *  ### 비-책임
 *  - [X] EventDescription 소유/해제 -- AudioSystem (캐시 owner) 책임.
 *  - [X] FMOD System 초기화/update/release -- AudioSystem 책임.
 *  - [X] Bank 로드/언로드 -- AudioWarmup + AudioSystem 책임.
 *  - [X] Listener 갱신 -- AudioSystem::SetListener 책임.
 *
 *  ### 정통 매핑
 *  - Unreal `UAudioComponent` -- 월드 위치 + 파라미터를 가진 SFX 컴포넌트.
 *  - Cocos2D `AudioEngine` one-shot -- @c SpawnAudioInstance 의 create->start->release 패턴.
 *
 *  ### SJH_HAS_FMOD 옵셔널 컴파일 가드
 *  본 .cpp 내 FMOD API 호출은 전부 @c #ifdef SJH_HAS_FMOD 블록 안에 있다.
 *  FMOD SDK 가 없는 빌드 환경에서는 헤더의 전방선언(@c FMOD::Studio::EventDescription/EventInstance)
 *  + 빈 구현으로 컴파일되어 @c SJH::engine 우산을 link 하는 모든 소비자가 FMOD 없이도
 *  빌드 가능하다. 단 @c OnUpdate 에서 @c !mIsLoop 이면 즉시 @c mIsFinished = true 처리 --
 *  FMOD 미빌드 환경에서도 SequencePlayable 이 다음 단계로 진행될 수 있도록 행(hang) 방지.
 *
 * @note
 *  - @c mInstance 멤버에 @c [[maybe_unused]] 가 붙어 있다. FMOD 미빌드 시 @c #ifdef 로
 *    사용처가 모두 제거되어 clang @c -Werror=unused-private-field 경고를 막기 위함 -- 의도된 수식자.
 *  - @c ~FmodStudioPlayable 는 @c stop(FMOD_STUDIO_STOP_IMMEDIATE) + @c release() 로
 *    인스턴스를 즉시 정리한다. @c EventDescription 은 Bank 소유이므로 포인터만 보유, release 금지.
 */
#ifndef _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__
#define _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__

#include "playable/playable_base.h"
#include <optional>
#include <string>
#include <vmath.h>

namespace FMOD::Studio { class EventDescription; class EventInstance; }

namespace TopdownShooter::Audio
{
	/**
	 * @brief FMOD Studio @c EventInstance 를 소유/재생/정리하는 leaf Playable 컴포넌트 (M5 T3).
	 * @details
	 *  소유 모델:
	 *  - @c mDesc (EventDescription) -- Bank 소유. 본 클래스는 *빌려 쓰기만* (release 금지).
	 *  - @c mInstance (EventInstance) -- 본 클래스가 소유. dtor/@c OnStop 에서 반드시 release.
	 *
	 *  재생 흐름:
	 *  1. @c OnPlay -- 이전 instance 정리 -> @c createInstance -> (3D 이면) @c set3DAttributes -> @c start.
	 *  2. @c OnUpdate -- @c getPlaybackState 폴링, @c FMOD_STUDIO_PLAYBACK_STOPPED 감지 ->
	 *     비루프(@p !mIsLoop) 이면 @c mIsFinished = true.
	 *  3. @c OnStop / dtor -- @c stop(FMOD_STUDIO_STOP_IMMEDIATE) + @c release.
	 *
	 *  루프 이벤트(BGM 등)는 @c SetIsLoop(true) 를 선행 호출해야 @c OnUpdate 가 종료로
	 *  오판하지 않는다.
	 */
	class FmodStudioPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		/// @brief 생성자.
		/// @param desc     재생할 이벤트의 템플릿. Bank 소유이므로 release 하지 않는다.
		///                 nullptr 이면 warn 후 no-op (안전 -- @c SpawnAudioInstance 의 null 가드).
		/// @param worldPos 3D 공간음향 위치. @c std::nullopt 이면 2D 이벤트로 재생.
		explicit FmodStudioPlayable(::FMOD::Studio::EventDescription *desc,
		                            std::optional<vmath::vec3> worldPos = std::nullopt);

		/// @brief 소멸자. @c mInstance 가 살아있으면 @c stop(FMOD_STUDIO_STOP_IMMEDIATE) + @c release.
		~FmodStudioPlayable() override;

		/// @brief PlayableBase::Pause 를 호출한 뒤 @c mInstance->setPaused(true) 로 FMOD 도 일시정지.
		void Pause() override;

		/// @brief 재생 중인 @c mInstance 의 파라미터를 이름으로 설정한다.
		/// @details FMOD @c setParameterByName 래퍼.
		///          BGM_STATE (Title=0 / Combat=1) 전환 -- Stage FSM 상태 전환 시 호출.
		///          @c mInstance 가 없으면 no-op.
		/// @param name  이벤트 파라미터 이름 (대소문자 구분, Studio 저작 도구와 일치해야 함).
		/// @param value 설정할 float 값.
		void SetParameter(const std::string &name, float value);

		/// @brief @c mInstance 의 일시정지 상태를 직접 토글한다.
		/// @details @c Pause() 와 달리 @c PlayableBase::mPaused 상태는 갱신하지 않는다.
		///          Stage FSM 의 Pause State 일시정지 <-> Combat 복귀 resume 전용.
		/// @param paused true 이면 일시정지, false 이면 재개.
		void SetPaused(bool paused);

	  protected:
		/// @brief PlayableBase hook -- 이전 instance 정리 후 새 instance 를 생성/시작한다.
		/// @details
		///  순서: stop+release 이전 instance -> @c createInstance -> (worldPos 있으면) @c set3DAttributes
		///  -> @c start. @c mDesc 가 nullptr 이면 즉시 return.
		void OnPlay() override;

		/// @brief PlayableBase hook -- @c mInstance 를 즉시 정지(@c FMOD_STUDIO_STOP_IMMEDIATE) + release.
		void OnStop() override;

		/// @brief PlayableBase hook -- @c getPlaybackState 폴링.
		/// @details @c FMOD_STUDIO_PLAYBACK_STOPPED 이고 @c !mIsLoop 이면 @c mIsFinished = true.
		///          SJH_HAS_FMOD 미정의 환경에서는 비루프 시 즉시 @c mIsFinished = true
		///          (SequencePlayable 행 방지).
		/// @param dt 프레임 델타 (미사용 -- FMOD 가 자체 시간 추적).
		void OnUpdate(float dt) override;

	  private:
		::FMOD::Studio::EventDescription *mDesc = nullptr;  ///< Bank 소유 이벤트 템플릿 -- release 금지, 포인터만 보유.
		// [[maybe_unused]] -- FMOD 미빌드 시 사용처가 #ifdef 로 빠져 clang
		// -Werror=unused-private-field 에 걸리는 것을 방지 (raw 포인터 멤버 한정).
		[[maybe_unused]] ::FMOD::Studio::EventInstance *mInstance = nullptr;  ///< 자체 소유 재생 단위 -- dtor/OnStop 에서 release 책임.
		std::optional<vmath::vec3>        mWorldPos;                          ///< 3D 이벤트 위치. nullopt 이면 2D 재생.
	};
}

#endif // _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__
