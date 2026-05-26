#include "playable/interval_playable.h"

namespace SJH::Playable
{
    IntervalPlayable::IntervalPlayable(float duration) : duration_(duration) {}
    IntervalPlayable::~IntervalPlayable() = default;

    void IntervalPlayable::OnUpdate(float /*dt*/)
    {
        if (elapsed_ >= duration_)
            finished_ = true;
    }
}
