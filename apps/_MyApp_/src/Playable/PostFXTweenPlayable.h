#ifndef __TOPDOWNSHOOTER_PLAYABLE_POSTFX_TWEEN_PLAYABLE_H__
#define __TOPDOWNSHOOTER_PLAYABLE_POSTFX_TWEEN_PLAYABLE_H__

#include "playable/playable_base.h"

#include <tweeny/tweeny.h>
#include <string>

namespace TopdownShooter::Playable
{
	/// @brief PostFX pass Material 의 float uniform 을 tweeny 트윈으로 구동하는 leaf Playable.
	///        PostFXRegistry 로 pass Material 을 매 프레임 조회(없으면 무시)해 Properties.Floats[uniform] 에 기록.
	///        피격 비네팅 플래시(uVignetteAmount) / 사망 grayscale 페이드(uGrayscaleAmount) 등 화면 전체 연출용.
	///        one-shot — progress>=1 에서 finished_ (Composite 안 1회 재생). Play() 시 트윈 처음으로 되감김.
	///        !! tweeny step(int32_t ms) 오버로드 강제 (memory tweeny_step_overload_trap — float 오버로드 금지).
	class PostFXTweenPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		/// @param passName    PostFXRegistry 키 (예: "grayscale_vignetting")
		/// @param uniformName 셰이더 float uniform 명 (예: "uVignetteAmount" / "uGrayscaleAmount")
		/// @param tween       tweeny::from(a).to(b).during(ms).via(easing) — 값이 곧 uniform 값
		PostFXTweenPlayable(std::string passName, std::string uniformName, tweeny::tween<float> tween);
		~PostFXTweenPlayable() override;

	  protected:
		void OnPlay() override;           // 트윈 되감기 (seek 0) — 매 Play 마다 처음부터
		void OnUpdate(float dt) override; // step(ms) → uniform 기록 → progress>=1 시 one-shot finish

	  private:
		std::string          mPassName;
		std::string          mUniformName;
		tweeny::tween<float> mTween;
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_POSTFX_TWEEN_PLAYABLE_H__
