#include "Spawns/VfxInstance.h"
#include "Spawns/AutoDespawnOnFinish.h"
#include "VFX/VFXSystem.h"
#include "VFX/EffekseerPlayable.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Spawns
{
    void SpawnVfxInstance(SJH::Scene::Actor& fxParent, VFX::VFXSystem* vfx,
                          SJH::Effect* effect, const vmath::vec3& pos)
    {
        if (!vfx || !effect) return; // 이펙트/시스템 미존재 — no-op

        auto* a = fxParent.AddChild(std::make_unique<SJH::Scene::Actor>("VfxInstance"));
        auto* p = a->AddComponent<VFX::EffekseerPlayable>(
            vfx->GetManager(), effect, pos, VFX::TrackPolicy::Static);
        a->AddComponent<AutoDespawnOnFinish>(p);
        p->Play();
    }
}
