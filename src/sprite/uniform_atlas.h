#ifndef __SJH_SPRITE_UNIFORM_ATLAS_H__
#define __SJH_SPRITE_UNIFORM_ATLAS_H__

#include "GL/gl3w.h"
#include "common/common.h"
#include "resource_registry/texture.h" // SJH::Texture / SJH::TextureUPtr (CLASS_PTR)
#include <vmath.h>

namespace SJH::Sprite
{
	CLASS_PTR(UniformAtlas)
	/// @brief frameIdx  atlas UV rect (uMin, vMin, uSize, vSize) 0..1 정규화.
	/// @param frameIdx    0-based frame index (row-major: col = idx % cols, row = idx / cols)
	/// @param cols        그리드 column 수 (atlas 가로 = cols × tileSize)
	/// @param tileSize    정사각 tile 한 변 픽셀 수
	/// @param atlasWidth  atlas 전체 가로 픽셀 (= cols × tileSize)
	/// @param atlasHeight atlas 전체 세로 픽셀 (= rows × tileSize)
	/// @return vmath::vec4 UV rect. cols<=0 또는 atlasWidth/Height<=0 이면 zero rect.
	/// @note GL 호출 없음 — 순수 math. 단위 테스트가 GL fixture 없이 검증.
	vmath::vec4 ComputeUVRect(int frameIdx, int cols, int tileSize,
	                          int atlasWidth, int atlasHeight);

	/// @brief 등간격 N×M 정사각 그리드 atlas — sprite frame 시퀀스의 1차원 인덱스  2D UV rect 변환.
	/// @details
	///   - PNG 한 장에 동일 tile 크기 sprite N×M 행렬로 배치
	///   - frameIdx 가 row-major (col = idx % cols, row = idx / cols)
	///   - GL texture 는 @c SJH::Texture (RAII) 가 보유 — 본 클래스는 grid metadata + texture 위탁
	///   - stb_image 직접 호출 안 함 — @c SJH::Image::Load 위임 (spec §B.5)
	class UniformAtlas
	{
	  public:
		UniformAtlas() = default;
		~UniformAtlas() = default; // mTexture 가 자동 소멸 (SJH::Texture::~Texture 가 glDeleteTextures)

		UniformAtlas(const UniformAtlas &) = delete;
		UniformAtlas &operator=(const UniformAtlas &) = delete;
		UniformAtlas(UniformAtlas &&) = default;
		UniformAtlas &operator=(UniformAtlas &&) = default;

		// === Fluent Builder API — 단계 분리 ===
		//
		// 사용 패턴:
		//   atlas.LoadFromPNG("foo.png").SetGrid(4, 4);    // grid 직접 명시
		//   atlas.LoadFromPNG("foo.png").SetTileSize(128); // 또는 tilePx 명시 (cols/rows 자동)
		//   if (!atlas.IsValid()) { /* error */ }
		//
		// 책임 분할:
		//  - LoadFromPNG: PNG 디코드 + GL texture 업로드 + 픽셀아트 매개변수 + atlasW/H 추출
		//  - SetGrid/SetTileSize: grid metadata 만 (cols/rows/tileSize)

		/// @brief PNG 로드 + GL texture 업로드 + 픽셀아트 (NEAREST + CLAMP). grid 는 미설정.
		/// @return self (체이닝). 실패 시 spdlog::error + self 반환 (후속 IsValid()=false).
		UniformAtlas &LoadFromPNG(const char *path);

		/// @brief cols/rows 직접 명시 — tileSize 자동 도출 (atlasW/cols, square tile 가정).
		/// @details LoadFromPNG 호출 *후* 호출 가정. atlasW/H 가 cols/rows 로 나누어떨어져야 함.
		UniformAtlas &SetGrid(int cols, int rows);

		/// @brief tilePx 명시 — cols/rows 자동 도출 (atlasW/tilePx, atlasH/tilePx).
		/// @details LoadFromPNG 호출 *후* 호출 가정. atlasW/H 가 tilePx 로 나누어떨어져야 함.
		UniformAtlas &SetTileSize(int tilePx);

		/// @brief 모든 단계 (texture + grid) 성공 후 true.
		bool IsValid() const
		{
			return mTexture && mCols > 0 && mRows > 0 && mTileSize > 0;
		}

		/// @brief Texture 명시 해제. 소멸자가 자동 호출하지만 명시 해제 가능.
		void Release();

		/// @brief frameIdx  atlas UV rect 0..1 정규화. ComputeUVRect 위임.
		vmath::vec4 GetUVRect(int frameIdx) const;

		/// @brief 등록된 atlas 의 전체 frame 수 (cols × rows).
		int FrameCount() const
		{
			return mCols * mRows;
		}

		// === Accessors ===
		/// @brief GL 텍스처 핸들 — main.cpp 의 glBindTexture 등에 사용. 미로드 시 0.
		GLuint TextureId() const
		{
			return mTexture ? mTexture->GetTextureID() : 0;
		}
		int AtlasWidth() const
		{
			return mAtlasWidth;
		}
		int AtlasHeight() const
		{
			return mAtlasHeight;
		}
		int TileSize() const
		{
			return mTileSize;
		}
		int Cols() const
		{
			return mCols;
		}
		int Rows() const
		{
			return mRows;
		}
		/// @brief Texture* 직접 접근 — RenderSystem 등이 필요 시 사용.
		const SJH::Texture *GetTexture() const
		{
			return mTexture.get();
		}

	  private:
		SJH::TextureUPtr mTexture; // GL 텍스처 RAII 위탁
		int mAtlasWidth = 0;
		int mAtlasHeight = 0;
		int mTileSize = 64;
		int mCols = 0;
		int mRows = 0;
	};
} // namespace SJH::Sprite

#endif // __SJH_SPRITE_UNIFORM_ATLAS_H__
