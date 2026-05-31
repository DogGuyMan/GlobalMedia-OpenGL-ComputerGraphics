#ifndef __TOPDOWNSHOOTER_SPAWNS_COMBAT_SEQUENCES_H__
#define __TOPDOWNSHOOTER_SPAWNS_COMBAT_SEQUENCES_H__

#include "Spawns/SequenceContext.h"
#include <vmath.h>

namespace TopdownShooter::Spawns
{
    /// @brief 적 피격 임팩트 — VfxInstance(spark) + AudioInstance(event:/Hit) @pos. 둘 다 단발 자동 despawn.
    void SpawnHitSpark(const SequenceContext& ctx, const vmath::vec3& worldPos);

    /// @brief 적 사망 — VfxInstance(explosion) + AudioInstance(event:/EnemyDeath) @pos.
    void SpawnEnemyDeathFX(const SequenceContext& ctx, const vmath::vec3& worldPos);

    /// @brief 픽업 — AudioInstance(event:/Pickup) @pos (오디오 전용).
    void SpawnPickupChime(const SequenceContext& ctx, const vmath::vec3& worldPos);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_COMBAT_SEQUENCES_H__
