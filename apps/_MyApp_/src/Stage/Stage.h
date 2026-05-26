#ifndef __TOPDOWNSHOOTER_STAGE_H__
#define __TOPDOWNSHOOTER_STAGE_H__

#include <cstdint>

namespace TopdownShooter::Stage
{
    /// @brief 스테이지 진행 상태 — Title / Combat / Boss 3 단계 비트 플래그.
    /// @details
    ///   - 각 enumerator 가 단일 비트 — FSM 통합 시 GetStateFlag/GetTransitFlag 와 호환.
    ///   - 현재는 Components::StageState 가 단순 보유 (setter/getter). FSM 활성화는 M4 / M7.
    ///   - NONE 은 의도적으로 없음 — 항상 Title/Combat/Boss 중 하나.
    enum class EStageStatus : uint64_t
    {
        Title  = 1ull << 0,
        Combat = 1ull << 1,
        Boss   = 1ull << 2,
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_H__
