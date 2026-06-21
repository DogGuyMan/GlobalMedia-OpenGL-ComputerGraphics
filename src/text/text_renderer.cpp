/**
 * @file text_renderer.cpp
 * @brief TextRenderer 구현 - 글리프 child Actor 재구성, tint/alpha 일괄 갱신.
 *
 * @details
 *  ### 책임
 *  - @c SetText -> @c Rebuild: owner 의 기존 글리프 child 제거 -> center offset 산출 ->
 *    문자별 child Actor + @c SpriteRenderer 추가.
 *  - @c SetColor / @c SetAlpha: @c mGlyphs 벡터를 순회해 각 @c SpriteRenderer 의 tint 갱신.
 *
 *  ### 비-책임
 *  - [X] 글리프 child 소유 - owner Actor 의 children 트리가 소유.
 *  - [X] Tween 구동 - Client 에서 @c SetAlpha / @c SetColor 를 직접 호출.
 *
 * @note @c Rebuild 는 Update 트리 순회 *밖*에서 호출해야 iterator 안전 (AddChild/RemoveChild 무효화 방지).
 */
#include "text/text_renderer.h"

#include "text/bitmap_font.h"
#include "sprite/sprite_component.h"   // SJH::Sprite::SpriteRenderer
#include "resource_registry/sprite_resources.h" // SJH::SpriteResources (D8 DI - plane/material 해결)

#include <memory>
#include <string>

namespace SJH::Text
{
    void TextRenderer::SetText(const std::string& s)
    {
        mText = s;
        Rebuild();
    }

    void TextRenderer::SetColor(const glm::vec4& rgba)
    {
        mColor = rgba;
        for (auto* g : mGlyphs)
            if (auto* sr = g->GetComponent<SJH::Sprite::SpriteRenderer>())
                sr->tint = mColor;
    }

    void TextRenderer::SetAlpha(float a)
    {
        mColor[3] = a;
        for (auto* g : mGlyphs)
            if (auto* sr = g->GetComponent<SJH::Sprite::SpriteRenderer>())
                sr->tint[3] = a;
    }

    void TextRenderer::Rebuild()
    {
        auto* owner = GetOwner();
        if (!owner) return;

        // 기존 글리프 제거 (re-SetText) - 스폰/SetText 시점(Update 트리 순회 밖, iterator 안전)
        for (auto* g : mGlyphs) owner->RemoveChild(g);
        mGlyphs.clear();

        if (!mFont || !mFont->GetAtlas() || mFont->CellH() <= 0) return;

        const float worldPerPx = mCharHeight / static_cast<float>(mFont->CellH());
        const float glyphW      = static_cast<float>(mFont->CellW()) * worldPerPx;

        float totalW = 0.0f;
        for (char c : mText)
            totalW += static_cast<float>(mFont->AdvanceOf(static_cast<unsigned char>(c))) * worldPerPx;
        float penX = -totalW * 0.5f;   // center

        for (char c : mText)
        {
            const auto  cp    = static_cast<unsigned char>(c);
            const int   frame = mFont->FrameOf(cp);
            const float advW  = static_cast<float>(mFont->AdvanceOf(cp)) * worldPerPx;
            if (frame >= 0)
            {
                auto* glyph = owner->AddChild(std::make_unique<SJH::Scene::Actor>("glyph"));
                auto& t = glyph->GetTransform();
                t.Translate = glm::vec3(penX + glyphW * 0.5f, mCharHeight * 0.5f, 0.0f); // 하단중앙
                t.Scale     = glm::vec3(glyphW, mCharHeight, 1.0f);                       // 빌보드 sx/sy
                // D8 DI - 공유 자원(plane/material)을 SpriteResources 로 해결 후 주입 (미주입 시 글리프 무성 소멸).
                auto* atlas = mFont->GetAtlas();
                auto* mesh  = SJH::SpriteResources::EnsureSharedPlane();
                auto* mat   = SJH::SpriteResources::CreateInstanceMaterial(atlas);
                auto* sr = glyph->AddComponent<SJH::Sprite::SpriteRenderer>(atlas, mesh, mat);
                sr->frameIdx = frame;
                sr->tint     = mColor;
                mGlyphs.push_back(glyph);
            }
            penX += advW;
        }
    }
}
