/**
 * @file CombatSequences.cpp
 * @brief CombatSequences.h 구현 - 전투 단발 FX/오디오 스폰 본체.
 *
 * @details
 *  각 함수는 @c SpawnVfxInstance / @c SpawnAudioInstance 헬퍼에 위임해
 *  @c AutoDespawnOnFinish 가 부착된 단발 Actor 를 fxRoot 아래 생성한다.
 *
 *  - @c SpawnHitSpark   : spark VFX 단발 (오디오 미사용).
 *  - @c SpawnEnemyDeathFX : explosion VFX + 사망 오디오 단발.
 *  - @c SpawnPickupChime  : 픽업 오디오 단발 (VFX 없음).
 *
 * @note FMOD 이벤트 경로 상수는 @c Audio::Constants.h 에서 제공 (EVENT_HIT / EVENT_ENEMY_DEATH / EVENT_PICKUP).
 */
#include "Spawns/CombatSequences.h"
#include "Spawns/VfxInstance.h"
#include "Spawns/AudioInstance.h"
#include "Audio/AudioSystem.h"
#include "Audio/Constants.h"          // EVENT_HIT / EVENT_ENEMY_DEATH / EVENT_PICKUP
#include "resource_registry/resource_registry.h"

namespace TopdownShooter::Spawns
{
    void SpawnHitSpark(const SequenceContext& ctx, const vmath::vec3& worldPos)
    {
        if (!ctx.fxRoot) return;
        // VFX one-shot (spark) + 오디오 one-shot (event:/Hit) - 각자 독립 자동 despawn.
        if (ctx.vfx && ctx.reg)
            SpawnVfxInstance(*ctx.fxRoot, ctx.vfx, ctx.reg->FindEffect("spark"), worldPos);
    }

    void SpawnEnemyDeathFX(const SequenceContext& ctx, const vmath::vec3& worldPos)
    {
        if (!ctx.fxRoot) return;
        if (ctx.vfx && ctx.reg)
            SpawnVfxInstance(*ctx.fxRoot, ctx.vfx, ctx.reg->FindEffect("explosion"), worldPos);
        if (ctx.audio)
            SpawnAudioInstance(*ctx.fxRoot, ctx.audio->LoadEvent(Audio::EVENT_ENEMY_DEATH), worldPos);
    }

    void SpawnPickupChime(const SequenceContext& ctx, const vmath::vec3& worldPos)
    {
        if (!ctx.fxRoot || !ctx.audio) return;
        SpawnAudioInstance(*ctx.fxRoot, ctx.audio->LoadEvent(Audio::EVENT_PICKUP), worldPos);
    }
}
