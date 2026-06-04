#include "Spawns/AmbientSequences.h"
#include "Audio/AudioSystem.h"
#include "Audio/FmodStudioPlayable.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Spawns
{
    void BuildBGM(const SequenceContext& ctx)
    {
        if (!ctx.audio || !ctx.sceneRoot) return;
        auto* evt = ctx.audio->LoadEvent("event:/BGM");
        if (!evt) return; // 이벤트 미존재 — no-op

        auto* bgmActor = ctx.sceneRoot->AddChild(std::make_unique<SJH::Scene::Actor>("BgmActor"));
        auto* bgm = bgmActor->AddComponent<TopdownShooter::Audio::FmodStudioPlayable>(evt);
        bgm->SetIsLoop(true);
        // Play() 는 여기서 안 함 — Stage FSM 의 TitleState::OnEnter 가 재생 시작을 소유.
        //   (startup 에서 미리 틀면 Title 진입 Play 와 겹쳐 두 겹 재생됨.)
    }
}
