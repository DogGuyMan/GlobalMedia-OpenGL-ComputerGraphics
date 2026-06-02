#ifndef __SJH_TEXT_TEXT_RENDERER_H__
#define __SJH_TEXT_TEXT_RENDERER_H__

#include "scene/actor.h"   // Component 베이스 + Actor
#include <vmath.h>
#include <string>
#include <vector>

namespace SJH::Text
{
    class BitmapFont;

    /// @brief α — 문자열을 글자마다 child Actor(SpriteRenderer 빌보드)로 펼치는 Component.
    /// @details center 정렬 + 하단중앙 앵커 + 월드 X 배치(spec §3). TweenPlayable 비의존
    ///          (SetAlpha/SetColor 만 노출, 구동은 Client).
    class TextRenderer : public SJH::Scene::Component
    {
      public:
        explicit TextRenderer(const BitmapFont* font) : mFont(font) {}

        void OnEnter() override {}
        void OnExit()  override { mGlyphs.clear(); }   // child 는 owner 소멸 시 자동 파괴
        void Update(float) override {}                  // 정적 — 틱 없음

        void SetText(const std::string& s);             // 글리프 child 재구성(Rebuild)
        void SetColor(const vmath::vec4& rgba);          // 모든 글리프 tint
        void SetAlpha(float a);                          // fade — 모든 글리프 tint.a
        void SetCharHeight(float worldH) { mCharHeight = worldH; }  // SetText 전 설정

      private:
        void Rebuild();

        const BitmapFont* mFont = nullptr;
        std::string       mText;
        float             mCharHeight = 0.5f;
        vmath::vec4       mColor = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        std::vector<SJH::Scene::Actor*> mGlyphs;         // 비소유 (owner children 소유)
    };
}

#endif // __SJH_TEXT_TEXT_RENDERER_H__
