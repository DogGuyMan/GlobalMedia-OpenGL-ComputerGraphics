#ifndef _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__
#define _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__

#include "playable/playable_base.h"
#include <optional>
#include <string>
#include <vmath.h>

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
		explicit FmodStudioPlayable(::FMOD::Studio::EventDescription *desc,
		                            std::optional<vmath::vec3> worldPos = std::nullopt);
		~FmodStudioPlayable() override;

		void Pause() override;

		/// @brief 재생 중 instance 파라미터 설정 (FMOD setParameterByName). instance 없으면 no-op.
		///        BGM_STATE(Title=0/Combat=1) 전환에 사용 (Stage FSM).
		void SetParameter(const std::string &name, float value);
		/// @brief instance pause/resume 토글. Pause() 와 달리 Playable mPaused 상태는 건드리지 않음
		///        (Pause State 일시정지 ↔ Combat 복귀 resume 용).
		void SetPaused(bool paused);

	  protected:
		void OnPlay() override;
		void OnStop() override;
		void OnUpdate(float dt) override;

	  private:
		::FMOD::Studio::EventDescription *mDesc     = nullptr;
		// [[maybe_unused]] — FMOD 미빌드 시 사용처가 #ifdef 로 빠져 clang
		// -Werror=unused-private-field 에 걸리는 것을 방지 (raw 포인터 멤버 한정).
		[[maybe_unused]] ::FMOD::Studio::EventInstance *mInstance = nullptr;
		std::optional<vmath::vec3>        mWorldPos;
	};
}

#endif // _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__
