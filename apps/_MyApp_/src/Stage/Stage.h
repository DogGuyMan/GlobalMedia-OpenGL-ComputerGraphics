/**
 * @file Stage.h
 * @brief 스테이지 진행 상태 enum @c EStageStatus 정의 - StageStateMachine 의 상태 식별 비트.
 *
 * @details
 *  ### 책임
 *  - Stage FSM 의 상태 집합을 단일 비트 플래그(@c uint64_t)로 표현.
 *  - @c SJH::FSM 의 self-identifying State 규약(GetStateFlag/GetTransitFlag)과 호환되는 비트 레이아웃 제공.
 *
 *  ### 비-책임
 *  - [X] 상태 전이 로직 - @c StageStateMachine / 각 구상 State(@c StageState.Impl.h) 담당.
 *  - [X] 상태별 동작(BGM/오버레이/tick) - 구상 State 의 OnEnter/OnUpdate/OnExit 담당.
 *
 *  ### 정통 매핑
 *  - Unity `enum` 기반 GameState 패턴 - 비트 플래그로 transit mask 합성(OR) 가능.
 *
 * @note startup 상태는 Title (Boot State 불요). NONE=0 은 FSM 엔진 sentinel 로 전이 대상이 될 수 없다.
 */
#ifndef __TOPDOWNSHOOTER_STAGE_H__
#define __TOPDOWNSHOOTER_STAGE_H__

#include <cstdint>

namespace TopdownShooter::Stage
{
    /// @brief 스테이지 진행 상태 - FSM(StageStateMachine)용 단일 비트 플래그.
    /// @details
    ///   - 각 enumerator 가 단일 비트 - IFsmState::GetStateFlag/GetTransitFlag 와 호환.
    ///   - NONE=0 은 FSM 엔진 sentinel(전이 불가). StateMachine 은 startup=Title 로 생성.
    ///   - Boss 는 미래 마일스톤(현재 미사용).
    enum class EStageStatus : uint64_t
    {
        NONE       = 0,         // FSM sentinel - 전이 불가
        Title      = 1ull << 0,
        CombatPlay = 1ull << 1, // 구 Combat 에서 리네임
        Boss       = 1ull << 2, // 미래 - 현재 미사용
        Pause      = 1ull << 3,
        GameOver   = 1ull << 4,
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_H__
