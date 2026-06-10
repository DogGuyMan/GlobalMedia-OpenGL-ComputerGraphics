/**
 * @file AmbientSequences.cpp
 * @brief AmbientSequences.h 구현 — BuildBGM 본체.
 *
 * @details
 *  - @c AudioSystem::LoadEvent 로 event:/BGM FMOD 이벤트를 로드한다.
 *  - "BgmActor" 를 sceneRoot 자식으로 생성한 뒤 @c FmodStudioPlayable 을 Loop 모드로 부착.
 *  - Play() 호출 생략 — @c TitleState::OnEnter 가 재생 시점을 단독으로 소유한다.
 *    (startup 에서 미리 Play 하면 Title 진입 Play 와 이중 재생됨.)
 *
 * @note FMOD 이벤트가 미존재(LoadEvent 반환 nullptr)이면 Actor 생성 없이 즉시 반환.
 */
#include "Spawns/AmbientSequences.h"
#include "Audio/AudioSystem.h"
#include "Audio/Constants.h"          // EVENT_BGM (이벤트 경로 상수)
#include "Audio/FmodStudioPlayable.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Spawns
{
    void BuildBGM(const SequenceContext& ctx)
    {
        if (!ctx.audio || !ctx.sceneRoot) return;
        auto* evt = ctx.audio->LoadEvent(Audio::EVENT_BGM);
        if (!evt) return; // 이벤트 미존재 — no-op

        auto* bgmActor = ctx.sceneRoot->AddChild(std::make_unique<SJH::Scene::Actor>("BgmActor"));
        auto* bgm = bgmActor->AddComponent<TopdownShooter::Audio::FmodStudioPlayable>(evt);
        bgm->SetIsLoop(true);
        // Play() 는 여기서 안 함 — Stage FSM 의 TitleState::OnEnter 가 재생 시작을 소유.
        //   (startup 에서 미리 틀면 Title 진입 Play 와 겹쳐 두 겹 재생됨.)
    }
}
