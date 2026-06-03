#ifndef __SJH_TEXT_BITMAP_FONT_H__
#define __SJH_TEXT_BITMAP_FONT_H__

#include <cstdint>
#include <string>
#include <unordered_map>

namespace SJH { class ResourceRegistry; }
namespace SJH::Sprite { class UniformAtlas; }

namespace SJH::Text
{
    /// @brief 한 글리프의 atlas frame index + advance(px).
    struct Glyph { int frameIndex = -1; int xadvance = 0; };

    /// @brief BMFont(AngelCode) PNG+XML 비트맵 폰트 — codepoint->{frame,advance} + 공유 UniformAtlas.
    /// @note  균일 그리드 BMFont 서브셋 전용(frostyfreeze/minogram). 범용 XML 파서 아님.
    ///        내부 UniformAtlas 는 ResourceRegistry 캐시(비소유) — 사이클 회피(spec §2.2).
    class BitmapFont
    {
      public:
        BitmapFont() = default;

        /// @brief PNG+XML 로드. 내부 UniformAtlas 는 reg 캐시(Find 우선). 실패 시 빈 폰트(IsValid()=false).
        static BitmapFont LoadFromBMFont(SJH::ResourceRegistry& reg, const std::string& key,
                                         const std::string& pngPath, const std::string& xmlPath);

        const Glyph* Find(std::uint32_t codepoint) const;
        int   FrameOf(std::uint32_t codepoint) const;    // 없으면 '?'->space fallback, 둘 다 없으면 -1(skip)
        int   AdvanceOf(std::uint32_t codepoint) const;  // px (fallback 동일)
        SJH::Sprite::UniformAtlas* GetAtlas() const { return mAtlas; }
        int   LineHeight() const { return mLineHeight; }
        int   CellW() const { return mCellW; }
        int   CellH() const { return mCellH; }
        bool  IsValid() const { return mAtlas != nullptr && !mGlyphs.empty(); }

      private:
        SJH::Sprite::UniformAtlas* mAtlas = nullptr;   // 비소유 (registry 보유)
        std::unordered_map<std::uint32_t, Glyph> mGlyphs;
        int mLineHeight = 0, mCellW = 0, mCellH = 0;
    };
}

#endif // __SJH_TEXT_BITMAP_FONT_H__
