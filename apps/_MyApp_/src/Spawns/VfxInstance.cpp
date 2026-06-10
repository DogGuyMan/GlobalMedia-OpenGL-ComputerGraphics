/**
 * @file VfxInstance.cpp
 * @brief @c SpawnVfxInstance 구현 -- Effekseer 단발 VFX Actor 조립.
 *
 * @details
 *  @c EffekseerPlayable(Static) 과 @c AutoDespawnOnFinish 를 새 자식 Actor 에 부착하고
 *  @c Play() 를 호출해 이펙트를 즉시 시작한다.
 *  이후 수명 관리는 @c AutoDespawnOnFinish + 부모의 sweep 에 완전 위임.
 *
 * @note @c VfxFacade.cpp 와 TU 를 분리한 이유:
 *       파사드(@c VfxFacade.cpp)는 @c resource_registry.h(-> @c gl3w.h)만 포함하지만,
 *       본 파일은 @c VFXSystem.h(-> @c EffekseerRendererGL, 시스템 @c gl3.h)를 끌어온다.
 *       같은 TU 에 두면 @c gl3w <-> 시스템 @c gl3.h 의 @c PFNGL* 재정의 충돌이 발생하므로
 *       반드시 분리 유지.
 */
#include "Spawns/VfxInstance.h"
#include "Spawns/AutoDespawnOnFinish.h"
#include "VFX/VFXSystem.h"
#include "VFX/EffekseerPlayable.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Spawns
{
    void SpawnVfxInstance(SJH::Scene::Actor& fxParent, VFX::VFXSystem* vfx,
                          SJH::Effect* effect, const vmath::vec3& pos, float yaw)
    {
        if (!vfx || !effect) return; // 이펙트/시스템 미존재 — no-op

        auto* a = fxParent.AddChild(std::make_unique<SJH::Scene::Actor>("VfxInstance"));
        auto* p = a->AddComponent<VFX::EffekseerPlayable>(
            vfx->GetManager(), effect, pos, VFX::TrackPolicy::Static, yaw);
        a->AddComponent<AutoDespawnOnFinish>(p);
        p->Play();
    }
}
