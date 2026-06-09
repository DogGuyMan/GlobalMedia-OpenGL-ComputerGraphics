/**
 * @file interval_playable.h
 * @brief @c SJH::Playable::IntervalPlayable - 고정 시간 대기 leaf Playable.
 *
 * @details
 *  ### 책임
 *  - @c mElapsed >= @c mDuration 이 되면 @c mIsFinished = @c true.
 *  - @c SequencePlayable::AppendInterval(float) 이 내부에서 생성하는 전용 leaf.
 *
 *  ### 비-책임
 *  - [X] 연출 로직 (스프라이트/사운드/파티클) - 다른 leaf Playable (@c SpriteSequencePlayable 등) 담당.
 *  - [X] Loop 동작 - 일반적으로 Loop=false 단발 대기. Loop=true 세팅은 가능하나 사용 케이스 없음
 *    (@c PlayableBase::SetIsLoop 경유 시 Loop 자동 재시작 - 주의).
 *
 * @note @c mElapsed 는 @c PlayableBase::Update(@c final) 에서 누적되므로
 *       @c OnUpdate 에서 별도 @c dt 누적 불필요.
 */
#ifndef __SJH_PLAYABLE_INTERVAL_PLAYABLE_H__
#define __SJH_PLAYABLE_INTERVAL_PLAYABLE_H__

#include "playable/playable_base.h"

namespace SJH::Playable
{
    /**
     * @brief 고정 시간(@c mDuration 초) 대기 후 완료를 신호하는 leaf Playable.
     * @details
     *  @c mElapsed >= @c mDuration 조건 충족 시 @c mIsFinished = @c true.
     *  @c mElapsed 는 @c PlayableBase::Update (final) 이 자동 누적 - @c OnUpdate 에서 @c dt 를 직접 쓸 필요 없음.
     *
     *  ### 전형적 사용처
     *  ```cpp
     *  seq->AppendInterval(0.5f);   // SequencePlayable 내부에서 make_unique<IntervalPlayable>(0.5f)
     *  ```
     *  직접 생성도 가능:
     *  ```cpp
     *  seq->Append(std::make_unique<IntervalPlayable>(1.0f));
     *  ```
     */
    class IntervalPlayable : public PlayableBase
    {
      public:
        /// @brief @p duration 초 대기 후 완료하는 Playable 생성.
        /// @param duration 대기 시간 (초, 양수).
        explicit IntervalPlayable(float duration);

        /// @brief out-of-line dtor (@c interval_playable.cpp 에 @c =default 정의).
        ~IntervalPlayable() override;

        /// @brief Actor 씬 그래프 진입 시 호출. 동작 없음.
        void OnEnter() override {}

        /// @brief Actor 씬 그래프 이탈 시 호출. 동작 없음.
        void OnExit()  override {}

      protected:
        /// @brief @c mElapsed >= @c mDuration 이면 @c mIsFinished = @c true.
        void OnUpdate(float dt) override;

      private:
        /// @brief 대기 시간 (초). 생성자에서 설정 후 불변.
        float mDuration;
    };
}

#endif // __SJH_PLAYABLE_INTERVAL_PLAYABLE_H__
