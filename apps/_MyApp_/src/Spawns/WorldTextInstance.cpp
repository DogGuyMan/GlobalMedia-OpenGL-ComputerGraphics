/**
 * @file WorldTextInstance.cpp
 * @brief @c SpawnWorldText 구현 + @c TopdownShooter::WorldText 전역 파사드 구현.
 *
 * @details
 *  (1) @c SpawnWorldText -- @c TextRenderer + @c TweenPlayable + @c AutoDespawnOnFinish 조립.
 *      TweenPlayable 콜백에서 easeOutQuad 상승 + 후반 alpha 페이드를 프레임마다 적용.
 *      트윈 종료(t >= 1) 시 @c AutoDespawnOnFinish.mDone = true -> 부모 sweep 에서 Actor 제거.
 *
 *  (2) @c WorldText 파사드 -- @c gFxRoot / @c gFont 정적 변수 보관 + @c SpawnDamage 단발 호출.
 *      GL 충돌 없어 @c VfxFacade.cpp 와 달리 별도 TU 불필요 (Effekseer 미포함).
 *
 * @note tweeny step 오버로드 함정 주의: @c step(int32_t ms) 가 밀리초,
 *       @c step(float) 가 [0..1] 비율. @c durationSec -> @c int32_t ms 로 변환 필수.
 */
#include "Spawns/WorldTextInstance.h"

#include "Spawns/AutoDespawnOnFinish.h"
#include "Tween/TweenPlayable.h"
#include "text/text_renderer.h"
#include "scene/actor.h"

#include <cstdint>
#include <memory>
#include <string>

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

// ── 전투 배선 파사드 (VFX::Spawn 대칭) — Life seam 이 fxRoot/font 없이 데미지 숫자 spawn ──
//    GL 충돌 없어 별도 TU 불필요 (VfxFacade 와 달리 Effekseer 비의존).
namespace TopdownShooter::WorldText
{
    namespace
    {
        SJH::Scene::Actor*     gFxRoot = nullptr;   // main 등록 — Director.Root() 자식 "FxRoot"
        SJH::Text::BitmapFont* gFont   = nullptr;   // main 등록 — Manager.WorldText().GetFont()
    }

    void SetSpawnContext(SJH::Scene::Actor* fxRoot, SJH::Text::BitmapFont* font)
    {
        gFxRoot = fxRoot;
        gFont   = font;
    }

    void SpawnDamage(int damage, const vmath::vec3& pos)
    {
        if (gFxRoot == nullptr || gFont == nullptr) return;   // 미등록 — no-op (VFX::Spawn 동일)
        Spawns::WorldTextStyle style;
        style.color = vmath::vec4(1.0f, 0.2f, 0.2f, 1.0f);    // 빨강 (피해 강조)
        style.scale = 0.5f;
        Spawns::SpawnWorldText(*gFxRoot, gFont, pos, "-" + std::to_string(damage), style);
    }
}
