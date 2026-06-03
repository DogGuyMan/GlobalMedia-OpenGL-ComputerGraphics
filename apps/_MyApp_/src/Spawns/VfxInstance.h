#ifndef __TOPDOWNSHOOTER_SPAWNS_VFX_INSTANCE_H__
#define __TOPDOWNSHOOTER_SPAWNS_VFX_INSTANCE_H__

#include <vmath.h>

// fwd
namespace SJH { class Effect; }
namespace SJH::Scene { class Actor; }
namespace TopdownShooter::VFX { class VFXSystem; }

namespace TopdownShooter::Spawns
{
    /// @brief 단발(one-shot) VFX 프리미티브 — AudioInstance 의 Effekseer 대칭판.
    /// @details
    ///   - fxParent 밑에 "VfxInstance" Actor 스폰 + EffekseerPlayable(Static) + AutoDespawnOnFinish + Play.
    ///   - 이펙트 종료 시 finished_ -> mDone -> SweepFinishedChildren 가 Actor 파괴 (메모리 자동 관리).
    ///   - 조립(composition) — 상속 아님. Actor 비상속 + 기존 부품(EffekseerPlayable + AutoDespawnOnFinish) 재사용.
    /// @param effect ResourceRegistry::FindEffect 결과. nullptr 이면 no-op (이펙트 미존재 안전).
    void SpawnVfxInstance(SJH::Scene::Actor& fxParent, VFX::VFXSystem* vfx,
                          SJH::Effect* effect, const vmath::vec3& pos);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_VFX_INSTANCE_H__
