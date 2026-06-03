#include "Spawns/WorldTextInstance.h"

#include "Spawns/AutoDespawnOnFinish.h"
#include "Tween/TweenPlayable.h"
#include "text/text_renderer.h"
#include "scene/actor.h"

#include <cstdint>
#include <memory>

namespace TopdownShooter::Spawns
{
    void SpawnWorldText(SJH::Scene::Actor& fxParent, SJH::Text::BitmapFont* font,
                        const vmath::vec3& worldPos, const std::string& text,
                        const WorldTextStyle& style)
    {
        if (!font) return;   // 폰트 미존재 — no-op (VfxInstance 동일)

        auto* a = fxParent.AddChild(std::make_unique<SJH::Scene::Actor>("WorldText"));
        a->GetTransform().Translate = worldPos;        // 앵커 = 하단중앙
        a->GetTransform().Scale     = vmath::vec3(style.scale, style.scale, 1.0f); // 전체 배율 — 자식 글리프 WorldMatrix 합성(빌보드 sx/sy + 배치 균일). Z 무관

        auto* tr = a->AddComponent<SJH::Text::TextRenderer>(font);
        tr->SetCharHeight(style.charHeight);           // SetText 전 설정(크기 bake)
        tr->SetColor(style.color);
        tr->SetText(text);                              // 글리프 child 빌드

        // 단일 progress 트윈 0->1 — 상승+페이드 동시 (one-shot). tweeny step(int32_t ms) 강제.
        auto tween = tweeny::from(0.0f).to(1.0f)
                         .during(static_cast<std::int32_t>(style.durationSec * 1000.0f));
        const float baseY = worldPos[1];
        auto* tw = a->AddComponent<Tween::TweenPlayable<float>>(
            std::move(tween),
            [a, tr, baseY, style](float t) {
                const float e = 1.0f - (1.0f - t) * (1.0f - t);            // easeOutQuad (팝->감속)
                a->GetTransform().Translate[1] = baseY + style.riseHeight * e;
                const float alpha = (t < style.fadeStart)
                                        ? 1.0f
                                        : 1.0f - (t - style.fadeStart) / (1.0f - style.fadeStart);
                tr->SetAlpha(alpha);                                       // 후반 페이드
            });
        tw->SetIsLoop(false);                           // t≥1 -> finished_

        a->AddComponent<AutoDespawnOnFinish>(tw);       // 종료 감지 (기존 sweeper 가 RemoveChild)
        tw->Play();
    }
}
