/**
 * @file interval_playable.cpp
 * @brief @c IntervalPlayable 생성자 / dtor / @c OnUpdate 구현.
 *
 * @details
 *  ### 책임
 *  - 생성자에서 @c mDuration 초기화.
 *  - @c OnUpdate: @c mElapsed >= @c mDuration 조건 충족 시 @c mIsFinished = @c true.
 *
 *  ### 비-책임
 *  - [X] @c mElapsed 직접 누적 - @c PlayableBase::Update (@c final) 가 자동 누적.
 *
 * @note @c dt 파라미터는 @c PlayableBase 가 이미 @c mElapsed 에 반영했으므로
 *       @c OnUpdate 내에서 @c dt 를 사용하지 않음 (미사용 파라미터 주석 마킹).
 */
#include "playable/interval_playable.h"

namespace SJH::Playable
{
    IntervalPlayable::IntervalPlayable(float duration) : mDuration(duration) {}

    // out-of-line dtor - vtable anchor
    IntervalPlayable::~IntervalPlayable() = default;

    void IntervalPlayable::OnUpdate(float /*dt*/)
    {
        // mElapsed 는 PlayableBase::Update(final) 이 자동 누적 - dt 직접 사용 불필요
        if (mElapsed >= mDuration)
            mIsFinished = true;
    }
}
