/**
 * @file TweenPlayable.h
 * @brief Tweeny @c tween<T> 를 감싸 매 프레임 보간값을 콜백에 흘리는 leaf Playable (M5 T4).
 *
 * @details
 *  ### 책임
 *  - tween 을 dt(초)만큼 진행시키고 보간된 값 @c T 를 사용자 콜백(@c mOnStep)으로 전달.
 *  - 루프 경계 처리 - tweeny 가 0/1 에서 클램프하므로 본 클래스가 수동 재개(아래 루프 의미론).
 *
 *  ### 비-책임
 *  - [X] 무엇을 트위닝하는지 - 값 적용은 전적으로 @c mOnStep 콜백 책임.
 *
 *  ### 정통 매핑
 *  - DOTween/Tweeny leaf - Composite(@c SequencePlayable / @c ParallelPlayable) 의 말단 노드.
 *
 * @note @c step(int32_t ms) 오버로드만 사용 - @c step(float ratio) 는 양 끝 깜빡임 폭주 함정
 *       (memory tweeny_step_overload_trap).
 */
#ifndef _TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__
#define _TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__

#include "playable/playable_base.h"
#include <tweeny/tweeny.h>
#include <cstdint>
#include <functional>

namespace TopdownShooter::Tween
{
	/**
	 * @brief Tweeny tween<T> wrap leaf Playable (M5 T4).
	 * @tparam T 보간 대상 값 타입 (float / glm::vec3 등).
	 * @details
	 *   !! step(int32_t ms) 오버로드 강제 - step(float ratio) 오버로드는 dt(초) 직접 입력 시 양 끝 깜빡임 폭주
	 *   (memory tweeny_step_overload_trap).
	 *
	 *   ### 루프 의미론 (중요)
	 *   tweeny 의 step 은 내부적으로 seek(progress + dp) -> clip(0,1) 이라 progress 가 1.0 에 닿으면
	 *   그 자리에서 *멈춘다* (자동 래핑 없음). 그래서 SetIsLoop(true) 만으로는 "끝에서 고정"이지 반복이 아니다.
	 *   -> 본 클래스가 경계에서 수동 재개한다:
	 *     - LoopMode::Restart  : progress>=1.0 시 seek(0.0f) - 톱니파(끝->처음 스냅). 0<->360 회전처럼
	 *                            시/종점이 시각적으로 같은 트윈에 적합.
	 *     - LoopMode::PingPong : progress>=1.0 시 backward(), <=0.0 시 forward() - 매끄러운 왕복.
	 *                            스케일 펄스 / 상하 바운스 / 밝기 호흡 등 "갔다 돌아오는" 효과 정통.
	 *   isLoop_=false (기본) 면 종전대로 progress>=1.0 에서 finished_ -> one-shot (Composite 안 한 번 재생).
	 */
	template <typename T>
	class TweenPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		/// @brief 루프 재개 방식 - isLoop_=true 일 때만 적용. 기본 PingPong (가장 매끄러운 상시 반복).
		enum class LoopMode {
			Restart,   ///< 끝에 닿으면 처음으로 스냅 (톱니파). 시/종점이 시각적으로 같은 트윈에 적합.
			PingPong   ///< 끝/처음에서 방향 반전 (매끄러운 왕복). 펄스/바운스/호흡 효과에 적합.
		};

		/// @brief tween 과 매 step 콜백을 받아 leaf Playable 구성.
		/// @param tween    진행시킬 tweeny tween (이동 보유).
		/// @param onStep   매 프레임 보간값 @c T 를 받는 콜백 (값 적용 책임).
		/// @param loopMode 루프 시 경계 재개 방식 (기본 @c PingPong).
		TweenPlayable(tweeny::tween<T> tween, std::function<void(T)> onStep,
		              LoopMode loopMode = LoopMode::PingPong)
		    : mTween(std::move(tween)), mOnStep(std::move(onStep)), mLoopMode(loopMode) {}
		~TweenPlayable() override = default;

	  protected:
		/// @brief @c PlayableBase 의 매 프레임 hook - tween 진행 + 콜백 + 루프/완료 처리.
		/// @param dt 직전 프레임 경과 시간(초). 내부에서 ms(int32_t)로 변환해 step.
		void OnUpdate(float dt) override
		{
			// !! int32_t (ms) 오버로드 명시 - float 오버로드 사용 절대 금지
			int32_t dtMs  = static_cast<int32_t>(dt * 1000.0f);
			T       value = mTween.step(dtMs);
			if (mOnStep) mOnStep(value);

			const float p = mTween.progress();
			if (!mIsLoop)
			{
				if (p >= 1.0f) mIsFinished = true;   // one-shot - 기존 동작 보존
				return;
			}

			// === 상시 루프 - step 이 0/1 에서 클램프하므로 경계에서 직접 재개 ===
			if (mLoopMode == LoopMode::Restart)
			{
				if (p >= 1.0f) mTween.seek(0.0f);          // 끝 -> 처음 (스냅)
			}
			else // PingPong
			{
				if (p >= 1.0f)      mTween.backward();      // 끝   -> 역방향
				else if (p <= 0.0f) mTween.forward();       // 처음 -> 정방향
			}
		}

	  private:
		tweeny::tween<T>       mTween;
		std::function<void(T)> mOnStep;
		LoopMode               mLoopMode = LoopMode::PingPong;
	};
}

#endif // _TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__
