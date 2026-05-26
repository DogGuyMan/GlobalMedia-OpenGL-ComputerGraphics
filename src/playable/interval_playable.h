#ifndef __SJH_PLAYABLE_INTERVAL_PLAYABLE_H__
#define __SJH_PLAYABLE_INTERVAL_PLAYABLE_H__

#include "playable/playable_base.h"

namespace SJH::Playable
{
    /// @brief N초 대기 후 finished_=true. SequencePlayable::AppendInterval 이 내부 생성.
    class IntervalPlayable : public PlayableBase
    {
      public:
        explicit IntervalPlayable(float duration);
        ~IntervalPlayable() override;

        void OnEnter() override {}
        void OnExit()  override {}

      protected:
        void OnUpdate(float dt) override;

      private:
        float duration_;
    };
}

#endif // __SJH_PLAYABLE_INTERVAL_PLAYABLE_H__
