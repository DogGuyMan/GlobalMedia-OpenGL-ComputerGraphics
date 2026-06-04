#ifndef _TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__
#define _TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__

#include "playable/playable_base.h"
#include "resource_registry/sound.h"   // SJH::Sound

namespace FMOD { class System; class Channel; }

namespace TopdownShooter::Audio
{
	/// @brief FMOD Core API .wav/.ogg 재생 leaf Playable (M5 T1).
	class FmodPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		FmodPlayable(::FMOD::System *sys, SJH::Sound *sound);
		~FmodPlayable() override;

		void Pause() override;

	  protected:
		void OnPlay() override;
		void OnStop() override;
		void OnUpdate(float dt) override;

	  private:
		::FMOD::System  *mSys     = nullptr;
		SJH::Sound      *mSound   = nullptr;
		// [[maybe_unused]] — FMOD 미빌드(SJH_HAS_FMOD 미정의) 시 모든 사용처가 #ifdef 로 빠져
		// clang -Werror=unused-private-field 에 걸리는 것을 방지.
		[[maybe_unused]] ::FMOD::Channel *mChannel = nullptr;
	};
}

#endif // _TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__
