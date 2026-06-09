/**
 * @file uniform_atlas.cpp
 * @brief UniformAtlas Fluent Builder 구현 - PNG 로드/GL texture 업로드/grid 설정/UV 변환.
 *
 * @details
 *  ### 책임
 *  - @c LoadFromPNG : @c SJH::Image::Load (stb_image 위임) -> @c SJH::Texture::CreateTexture (GL 위임) ->
 *    NEAREST + CLAMP_TO_EDGE 필터 설정.
 *  - @c SetGrid : cols/rows 명시, tileSize(U) = atlasW/cols, tileHeight(V) = atlasH/rows 도출.
 *  - @c SetTileSize : tilePx 명시, cols = atlasW/tilePx, rows = atlasH/tilePx 도출 (정사각 tile).
 *  - @c ComputeUVRect : 정사각/비정사각 두 오버로드 - GL 호출 없는 순수 math.
 *
 *  ### 비-책임
 *  - [X] stb_image 직접 호출 금지 - @c SJH::Image::Load 위임.
 *  - [X] glGenTextures 직접 호출 금지 - @c SJH::Texture::CreateTexture 위임.
 *
 * @note ResourceRegistry 키 컨벤션('_' 접두) 은 @c sprite_component.cpp 참조.
 */
#include "uniform_atlas.h"

#include "resource_registry/image.h"   // SJH::Image::Load
#include "GL/gl3w.h"
#include <spdlog/spdlog.h>

namespace SJH::Sprite
{
    vmath::vec4 ComputeUVRect(int frameIdx, int cols, int tileW, int tileH,
                               int atlasWidth, int atlasHeight)
    {
        if (cols <= 0 || atlasWidth <= 0 || atlasHeight <= 0) {
            return vmath::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        int col = frameIdx % cols;
        int row = frameIdx / cols;
        float u  = static_cast<float>(col * tileW) / static_cast<float>(atlasWidth);
        float v  = static_cast<float>(row * tileH) / static_cast<float>(atlasHeight);
        float du = static_cast<float>(tileW)       / static_cast<float>(atlasWidth);
        float dv = static_cast<float>(tileH)       / static_cast<float>(atlasHeight);
        return vmath::vec4(u, v, du, dv);
    }

    vmath::vec4 ComputeUVRect(int frameIdx, int cols, int tileSize,
                               int atlasWidth, int atlasHeight)
    {
        // 정사각 편의 오버로드 - tileW=tileH=tileSize 위임 (기존 시그니처/단위 테스트 호환).
        return ComputeUVRect(frameIdx, cols, tileSize, tileSize, atlasWidth, atlasHeight);
    }

    UniformAtlas& UniformAtlas::LoadFromPNG(const char* path)
    {
        if (!path) {
            spdlog::error("[UniformAtlas] LoadFromPNG: null path");
            return *this;
        }

        // === 1. PNG 디코드 - SJH::Image 위임 (stbi_load + V축 보정 + RAII) ===
        auto image = SJH::Image::Load(/*image_name=*/path, /*filepath=*/path);
        if (!image) {
            spdlog::error("[UniformAtlas] Image::Load failed: {}", path);
            return *this;
        }

        mAtlasWidth  = image->GetWidth();
        mAtlasHeight = image->GetHeight();

        // === 2. GL 텍스처 업로드 - SJH::Texture 위임 (glGenTextures + glTexImage2D + RAII) ===
        mTexture = SJH::Texture::CreateTexture(image.get());
        if (!mTexture) {
            spdlog::error("[UniformAtlas] Texture::CreateTexture failed: {}", path);
            return *this;
        }

        // === 3. 픽셀아트 매개변수 - NEAREST + CLAMP_TO_EDGE (인접 tile bleed 방지) ===
        mTexture->Bind();
        mTexture->SetFilter(GL_NEAREST, GL_NEAREST);
        mTexture->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        spdlog::info("[UniformAtlas] loaded {} ({}x{}) - grid 미설정, SetGrid/SetTileSize 호출 필요",
                      path, mAtlasWidth, mAtlasHeight);
        return *this;
    }

    UniformAtlas& UniformAtlas::SetGrid(int cols, int rows)
    {
        if (cols <= 0 || rows <= 0 || mAtlasWidth <= 0 || mAtlasHeight <= 0) {
            spdlog::error("[UniformAtlas] SetGrid: invalid (cols={}, rows={}, atlas={}x{}) - LoadFromPNG 먼저 호출",
                           cols, rows, mAtlasWidth, mAtlasHeight);
            return *this;
        }
        if (mAtlasWidth % cols != 0 || mAtlasHeight % rows != 0) {
            spdlog::error("[UniformAtlas] atlas size {}x{} not divisible by grid {}x{}",
                           mAtlasWidth, mAtlasHeight, cols, rows);
            return *this;
        }
        mCols       = cols;
        mRows       = rows;
        mTileSize   = mAtlasWidth  / cols;   // tile 가로(U)
        mTileHeight = mAtlasHeight / rows;   // tile 세로(V) - 비정사각 폰트(6x10) 지원 (이전엔 square 가정으로 V 잘림)
        spdlog::info("[UniformAtlas] grid set ({}x{} grid, tile={}x{})", mCols, mRows, mTileSize, mTileHeight);
        return *this;
    }

    UniformAtlas& UniformAtlas::SetTileSize(int tilePx)
    {
        if (tilePx <= 0 || mAtlasWidth <= 0 || mAtlasHeight <= 0) {
            spdlog::error("[UniformAtlas] SetTileSize: invalid (tilePx={}, atlas={}x{}) - LoadFromPNG 먼저 호출",
                           tilePx, mAtlasWidth, mAtlasHeight);
            return *this;
        }
        if (mAtlasWidth % tilePx != 0 || mAtlasHeight % tilePx != 0) {
            spdlog::error("[UniformAtlas] atlas size {}x{} not divisible by tile {}",
                           mAtlasWidth, mAtlasHeight, tilePx);
            return *this;
        }
        mTileSize   = tilePx;
        mTileHeight = tilePx;   // 정사각 (tilePx 명시 경로)
        mCols       = mAtlasWidth / tilePx;
        mRows       = mAtlasHeight / tilePx;
        spdlog::info("[UniformAtlas] tileSize set (tile={}, {}x{} grid)", tilePx, mCols, mRows);
        return *this;
    }

    void UniformAtlas::Release()
    {
        mTexture.reset();   // SJH::Texture::~Texture 가 glDeleteTextures 자동 호출
    }

    vmath::vec4 UniformAtlas::GetUVRect(int frameIdx) const
    {
        return ComputeUVRect(frameIdx, mCols, mTileSize, mTileHeight, mAtlasWidth, mAtlasHeight);
    }
}
