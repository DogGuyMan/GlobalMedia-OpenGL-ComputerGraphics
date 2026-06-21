/**
 * @file CombatSequences.h
 * @brief 전투 이벤트별 단발 VFX/Audio 시퀀스 빌더 모음.
 *
 * @details
 *  ### 책임
 *  - 피격(@c SpawnHitSpark) / 사망(@c SpawnEnemyDeathFX) / 픽업(@c SpawnPickupChime) 순간에
 *    fxRoot 자식으로 단발 VFX Actor 와 오디오 Actor 를 스폰.
 *  - 스폰된 Actor 는 @c AutoDespawnOnFinish 를 보유해 @c SweepFinishedChildren 이 자동 파괴.
 *  ### 비-책임
 *  - [X] 스폰된 Actor 파괴 타이밍 제어 - @c OneShotSweeper::SweepFinishedChildren 담당.
 *  - [X] FMOD 이벤트 경로 상수 - @c Audio::Constants.h 담당.
 *  - [X] Effekseer effect 리소스 로드 - @c ResourceRegistry::FindEffect 담당.
 *  ### 정통 매핑
 *  - Cocos2D @c SimpleAudioEngine::playEffect / @c ParticleSystem::create - 이벤트 트리거 시 단발 스폰.
 *
 * @note @c ctx.fxRoot 가 nullptr 이면 모든 함수가 no-op 으로 안전하게 반환.
 *       VFX 는 @c ctx.vfx + @c ctx.reg 가 모두 유효할 때만 스폰된다.
 */
#ifndef __TOPDOWNSHOOTER_SPAWNS_COMBAT_SEQUENCES_H__
#define __TOPDOWNSHOOTER_SPAWNS_COMBAT_SEQUENCES_H__

#include "Spawns/SequenceContext.h"
#include <glm/glm.hpp>

namespace TopdownShooter::Spawns
{
    /// @brief 적 피격 임팩트 - VfxInstance(spark) + AudioInstance(event:/Hit) @pos. 둘 다 단발 자동 despawn.
    /// @param ctx      의존 묶음 (@c fxRoot / @c vfx / @c reg 가 유효해야 VFX 스폰).
    /// @param worldPos 스폰 위치 (월드 좌표).
    void SpawnHitSpark(const SequenceContext& ctx, const glm::vec3& worldPos);

    /// @brief 적 사망 - VfxInstance(explosion) + AudioInstance(event:/EnemyDeath) @pos.
    /// @param ctx      의존 묶음 (@c fxRoot 필수; @c vfx/@c reg 유효 시 VFX, @c audio 유효 시 사운드).
    /// @param worldPos 스폰 위치 (월드 좌표).
    void SpawnEnemyDeathFX(const SequenceContext& ctx, const glm::vec3& worldPos);

    /// @brief 픽업 - AudioInstance(event:/Pickup) @pos (오디오 전용, VFX 없음).
    /// @param ctx      의존 묶음 (@c fxRoot + @c audio 가 유효해야 실행).
    /// @param worldPos 스폰 위치 (월드 좌표).
    void SpawnPickupChime(const SequenceContext& ctx, const glm::vec3& worldPos);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_COMBAT_SEQUENCES_H__
