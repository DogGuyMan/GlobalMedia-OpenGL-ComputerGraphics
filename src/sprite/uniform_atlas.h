#ifndef __SJH_SPRITE_UNIFORM_ATLAS_H__
#define __SJH_SPRITE_UNIFORM_ATLAS_H__

#include "resource_registry/texture.h"   // SJH::Texture / SJH::TextureUPtr (CLASS_PTR)
#include "GL/gl3w.h"
#include <vmath.h>

namespace SJH::Sprite
{
    /// @brief frameIdx → atlas UV rect (uMin, vMin, uSize, vSize) 0..1 정규화.
    /// @param frameIdx    0-based frame index (row-major: col = idx % cols, row = idx / cols)
    /// @param cols        그리드 column 수 (atlas 가로 = cols × tileSize)
    /// @param tileSize    정사각 tile 한 변 픽셀 수
    /// @param atlasWidth  atlas 전체 가로 픽셀 (= cols × tileSize)
    /// @param atlasHeight atlas 전체 세로 픽셀 (= rows × tileSize)
    /// @return vmath::vec4 UV rect. cols<=0 또는 atlasWidth/Height<=0 이면 zero rect.
    /// @note GL 호출 없음 — 순수 math. 단위 테스트가 GL fixture 없이 검증.
    vmath::vec4 ComputeUVRect(int frameIdx, int cols, int tileSize,
                               int atlasWidth, int atlasHeight);

    /// @brief 등간격 N×M 정사각 그리드 atlas — sprite frame 시퀀스의 1차원 인덱스 → 2D UV rect 변환.
    /// @details
    ///   - PNG 한 장에 동일 tile 크기 sprite N×M 행렬로 배치
    ///   - frameIdx 가 row-major (col = idx % cols, row = idx / cols)
    ///   - GL texture 는 @c SJH::Texture (RAII) 가 보유 — 본 클래스는 grid metadata + texture 위탁
    ///   - stb_image 직접 호출 안 함 — @c SJH::Image::Load 위임 (spec §B.5)
    class UniformAtlas
    {
    public:
        UniformAtlas() = default;
        ~UniformAtlas() = default;   // mTexture 가 자동 소멸 (SJH::Texture::~Texture 가 glDeleteTextures)

        UniformAtlas(const UniformAtlas&)            = delete;
        UniformAtlas& operator=(const UniformAtlas&) = delete;
        UniformAtlas(UniformAtlas&&)                 = default;
        UniformAtlas& operator=(UniformAtlas&&)      = default;

        /// @brief PNG 로드 (Image 위임) + GL_TEXTURE_2D 생성 (Texture 위임) + 픽셀아트 매개변수 (NEAREST).
        /// @return 성공 시 true. 실패 시 spdlog::error 출력 후 false (mTexture 안 생성).
        bool LoadFromPNG(const char* path, int tilePx);

        /// @brief Texture 명시 해제. 소멸자가 자동 호출하지만 명시 해제 가능.
        void Release();

        /// @brief frameIdx → atlas UV rect 0..1 정규화. ComputeUVRect 위임.
        vmath::vec4 GetUVRect(int frameIdx) const;

        /// @brief 등록된 atlas 의 전체 frame 수 (cols × rows).
        int FrameCount() const { return mCols * mRows; }

        // === Accessors ===
        /// @brief GL 텍스처 핸들 — main.cpp 의 glBindTexture 등에 사용. 미로드 시 0.
        GLuint TextureId()   const { return mTexture ? mTexture->GetTextureID() : 0; }
        int    AtlasWidth()  const { return mAtlasWidth; }
        int    AtlasHeight() const { return mAtlasHeight; }
        int    TileSize()    const { return mTileSize; }
        int    Cols()        const { return mCols; }
        int    Rows()        const { return mRows; }
        /// @brief Texture* 직접 접근 — RenderSystem 등이 필요 시 사용.
        const SJH::Texture* GetTexture() const { return mTexture.get(); }

    private:
        SJH::TextureUPtr mTexture;          // GL 텍스처 RAII 위탁
        int    mAtlasWidth  = 0;
        int    mAtlasHeight = 0;
        int    mTileSize    = 64;
        int    mCols        = 0;
        int    mRows        = 0;
    };
}

#endif // __SJH_SPRITE_UNIFORM_ATLAS_H__
