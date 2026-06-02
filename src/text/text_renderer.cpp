#include "text/text_renderer.h"

#include "text/bitmap_font.h"
#include "sprite/sprite_component.h"   // SJH::Sprite::SpriteRenderer

#include <memory>
#include <string>

namespace SJH::Text
{
    void TextRenderer::SetText(const std::string& s)
    {
        mText = s;
        Rebuild();
    }

    void TextRenderer::SetColor(const vmath::vec4& rgba)
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

        // 기존 글리프 제거 (re-SetText) — 스폰/SetText 시점(Update 트리 순회 밖, iterator 안전)
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
                t.Translate = vmath::vec3(penX + glyphW * 0.5f, mCharHeight * 0.5f, 0.0f); // 하단중앙
                t.Scale     = vmath::vec3(glyphW, mCharHeight, 1.0f);                       // 빌보드 sx/sy
                auto* sr = glyph->AddComponent<SJH::Sprite::SpriteRenderer>(mFont->GetAtlas());
                sr->frameIdx = frame;
                sr->tint     = mColor;
                mGlyphs.push_back(glyph);
            }
            penX += advW;
        }
    }
}
