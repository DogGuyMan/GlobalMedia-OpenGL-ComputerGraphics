#include "text/bitmap_font.h"

#include "resource_registry/resource_registry.h"

#include <spdlog/spdlog.h>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace SJH::Text
{
    namespace
    {
        // BMFont 속성 추출 — `name="123"` 의 정수. atoi 가 닫는 따옴표에서 멈춤.
        int AttrInt(const std::string& s, const char* name, int def = 0)
        {
            const std::string key = std::string(name) + "=\"";
            const auto p = s.find(key);
            if (p == std::string::npos) return def;
            return std::atoi(s.c_str() + p + key.size());
        }
    }

    BitmapFont BitmapFont::LoadFromBMFont(SJH::ResourceRegistry& reg, const std::string& key,
                                          const std::string& pngPath, const std::string& xmlPath)
    {
        BitmapFont font;

        std::ifstream in(xmlPath, std::ios::binary);   // 바이너리 모드 (크로스플랫폼)
        if (!in)
        {
            spdlog::warn("[BitmapFont] XML 열기 실패: {}", xmlPath);
            return font;
        }
        std::stringstream buf;
        buf << in.rdbuf();
        const std::string content = buf.str();

        struct Raw { std::uint32_t id; int x, y, w, h, adv; };
        std::vector<Raw> chars;
        int scaleW = 0, scaleH = 0;

        std::istringstream lines(content);
        std::string line;
        while (std::getline(lines, line))
        {
            if (line.find("<common") != std::string::npos)
            {
                scaleW          = AttrInt(line, "scaleW");
                scaleH          = AttrInt(line, "scaleH");
                font.mLineHeight = AttrInt(line, "lineHeight");
            }
            else if (line.find("<char ") != std::string::npos)
            {
                Raw r;
                r.id  = static_cast<std::uint32_t>(AttrInt(line, "id"));
                r.x   = AttrInt(line, "x");
                r.y   = AttrInt(line, "y");
                r.w   = AttrInt(line, "width");
                r.h   = AttrInt(line, "height");
                r.adv = AttrInt(line, "xadvance");
                chars.push_back(r);
            }
        }

        if (chars.empty() || scaleW <= 0 || scaleH <= 0)
        {
            spdlog::warn("[BitmapFont] 파싱 실패(char 0 또는 scale 0): {}", xmlPath);
            return font;
        }
        font.mCellW = chars[0].w;
        font.mCellH = chars[0].h;
        if (font.mCellW <= 0 || font.mCellH <= 0)
        {
            spdlog::warn("[BitmapFont] cell 크기 0: {}", xmlPath);
            return font;
        }
        const int cols = scaleW / font.mCellW;
        const int rows = scaleH / font.mCellH;

        // UniformAtlas — Find 우선(중복키 공유), 없으면 Create
        font.mAtlas = reg.FindUniformAtlas(key);
        if (!font.mAtlas) font.mAtlas = reg.CreateUniformAtlas(key, pngPath, cols, rows);
        if (!font.mAtlas)
        {
            spdlog::warn("[BitmapFont] UniformAtlas 생성 실패: {} ({}x{})", pngPath, cols, rows);
            return font;
        }

        for (const auto& r : chars)
        {
            // 텍스처 V-flip 보정 — SJH::Image::Load 가 PNG 를 상하 반전해 GL 업로드하므로
            // (texture V=0 = PNG 맨 아래 행), BMFont 의 y(위->아래) 를 그대로 frame 행으로 쓰면
            // 행이 뒤집혀 엉뚱한 글자가 샘플링된다. frameRow = (rows-1) - PNG행 으로 보정.
            // (minogram = 이 엔진 첫 다중행 atlas 라 여기서 발견 — Nx1 스트립은 row=0 뿐이라 무관했음.)
            const int frameRow = (rows - 1) - (r.y / font.mCellH);
            const int frame    = frameRow * cols + (r.x / font.mCellW);
            font.mGlyphs[r.id] = Glyph{frame, r.adv};
        }
        return font;
    }

    const Glyph* BitmapFont::Find(std::uint32_t cp) const
    {
        auto it = mGlyphs.find(cp);
        return it == mGlyphs.end() ? nullptr : &it->second;
    }

    int BitmapFont::FrameOf(std::uint32_t cp) const
    {
        if (auto* g = Find(cp)) return g->frameIndex;
        if (auto* q = Find('?')) return q->frameIndex;   // fallback 1
        if (auto* s = Find(' ')) return s->frameIndex;   // fallback 2
        return -1;                                        // skip
    }

    int BitmapFont::AdvanceOf(std::uint32_t cp) const
    {
        if (auto* g = Find(cp)) return g->xadvance;
        if (auto* q = Find('?')) return q->xadvance;
        if (auto* s = Find(' ')) return s->xadvance;
        return mCellW;                                    // 최후 fallback
    }
}
