#ifndef __TOPDOWNSHOOTER_SPAWNS_ONE_SHOT_SWEEPER_H__
#define __TOPDOWNSHOOTER_SPAWNS_ONE_SHOT_SWEEPER_H__

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Spawns
{
    /// @brief fxParent 자식 중 AutoDespawnOnFinish.IsDone() 인 child 를 RemoveChild.
    ///        매 프레임 씬 Update *직후* 1회 호출 (Cocos end-of-frame cleanup 정통).
    void SweepFinishedChildren(SJH::Scene::Actor& fxParent);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_ONE_SHOT_SWEEPER_H__
