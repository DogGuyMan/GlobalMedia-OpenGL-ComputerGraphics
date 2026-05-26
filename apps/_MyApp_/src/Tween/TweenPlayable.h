#ifndef _TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__
#define _TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__

#include "playable/playable_base.h"
#include <tweeny/tweeny.h>
#include <cstdint>
#include <functional>

namespace TopdownShooter::Tween
{
	/// @brief Tweeny tween<T> wrap leaf Playable (M5 T4).
	/// @details
	///   ⚠ step(int32_t ms) 오버로드 강제 — step(float ratio) 오버로드는 dt(초) 직접 입력 시 양 끝 깜빡임 폭주
	///   (memory tweeny_step_overload_trap).
	///   tweeny 의 tween.progress() 는 [0..1] — isLoop_=false 면 1.0 도달 시 finished_.
	template <typename T>
	class TweenPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		TweenPlayable(tweeny::tween<T> tween, std::function<void(T)> onStep)
		    : mTween(std::move(tween)), mOnStep(std::move(onStep)) {}
		~TweenPlayable() override = default;

	  protected:
		void OnUpdate(float dt) override
		{
			// ⚠ int32_t (ms) 오버로드 명시 — float 오버로드 사용 절대 금지
			int32_t dtMs = static_cast<int32_t>(dt * 1000.0f);
			T value = mTween.step(dtMs);
			if (mOnStep) mOnStep(value);
			if (mTween.progress() >= 1.0f && !isLoop_) finished_ = true;
		}

	  private:
		tweeny::tween<T>       mTween;
		std::function<void(T)> mOnStep;
	};
}

#endif // _TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__
