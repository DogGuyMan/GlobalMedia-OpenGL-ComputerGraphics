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
                          SJH::Effect* effect, const vmath::vec3& pos, float yaw = 0.0f);
}

namespace TopdownShooter::VFX
{
    /// @brief VFX 단발 스폰 파사드 — main 이 컨텍스트(fxRoot+VFXSystem)를 1회 등록한 뒤 key 로 호출.
    /// @details 컴포넌트는 VFX 를 모르고, 빌더가 Spawn 람다를 seam 으로 주입한다 (PlayableDirector::Play 패턴).
    ///          내부에서 ResourceRegistry::Get().FindEffect(key) -> Spawns::SpawnVfxInstance 로 위임.
    ///          미등록(컨텍스트 nullptr) 또는 key 미존재면 조용히 no-op.
    /// @param yaw Y축 회전(라디안) — 발사 방향 등. 기본 0(회전 없음).
    void SetSpawnContext(SJH::Scene::Actor* fxRoot, VFXSystem* vfx);
    void Spawn(const char* key, const vmath::vec3& pos, float yaw = 0.0f);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_VFX_INSTANCE_H__
