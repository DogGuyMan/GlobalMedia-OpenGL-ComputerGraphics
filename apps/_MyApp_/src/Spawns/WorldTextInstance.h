#ifndef __TOPDOWNSHOOTER_SPAWNS_WORLD_TEXT_INSTANCE_H__
#define __TOPDOWNSHOOTER_SPAWNS_WORLD_TEXT_INSTANCE_H__

#include <vmath.h>
#include <string>

namespace SJH
{
    namespace Scene { class Actor; }
    namespace Text  { class BitmapFont; }
}

namespace TopdownShooter::Spawns
{
    /// @brief 월드 텍스트 외형/모션 파라미터 (spec §4.2).
    struct WorldTextStyle
    {
        vmath::vec4 color       = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        float       charHeight  = 0.5f;    // 월드 단위 (글리프 높이)
        float       riseHeight  = 0.7f;    // 월드 +Y 상승량
        float       durationSec = 0.9f;    // 수명
        float       fadeStart   = 0.45f;   // 진행률 0..1 — 이 지점부터 alpha 1→0
    };

    /// @brief 월드 위치에 떠오르며 사라지는 텍스트 spawn (VfxInstance 패턴). font==nullptr → no-op.
    ///        Actor + TextRenderer + TweenPlayable(상승·페이드) + AutoDespawnOnFinish + Play.
    ///        despawn 은 기존 SweepFinishedChildren(fxParent) 가 수행.
    void SpawnWorldText(SJH::Scene::Actor& fxParent, SJH::Text::BitmapFont* font,
                        const vmath::vec3& worldPos, const std::string& text,
                        const WorldTextStyle& style);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_WORLD_TEXT_INSTANCE_H__
