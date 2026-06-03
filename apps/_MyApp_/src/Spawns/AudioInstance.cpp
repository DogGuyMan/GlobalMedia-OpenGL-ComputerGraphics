#include "Spawns/AudioInstance.h"
#include "Spawns/AutoDespawnOnFinish.h"
#include "Audio/FmodStudioPlayable.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Spawns
{
    void SpawnAudioInstance(SJH::Scene::Actor& fxParent,
                            ::FMOD::Studio::EventDescription* desc,
                            std::optional<vmath::vec3> pos)
    {
        if (!desc) return; // 이벤트 미존재 — no-op (LoadEvent 가 nullptr 반환한 경우)

        auto* a = fxParent.AddChild(std::make_unique<SJH::Scene::Actor>("AudioInstance"));
        // FmodStudioPlayable — 이벤트 instance 생성+start. pos 있으면 3D.
        auto* p = a->AddComponent<TopdownShooter::Audio::FmodStudioPlayable>(desc, pos);
        // 종료 감지 -> 다음 sweep 에서 Actor 파괴 (메모리 자동 관리).
        a->AddComponent<AutoDespawnOnFinish>(p);
        p->Play();
    }
}
