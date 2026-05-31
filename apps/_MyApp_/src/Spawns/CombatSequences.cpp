#include "Spawns/CombatSequences.h"
#include "Spawns/VfxInstance.h"
#include "Spawns/AudioInstance.h"
#include "Audio/AudioSystem.h"
#include "resource_registry/resource_registry.h"

namespace TopdownShooter::Spawns
{
    void SpawnHitSpark(const SequenceContext& ctx, const vmath::vec3& worldPos)
    {
        if (!ctx.fxRoot) return;
        // VFX one-shot (spark) + 오디오 one-shot (event:/Hit) — 각자 독립 자동 despawn.
        if (ctx.vfx && ctx.reg)
            SpawnVfxInstance(*ctx.fxRoot, ctx.vfx, ctx.reg->FindEffect("spark"), worldPos);
        if (ctx.audio)
            SpawnAudioInstance(*ctx.fxRoot, ctx.audio->LoadEvent("event:/Hit"), worldPos);
    }

    void SpawnEnemyDeathFX(const SequenceContext& ctx, const vmath::vec3& worldPos)
    {
        if (!ctx.fxRoot) return;
        if (ctx.vfx && ctx.reg)
            SpawnVfxInstance(*ctx.fxRoot, ctx.vfx, ctx.reg->FindEffect("explosion"), worldPos);
        if (ctx.audio)
            SpawnAudioInstance(*ctx.fxRoot, ctx.audio->LoadEvent("event:/EnemyDeath"), worldPos);
    }

    void SpawnPickupChime(const SequenceContext& ctx, const vmath::vec3& worldPos)
    {
        if (!ctx.fxRoot || !ctx.audio) return;
        SpawnAudioInstance(*ctx.fxRoot, ctx.audio->LoadEvent("event:/Pickup"), worldPos);
    }
}
