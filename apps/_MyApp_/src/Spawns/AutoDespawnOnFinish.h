#ifndef __TOPDOWNSHOOTER_SPAWNS_AUTO_DESPAWN_ON_FINISH_H__
#define __TOPDOWNSHOOTER_SPAWNS_AUTO_DESPAWN_ON_FINISH_H__

#include "playable/iplayable.h"
#include "scene/actor.h"

namespace TopdownShooter::Spawns
{
    /// @brief 감시 Composite(IPlayable*)가 IsFinished() 면 mDone=true 마킹.
    ///        실제 RemoveChild 는 SweepFinishedChildren 가 Update 밖에서 수행 (반복자 무효화 회피).
    class AutoDespawnOnFinish : public SJH::Scene::Component
    {
      public:
        explicit AutoDespawnOnFinish(SJH::Playable::IPlayable* watched) : mWatched(watched) {}

        bool IsDone() const { return mDone; }

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override
        {
            if (!mDone && mWatched && mWatched->IsFinished())
                mDone = true;
        }

      private:
        SJH::Playable::IPlayable* mWatched = nullptr;
        bool                      mDone    = false;
    };
}

#endif // __TOPDOWNSHOOTER_SPAWNS_AUTO_DESPAWN_ON_FINISH_H__
