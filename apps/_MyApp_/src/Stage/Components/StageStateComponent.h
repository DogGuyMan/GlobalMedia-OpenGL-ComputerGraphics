/**
 * @file StageStateComponent.h
 * @brief Stage Actor 에 부착되는 진행 상태 식별 Component — 현재 EStageStatus 를 보유.
 *
 * @details
 *  ### 책임
 *  - @c EStageStatus 를 setter/getter 로 노출하여 외부 코드가 "이 Actor 가 어느 스테이지
 *    상태에 있는지" 를 조회할 수 있도록 한다.
 *  - 이 Component 의 존재 자체가 Actor 가 Stage 임을 식별하는 마커 역할을 한다.
 *
 *  ### 비-책임
 *  - [X] FSM 전이 판정 — @c StageStateMachine 이 담당. M4/M7 이후 FSM 통합 예정.
 *  - [X] 게임 로직 tick — Update/OnEnter/OnExit 는 stub.
 *
 * @note 현재는 setter/getter 만 제공하는 단순 상태 보유 Component.
 *       FSM 통합은 M4 / M7 후속 리팩토링에서 진행한다.
 */
#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_STAGE_STATE_COMPONENT_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_STAGE_STATE_COMPONENT_H__

#include "Stage/Stage.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage::Components
{
    /**
     * @brief Stage Actor 에 부착되는 진행 상태 식별 Component.
     * @details
     *  현재는 setter/getter 만 제공. FSM 통합은 후속 리팩토링(M4 / M7) 예정.
     *  이 Component 의 존재 여부로 Actor 가 "Stage Actor" 임을 식별할 수 있다.
     */
    class StageState : public SJH::Scene::Component
    {
    public:
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override {}

        /// @brief 현재 스테이지 상태를 반환한다.
        /// @return 현재 @c EStageStatus 값.
        EStageStatus Current() const { return mCurrent; }

        /// @brief 현재 스테이지 상태를 설정한다.
        /// @param s 설정할 @c EStageStatus 값.
        void SetCurrent(EStageStatus s) { mCurrent = s; }

    private:
        EStageStatus mCurrent = EStageStatus::Title; ///< 현재 스테이지 진행 상태 (초기값 = Title).
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_STAGE_STATE_COMPONENT_H__
