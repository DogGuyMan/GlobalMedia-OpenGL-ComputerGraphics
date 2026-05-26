#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_STAGE_STATE_COMPONENT_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_STAGE_STATE_COMPONENT_H__

#include "Stage/Stage.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage::Components
{
    /// @brief Stage Actor 에 부착되는 진행 상태 Component.
    /// @details
    ///   - 현재는 setter/getter 만 — FSM 통합은 후속 (M4 / M7).
    ///   - 이 Component 의 존재 여부로 Actor 가 "Stage" 인지 식별 가능.
    class StageState : public SJH::Scene::Component
    {
    public:
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override {}

        EStageStatus Current() const { return mCurrent; }
        void SetCurrent(EStageStatus s) { mCurrent = s; }

    private:
        EStageStatus mCurrent = EStageStatus::Title;
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_STAGE_STATE_COMPONENT_H__
