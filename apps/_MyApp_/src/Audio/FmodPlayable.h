/**
 * @file FmodPlayable.h
 * @brief FMOD Core 직접 재생을 감싸는 leaf Playable - @c playSound + @c Channel 로 .wav/.ogg 재생 (M5 T1).
 *
 * @details
 *  ### 책임
 *  - @c SJH::Sound (Core @c FMOD::Sound RAII wrap) 를 Core @c System::playSound 로 재생.
 *  - @c Channel 핸들로 stop/pause/종료 폴링 - @c IPlayable 계약(Play/Pause/Stop/IsFinished) 노출.
 *  - 비루프 시 @c Channel::isPlaying 폴링으로 종료를 감지해 @c IsFinished 마킹 (FMODAPI.md §13).
 *
 *  ### 비-책임
 *  - [X] @c Sound 로드/소유 - @c SJH::ResourceRegistry 가 캐시 (여기는 빌림 포인터).
 *  - [X] Core System 생성/소유 - @c AudioSystem 담당 (여기는 빌림 포인터).
 *
 *  ### 정통 매핑
 *  - Unity `AudioSource` - 한 사운드의 재생 제어 단위 (씬 Component).
 *
 * @note Core @c Channel 은 RAII 가 아닌 정수 핸들에 가까운 빌림 포인터 - 정지/종료 후 재활용되므로
 *       직접 release/delete 금지 (@c isPlaying 으로 가드, FMODAPI.md §13).
 * @note 단발 SFX 는 Studio 이벤트(@c FmodStudioPlayable)로 통일 - Core leaf 는 BGM/지속음 핸들 제어용.
 * @note FMOD 미빌드 시 모든 동작이 no-op - 시퀀스 행을 막기 위해 비루프면 즉시 @c IsFinished (.cpp 참조).
 */
#ifndef _TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__
#define _TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__

#include "playable/playable_base.h"
#include "resource_registry/sound.h"   // SJH::Sound

namespace FMOD { class System; class Channel; }

namespace TopdownShooter::Audio
{
	/**
	 * @brief FMOD Core API .wav/.ogg 재생 leaf Playable (M5 T1).
	 * @details @c PlayableBase 파생 - @c OnPlay/OnStop/OnUpdate hook 에서 Core @c playSound/Channel 호출.
	 *          @c mSys / @c mSound 는 외부 소유 빌림 포인터, @c mChannel 만 재생 단위로 보유.
	 */
	class FmodPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		/// @brief Core System 과 재생할 Sound 를 받아 leaf 구성 (둘 중 하나라도 null 이면 warn).
		/// @param sys   Core System 빌림 포인터 (@c AudioSystem::GetSystem).
		/// @param sound 재생할 Sound 빌림 포인터 (@c ResourceRegistry 캐시).
		FmodPlayable(::FMOD::System *sys, SJH::Sound *sound);
		/// @brief 살아있는 @c Channel 이 있으면 stop 후 정리.
		~FmodPlayable() override;

		/// @brief 일시정지 - base 상태 갱신 후 @c Channel::setPaused(true).
		void Pause() override;

	  protected:
		/// @brief 재생 시작 - Core @c System::playSound 로 @c mSound 를 재생해 @c mChannel 획득.
		void OnPlay() override;
		/// @brief 재생 정지 - @c Channel::stop 후 핸들 무효화.
		void OnStop() override;
		/// @brief 매 프레임 종료 폴링 - 비루프 시 @c Channel::isPlaying false 면 @c IsFinished 마킹.
		/// @param dt 미사용 (Channel 종료는 FMOD 가 추적).
		void OnUpdate(float dt) override;

	  private:
		::FMOD::System  *mSys     = nullptr;  ///< Core System 빌림 포인터 (AudioSystem 소유).
		SJH::Sound      *mSound   = nullptr;  ///< 재생 대상 Sound 빌림 포인터 (ResourceRegistry 소유).
		// [[maybe_unused]] — FMOD 미빌드(SJH_HAS_FMOD 미정의) 시 모든 사용처가 #ifdef 로 빠져
		// clang -Werror=unused-private-field 에 걸리는 것을 방지.
		[[maybe_unused]] ::FMOD::Channel *mChannel = nullptr;  ///< 현재 재생 Channel - 빌림 핸들 (release 금지).
	};
}

#endif // _TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__
