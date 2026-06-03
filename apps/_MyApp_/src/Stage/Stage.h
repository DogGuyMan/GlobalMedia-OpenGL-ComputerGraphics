#ifndef __TOPDOWNSHOOTER_STAGE_H__
#define __TOPDOWNSHOOTER_STAGE_H__

#include <cstdint>

namespace TopdownShooter::Stage
{
    /// @brief 스테이지 진행 상태 — FSM(StageStateMachine)용 단일 비트 플래그.
    /// @details
    ///   - 각 enumerator 가 단일 비트 — IFsmState::GetStateFlag/GetTransitFlag 와 호환.
    ///   - NONE=0 은 FSM 엔진 sentinel(전이 불가). StateMachine 은 startup=Title 로 생성.
    ///   - Boss 는 미래 마일스톤(현재 미사용).
    enum class EStageStatus : uint64_t
    {
        NONE       = 0,         // FSM sentinel — 전이 불가
        Title      = 1ull << 0,
        CombatPlay = 1ull << 1, // 구 Combat 에서 리네임
        Boss       = 1ull << 2, // 미래 — 현재 미사용
        Pause      = 1ull << 3,
        GameOver   = 1ull << 4,
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_H__
