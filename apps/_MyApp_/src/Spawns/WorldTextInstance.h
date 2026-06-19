/**
 * @file WorldTextInstance.h
 * @brief 월드 공간에 떠오르며 사라지는 텍스트 Actor 를 조립/스폰하는 팩토리 + 전역 파사드.
 *
 * @details
 *  ### 책임
 *  - @c SpawnWorldText: @c fxParent 밑에 "WorldText" Actor 를 생성하고
 *    @c TextRenderer + @c TweenPlayable(상승 + 페이드) + @c AutoDespawnOnFinish 를 부착 후 @c Play().
 *  - @c WorldText::SetSpawnContext / @c WorldText::SpawnDamage: main 이 1회 컨텍스트를 등록해 두면
 *    @c Life seam 이 fxRoot/font 를 직접 들지 않고 데미지 숫자를 단발 스폰 가능.
 *  ### 비-책임
 *  - [X] 글리프 빌드 상세 -- @c TextRenderer 담당.
 *  - [X] Tween 진행 -- @c TweenPlayable 담당.
 *  - [X] Actor 트리 sweep(RemoveChild) -- 부모의 @c SweepFinishedChildren 담당.
 *  ### 정통 매핑
 *  - Unity @c TextMeshPro + @c DOTween.To(alpha, 0, dur) + @c Destroy(go, dur) 패턴.
 *  - Cocos2D @c Label + @c Sequence(MoveBy, FadeOut, RemoveSelf) 패턴.
 *
 * @note @c VfxFacade.cpp 와 달리 GL 충돌이 없어 별도 TU 불필요 -- Effekseer 비의존.
 *       @p font 가 nullptr 이면 조용히 no-op (@c VfxInstance 동일).
 */
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
    /**
     * @brief 월드 텍스트 외형/모션 파라미터 (spec sec.4.2).
     * @details
     *  @c SpawnWorldText 에 전달하는 값 클래스(PoD). 각 필드는 기본값으로 흰색 0.9초 상승 텍스트.
     *  @c SpawnDamage 는 빨강 + scale = 0.5 로 오버라이드해 사용.
     */
    struct WorldTextStyle
    {
        vmath::vec4 color       = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f); ///< 텍스트 색상 (RGBA).
        float       charHeight  = 0.5f;    ///< 글리프 높이 -- 월드 단위. 최종 크기 = charHeight x scale.
        float       scale       = 1.0f;    ///< 전체 배율 (Actor.Transform.Scale 합성).
        float       riseHeight  = 0.7f;    ///< 월드 +Y 상승량(배율 영향 없음 -- 필요 시 별도 조정).
        float       durationSec = 0.9f;    ///< 텍스트 수명 (초).
        float       fadeStart   = 0.45f;   ///< 진행률 0..1 -- 이 지점부터 alpha 1->0 페이드 시작.
    };

    /// @brief 월드 위치에 떠오르며 사라지는 텍스트를 단발 스폰 (@c VfxInstance 패턴).
    /// @details
    ///  Actor + @c TextRenderer + @c TweenPlayable(상승+페이드) + @c AutoDespawnOnFinish 조립 후 @c Play().
    ///  despawn 은 @c SweepFinishedChildren(fxParent) 가 수행.
    /// @param fxParent 스폰 Actor 를 붙일 부모 Actor (FxRoot 등).
    /// @param font     글리프 atlas. nullptr 이면 no-op.
    /// @param worldPos 텍스트 앵커 월드 위치 (하단 중앙).
    /// @param text     표시할 문자열.
    /// @param style    외형/모션 파라미터.
    void SpawnWorldText(SJH::Scene::Actor& fxParent, SJH::Text::BitmapFont* font,
                        const vmath::vec3& worldPos, const std::string& text,
                        const WorldTextStyle& style);
}

namespace TopdownShooter::WorldText
{
    /// @brief 전역 WorldText 스폰 컨텍스트 등록 (main startup 1회 호출).
    ///        @c VFX::SetSpawnContext 대칭 -- @c Life seam 이 fxRoot/font 를 직접 들지 않고
    ///        @c SpawnDamage 만 호출 가능하게 한다.
    /// @param fxRoot 스폰 Actor 를 붙일 루트 Actor (Director.Root() 자식 "FxRoot").
    /// @param font   @c BitmapFont 포인터 (GameSystems.WorldText().GetFont() 등).
    void SetSpawnContext(SJH::Scene::Actor* fxRoot, SJH::Text::BitmapFont* font);

    /// @brief 데미지 숫자 단발 스폰 ("-N" 빨강 0.5 배).
    ///        컨텍스트 미등록 또는 폰트 없음 시 조용히 no-op.
    ///        @c VFX::Spawn 대칭 -- 빌더가 @c Life::SetOnDamageNumber seam 에 주입.
    /// @param damage 표시할 피해량 (양수). "-" 접두사는 내부에서 자동 추가.
    /// @param pos    월드 스폰 위치.
    void SpawnDamage(int damage, const vmath::vec3& pos);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_WORLD_TEXT_INSTANCE_H__
