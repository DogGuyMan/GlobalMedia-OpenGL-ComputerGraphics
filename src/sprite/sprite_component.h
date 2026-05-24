#ifndef __SJH_SPRITE_SPRITE_COMPONENT_H__
#define __SJH_SPRITE_SPRITE_COMPONENT_H__

#include "scene/actor.h"   // SJH::Scene::Component
#include <vmath.h>

namespace SJH::Sprite
{
    class UniformAtlas;   // forward — 핸들 참조

    /// @brief 게임 무관 sprite 데이터 — atlas 참조 + frameIdx + size + tint + flipX.
    /// @details *시간 축 갱신 책임 없음* — frameIdx 만 보유.
    ///          SpriteSequencePlayable (M3.5 이후) 이 frameIdx 갱신.
    ///          M1 에서는 main.cpp 가 직접 frameIdx=0 설정 후 그리기.
    class SpriteComponent : public SJH::Scene::Component
    {
    public:
        SpriteComponent() = default;

        // === Lifecycle (Component 순수 가상 — 빈 override) ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override {}   // ← Playable 이 frameIdx 갱신, 본인 무동작

        // === 게임 무관 데이터 (public 멤버 직접 접근 — POD-ish) ===
        UniformAtlas* atlas    = nullptr;
        int           frameIdx = 0;
        vmath::vec2   size     = vmath::vec2(1.0f, 1.0f);   // 월드 단위
        vmath::vec4   tint     = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        bool          flipX    = false;
    };
}

#endif // __SJH_SPRITE_SPRITE_COMPONENT_H__
