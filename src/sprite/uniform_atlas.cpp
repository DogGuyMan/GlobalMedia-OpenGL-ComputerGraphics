#define STB_IMAGE_IMPLEMENTATION   // ← stb_image 정의 책임 단일 위치 (spec 부록 B.5)
#include "stb_image.h"

#include "uniform_atlas.h"
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

    UniformAtlas::~UniformAtlas()
    {
        Release();
    }

    bool UniformAtlas::LoadFromPNG(const char* path, int tilePx)
    {
        if (!path || tilePx <= 0) {
            spdlog::error("[UniformAtlas] invalid args: path={}, tilePx={}",
                           path ? path : "(null)", tilePx);
            return false;
        }

        stbi_set_flip_vertically_on_load(true);   // OpenGL V축 보정 (spec 부록 B.1)

        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = stbi_load(path, &w, &h, &channels, 4);
        if (!pixels) {
            spdlog::error("[UniformAtlas] load failed: {} ({})",
                           path, stbi_failure_reason());
            return false;
        }
        if (w % tilePx != 0 || h % tilePx != 0) {
            spdlog::error("[UniformAtlas] atlas size {}x{} not divisible by tile {}",
                           w, h, tilePx);
            stbi_image_free(pixels);
            return false;
        }

        mAtlasWidth  = w;
        mAtlasHeight = h;
        mTileSize    = tilePx;
        mCols        = w / tilePx;
        mRows        = h / tilePx;

        glGenTextures(1, &mTextureId);
        glBindTexture(GL_TEXTURE_2D, mTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);  // 픽셀아트
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(pixels);

        spdlog::info("[UniformAtlas] loaded {} ({}x{}, tile={}, {}x{} grid)",
                      path, w, h, tilePx, mCols, mRows);
        return true;
    }

    void UniformAtlas::Release()
    {
        if (mTextureId) {
            glDeleteTextures(1, &mTextureId);
            mTextureId = 0;
        }
    }

    vmath::vec4 UniformAtlas::GetUVRect(int frameIdx) const
    {
        return ComputeUVRect(frameIdx, mCols, mTileSize, mAtlasWidth, mAtlasHeight);
    }
}
