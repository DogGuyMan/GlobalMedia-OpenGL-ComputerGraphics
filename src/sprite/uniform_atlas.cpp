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

    bool UniformAtlas::LoadFromPNG(const char* /*path*/, int /*tilePx*/)
    {
        // Task 2 에서 구현
        return false;
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
