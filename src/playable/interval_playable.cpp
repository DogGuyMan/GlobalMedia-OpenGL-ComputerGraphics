#include "playable/interval_playable.h"

namespace SJH::Playable
{
    IntervalPlayable::IntervalPlayable(float duration) : mDuration(duration) {}
    IntervalPlayable::~IntervalPlayable() = default;

    void IntervalPlayable::OnUpdate(float /*dt*/)
    {
        if (mElapsed >= mDuration)
            mIsFinished = true;
    }
}
