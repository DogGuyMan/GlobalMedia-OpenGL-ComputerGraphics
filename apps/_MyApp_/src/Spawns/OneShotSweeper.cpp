/**
 * @file OneShotSweeper.cpp
 * @brief OneShotSweeper.h 구현 — SweepFinishedChildren 본체.
 *
 * @details
 *  @c AutoDespawnOnFinish::IsDone() 술어로 완료된 단발 FX 자식을 찾아 @c RemoveChild 로 파괴한다.
 *
 *  구현 포인트:
 *  - @c Actor::FindChildIf 는 매 호출 새 스캔 → @c RemoveChild 직후 반복자 무효화 없음.
 *  - while + FindChildIf 루프로 완료 Actor 가 0개가 될 때까지 반복 제거.
 *  - @c RemoveChild 는 @c OnExit 후 erase — FMOD/Effekseer 자원 정리는 leaf dtor 에 위임.
 *  - 보존+재부착이 필요하면 @c DetachChild 이지만, 여기선 폐기이므로 @c RemoveChild 가 정확.
 */
#include "Spawns/OneShotSweeper.h"
#include "Spawns/AutoDespawnOnFinish.h"
#include "scene/actor.h"

namespace TopdownShooter::Spawns
{
    void SweepFinishedChildren(SJH::Scene::Actor& fxParent)
    {
        // 완료(IsDone) 단발 FX 자식을 찾아 파괴. Actor::FindChildIf (술어 탐색, 2026-05-31 추가) 활용.
        // - 단발 FX 는 *파괴* 대상 -> RemoveChild (OnExit 후 erase). leaf dtor 가 FMOD/Effekseer 자원 정리.
        //   (보존+재부착이 필요하면 DetachChild 지만, 여기선 폐기이므로 RemoveChild 가 정확.)
        // - FindChildIf 는 매 호출 새 스캔 -> RemoveChild 직후 반복자 무효화 문제 없음 (live iterator 미보유).
        const auto isDone = [](const SJH::Scene::Actor* a) {
            auto* ad = a->GetComponent<AutoDespawnOnFinish>();
            return ad && ad->IsDone();
        };
        while (SJH::Scene::Actor* done = fxParent.FindChildIf(isDone))
            fxParent.RemoveChild(done);
    }
}
