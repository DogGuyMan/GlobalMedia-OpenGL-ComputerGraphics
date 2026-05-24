#include "uniform_atlas.h"

#include "resource_registry/image.h"   // SJH::Image::Load
#include "GL/gl3w.h"
#include <spdlog/spdlog.h>

namespace SJH::Sprite
{
    vmath::vec4 ComputeUVRect(int frameIdx, int cols, int tileSize,
                               int atlasWidth, int atlasHeight)
    {
        if (cols <= 0 || atlasWidth <= 0 || atlasHeight <= 0) {
            return vmath::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        int col = frameIdx % cols;
        int row = frameIdx / cols;
        float u  = static_cast<float>(col * tileSize) / static_cast<float>(atlasWidth);
        float v  = static_cast<float>(row * tileSize) / static_cast<float>(atlasHeight);
        float du = static_cast<float>(tileSize)        / static_cast<float>(atlasWidth);
        float dv = static_cast<float>(tileSize)        / static_cast<float>(atlasHeight);
        return vmath::vec4(u, v, du, dv);
    }

    bool UniformAtlas::LoadFromPNG(const char* path, int tilePx)
    {
        if (!path || tilePx <= 0) {
            spdlog::error("[UniformAtlas] invalid args: path={}, tilePx={}",
                           path ? path : "(null)", tilePx);
            return false;
        }

        // === 1. PNG 디코드 — SJH::Image 위임 (stbi_load + V축 보정 + RAII) ===
        auto image = SJH::Image::Load(/*image_name=*/path, /*filepath=*/path);
        if (!image) {
            spdlog::error("[UniformAtlas] Image::Load failed: {}", path);
            return false;
        }

        const int w = image->GetWidth();
        const int h = image->GetHeight();
        if (w % tilePx != 0 || h % tilePx != 0) {
            spdlog::error("[UniformAtlas] atlas size {}x{} not divisible by tile {}",
                           w, h, tilePx);
            return false;
        }

        // === 2. grid metadata ===
        mAtlasWidth  = w;
        mAtlasHeight = h;
        mTileSize    = tilePx;
        mCols        = w / tilePx;
        mRows        = h / tilePx;

        // === 3. GL 텍스처 업로드 — SJH::Texture 위임 (glGenTextures + glTexImage2D + RAII) ===
        mTexture = SJH::Texture::CreateTexture(image.get());
        if (!mTexture) {
            spdlog::error("[UniformAtlas] Texture::CreateTexture failed: {}", path);
            return false;
        }

        // === 4. 픽셀아트 매개변수 — Texture 의 default (LINEAR_MIPMAP_LINEAR/LINEAR + CLAMP_TO_EDGE)
        //        에 의존하지 않고 명시 — UniformAtlas 가 Texture 내부 default 와 분리. (Task 4 fixup follow-up) ===
        mTexture->Bind();
        mTexture->SetFilter(GL_NEAREST, GL_NEAREST);
        mTexture->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);   // 픽셀아트 — 인접 tile bleed 방지

        spdlog::info("[UniformAtlas] loaded {} ({}x{}, tile={}, {}x{} grid)",
                      path, w, h, tilePx, mCols, mRows);
        return true;
    }

    void UniformAtlas::Release()
    {
        mTexture.reset();   // SJH::Texture::~Texture 가 glDeleteTextures 자동 호출
    }

    vmath::vec4 UniformAtlas::GetUVRect(int frameIdx) const
    {
        return ComputeUVRect(frameIdx, mCols, mTileSize, mAtlasWidth, mAtlasHeight);
    }
}
