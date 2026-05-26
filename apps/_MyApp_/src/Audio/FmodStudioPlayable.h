#ifndef _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__
#define _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__

#include "playable/playable_base.h"

namespace FMOD::Studio { class EventDescription; class EventInstance; }

namespace TopdownShooter::Audio
{
	/// @brief FMOD Studio EventDescription 을 *createInstance + start* 하여 재생 (M5 T3).
	/// @details
	///   - EventDescription = 외부 owner (AudioSystem 캐시). EventInstance = 자체 owner.
	///   - OnPlay 마다 새 instance 생성. loop event 는 SetIsLoop(true) 동반.
	class FmodStudioPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		explicit FmodStudioPlayable(::FMOD::Studio::EventDescription *desc);
		~FmodStudioPlayable() override;

		void Pause() override;

	  protected:
		void OnPlay() override;
		void OnStop() override;
		void OnUpdate(float dt) override;

	  private:
		::FMOD::Studio::EventDescription *mDesc     = nullptr;
		::FMOD::Studio::EventInstance    *mInstance = nullptr;
	};
}

#endif // _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__
