/**
 * @file PostFXTweenPlayable.h
 * @brief PostFX pass Material 의 float uniform 을 tweeny 트윈으로 구동하는 leaf Playable.
 *
 * @details
 *  ### 책임
 *  - @c PlayableBase (IPlayable + Component 다중 상속) 를 구현해 tweeny 트윈을 구동한다.
 *  - @c PostFXRegistry 로 패스 Material 을 매 프레임 조회하고
 *    @c Properties.Floats[uniformName] 에 트윈 현재값을 기록한다.
 *  - one-shot: progress >= 1.0 에서 @c mIsFinished = true -> Composite 안에서 1회 재생 후 종료.
 *  - @c Play() 시 트윈 처음으로 되감기 (@c seek(0)) -- Stop()+Play() 재트리거 정상 동작.
 *  ### 비-책임
 *  - [X] Material 소유권 -- PassComponent 가 소유. 비소유 포인터 조회만.
 *  - [X] PostFX 활성화/비활성화 -- PassComponent::SetEnabled 담당.
 *  - [X] HP 연속 바인딩 -- @c HpGrayscalePostFX (별도 Component) 담당.
 *  ### 정통 매핑
 *  - Cocos2D @c ActionTo / @c ActionBy + @c EaseFunction leaf action.
 *  - Unity @c DOTween.To(getter, setter, endValue, duration) leaf tween.
 * @note tweeny @c step(int32_t ms) 오버로드를 반드시 사용해야 한다.
 *       @c step(float) 는 [0,1] 비율 오버로드로, dt(ms) 를 그대로 넘기면 tween 이 폭주한다
 *       (memory: tweeny_step_overload_trap).
 */
#ifndef __TOPDOWNSHOOTER_PLAYABLE_POSTFX_TWEEN_PLAYABLE_H__
#define __TOPDOWNSHOOTER_PLAYABLE_POSTFX_TWEEN_PLAYABLE_H__

#include "playable/playable_base.h"

#include <tweeny/tweeny.h>
#include <string>

namespace TopdownShooter::Playable
{
	/**
	 * @brief PostFX float uniform 을 tweeny 트윈으로 구동하는 leaf Playable.
	 * @details
	 *  - 피격 비네팅 플래시(@c uVignetteAmount) / 사망 grayscale 페이드(@c uGrayscaleAmount) 등
	 *    화면 전체 일시 연출에 사용.
	 *  - @c PlayableBase::Update -> @c OnUpdate 훅 -> @c mTween.step(ms) -> uniform 기록.
	 *  - one-shot: @c mTween.progress() >= 1.0 에서 @c mIsFinished = true.
	 *  - Composite(@c SequencePlayable / @c ParallelPlayable) 안에 leaf 로 삽입 가능.
	 */
	class PostFXTweenPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		/// @brief PostFX 패스 이름 / uniform 명 / tweeny 트윈을 지정해 생성.
		/// @param passName    @c PostFXRegistry 키 (예: "grayscale_vignetting").
		/// @param uniformName 기록 대상 float uniform 명 (예: "uVignetteAmount" / "uGrayscaleAmount").
		/// @param tween       @c tweeny::from(a).to(b).during(ms).via(easing) -- 값이 곧 uniform 값.
		PostFXTweenPlayable(std::string passName, std::string uniformName, tweeny::tween<float> tween);
		~PostFXTweenPlayable() override;

	  protected:
		/// @brief @c Play() 호출 시 트윈을 seek(0) 으로 되감기.
		/// @details Stop() + Play() 재트리거 시 처음부터 다시 재생 보장.
		void OnPlay() override;           // 트윈 되감기 (seek 0) - 매 Play 마다 처음부터
		/// @brief 트윈을 ms 단위로 step 해 uniform 기록. progress >= 1.0 시 one-shot 종료.
		/// @param dt 프레임 델타 타임 (초). 내부에서 int32_t ms 로 변환해 @c mTween.step(ms) 호출.
		/// @note @c step(float) 비율 오버로드 금지 -- @c int32_t ms 오버로드 강제
		///       (memory: tweeny_step_overload_trap).
		void OnUpdate(float dt) override; // step(ms) -> uniform 기록 -> progress>=1 시 one-shot finish

	  private:
		std::string          mPassName;    ///< PostFXRegistry 검색 키 (예: "grayscale_vignetting").
		std::string          mUniformName; ///< 기록 대상 float uniform 명 (예: "uVignetteAmount").
		tweeny::tween<float> mTween;       ///< tweeny 트윈 인스턴스 -- from/to/during/via 체인 결과.
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_POSTFX_TWEEN_PLAYABLE_H__
