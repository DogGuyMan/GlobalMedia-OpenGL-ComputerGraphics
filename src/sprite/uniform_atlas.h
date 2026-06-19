/**
 * @file uniform_atlas.h
 * @brief 등간격 NxM 그리드 sprite atlas - PNG 로드/UV 변환/GL 텍스처 RAII 를 Fluent Builder 로 제공.
 *
 * @details
 *  ### 책임
 *  - PNG 한 장에 동일 tile 크기로 배치된 sprite frame 시퀀스를 관리.
 *  - @c LoadFromPNG().SetGrid(cols, rows) Fluent Builder 패턴으로 atlas 를 구성.
 *  - 1차원 frameIdx -> 2D UV rect (0..1 정규화) 변환 (@ref ComputeUVRect 자유 함수 + @ref GetUVRect).
 *  - 픽셀아트 NEAREST + CLAMP_TO_EDGE 를 LoadFromPNG 내부에서 자동 적용.
 *
 *  ### 비-책임
 *  - [X] stb_image 직접 호출 - @c SJH::Image::Load 에 위임 (spec sec.B.5).
 *  - [X] glGenTextures 직접 호출 - @c SJH::Texture::CreateTexture 에 위임.
 *  - [X] atlas 수명 관리 - @c SJH::ResourceRegistry::CreateUniformAtlas 가 소유.
 *
 * @note @c ResourceRegistry::CreateUniformAtlas 가 Fluent Builder 2 단계(@c LoadFromPNG + @c SetGrid)를
 *       원자적으로 수행. 호출자는 Registry 경유 사용을 권장.
 */

#ifndef __SJH_SPRITE_UNIFORM_ATLAS_H__
#define __SJH_SPRITE_UNIFORM_ATLAS_H__

#include "GL/gl3w.h"
#include "common/common.h"
#include "texture/texture.h" // SJH::Texture / SJH::TextureUPtr (CLASS_PTR)
#include <vmath.h>

namespace SJH::Sprite
{
	CLASS_PTR(UniformAtlas)

	/**
	 * @brief frameIdx -> atlas UV rect (uMin, vMin, uSize, vSize) 0..1 정규화 - 정사각 tile 편의 오버로드.
	 * @param frameIdx    0-based frame index (row-major: col = idx % cols, row = idx / cols)
	 * @param cols        그리드 column 수
	 * @param tileSize    정사각 tile 한 변 픽셀 수
	 * @param atlasWidth  atlas 전체 가로 픽셀 (= cols x tileSize)
	 * @param atlasHeight atlas 전체 세로 픽셀 (= rows x tileSize)
	 * @return @c vmath::vec4(uMin, vMin, uSize, vSize). cols <= 0 또는 atlasWidth/Height <= 0 이면 zero rect.
	 * @note GL 호출 없음 - 순수 math. 단위 테스트가 GL fixture 없이 검증.
	 */
	vmath::vec4 ComputeUVRect(int frameIdx, int cols, int tileSize,
	                          int atlasWidth, int atlasHeight);

	/**
	 * @brief frameIdx -> atlas UV rect - 비정사각 tile (tileW != tileH) 지원 오버로드.
	 * @details 비트맵 폰트 (6x10 픽셀 등) 처럼 tile 가로/세로가 다른 경우에 사용.
	 *          정사각 tile 은 @c tileW=tileH 로 이 오버로드를 직접 호출하거나 정사각 오버로드 사용.
	 * @param frameIdx    0-based frame index (row-major)
	 * @param cols        그리드 column 수
	 * @param tileW       tile 가로 픽셀 (U 방향)
	 * @param tileH       tile 세로 픽셀 (V 방향)
	 * @param atlasWidth  atlas 전체 가로 픽셀
	 * @param atlasHeight atlas 전체 세로 픽셀
	 * @return @c vmath::vec4(uMin, vMin, uSize, vSize). 유효하지 않은 인자이면 zero rect.
	 */
	vmath::vec4 ComputeUVRect(int frameIdx, int cols, int tileW, int tileH,
	                          int atlasWidth, int atlasHeight);

	/**
	 * @brief 등간격 NxM 그리드 sprite atlas - Fluent Builder 로 PNG 로드 + grid 설정.
	 * @details
	 *  PNG 한 장에 동일 tile 크기 sprite 를 NxM 행렬로 배치한 atlas 를 관리한다.
	 *  frameIdx 는 row-major 1차원 인덱스 (col = idx % cols, row = idx / cols).
	 *
	 *  ### Fluent Builder 사용 패턴
	 *  @code
	 *  SJH::Sprite::UniformAtlas atlas;
	 *  atlas.LoadFromPNG("resources/sprites/player.png").SetGrid(4, 4);
	 *  // 또는 tile 크기로 grid 자동 도출:
	 *  atlas.LoadFromPNG("resources/sprites/player.png").SetTileSize(64);
	 *  if (!atlas.IsValid()) { // 에러 처리 }
	 *  @endcode
	 *
	 *  ### 책임 분할
	 *  - @c LoadFromPNG : PNG 디코드 + GL texture 업로드 + 픽셀아트 매개변수 + atlasW/H 추출.
	 *  - @c SetGrid / @c SetTileSize : grid metadata(cols/rows/tileSize) 만 설정.
	 *  - GL texture 는 @c SJH::Texture (RAII) 가 보유 - 본 클래스는 @c unique_ptr 로 소유.
	 *
	 *  ### 비-책임
	 *  - [X] stb_image 직접 호출 - @c SJH::Image::Load 위임.
	 *  - [X] glGenTextures 직접 호출 - @c SJH::Texture::CreateTexture 위임.
	 */
	class UniformAtlas
	{
	  public:
		/// @brief 기본 생성자 - texture/grid 미초기화. @c LoadFromPNG 호출 전까지 @c IsValid() = false.
		UniformAtlas() = default;

		/// @brief 소멸자 - @c mTexture(@c SJH::Texture::~Texture 가 @c glDeleteTextures) 자동 해제.
		~UniformAtlas() = default;

		UniformAtlas(const UniformAtlas &) = delete;
		UniformAtlas &operator=(const UniformAtlas &) = delete;
		UniformAtlas(UniformAtlas &&) = default;
		UniformAtlas &operator=(UniformAtlas &&) = default;

		// =====================================================================
		// Fluent Builder API
		// =====================================================================

		/// @brief PNG 로드 + GL texture 업로드 + 픽셀아트 필터(NEAREST + CLAMP_TO_EDGE) 적용. grid 는 미설정.
		/// @details
		///   내부에서 @c SJH::Image::Load -> @c SJH::Texture::CreateTexture 순으로 위임.
		///   성공 후 @c atlasWidth / @c atlasHeight 가 채워지고, @c SetGrid / @c SetTileSize 호출을 기다린다.
		/// @param path PNG 파일 경로 (실행 파일 기준 상대경로).
		/// @return @c *this (체이닝용). 실패 시 @c spdlog::error 출력 + @c *this 반환 (후속 @c IsValid() = false).
		UniformAtlas &LoadFromPNG(const char *path);

		/// @brief cols/rows 직접 명시 - tileSize 자동 도출 (atlasW / cols).
		/// @details
		///   @c LoadFromPNG 호출 *후* 호출 가정.
		///   atlasW/H 가 cols/rows 로 나누어떨어지지 않으면 @c spdlog::error + no-op.
		///   비정사각 tile(폰트 등)도 지원 - tileSize(U) = atlasW/cols, tileHeight(V) = atlasH/rows.
		/// @param cols 그리드 column 수 (양의 정수).
		/// @param rows 그리드 row 수 (양의 정수).
		/// @return @c *this (체이닝용).
		UniformAtlas &SetGrid(int cols, int rows);

		/// @brief tilePx 명시 - cols/rows 자동 도출 (atlasW / tilePx).
		/// @details
		///   @c LoadFromPNG 호출 *후* 호출 가정. 정사각 tile 경로.
		///   atlasW/H 가 @p tilePx 로 나누어떨어지지 않으면 @c spdlog::error + no-op.
		/// @param tilePx tile 한 변 픽셀 수 (정사각, 양의 정수).
		/// @return @c *this (체이닝용).
		UniformAtlas &SetTileSize(int tilePx);

		// =====================================================================
		// 조회 / 상태
		// =====================================================================

		/// @brief texture 업로드 + grid 설정이 모두 성공한 경우 @c true.
		bool IsValid() const
		{
			return mTexture && mCols > 0 && mRows > 0 && mTileSize > 0;
		}

		/// @brief GL texture 명시 해제 (@c glDeleteTextures). 소멸자가 자동 호출하지만 조기 해제 시 사용.
		void Release();

		/// @brief frameIdx -> atlas UV rect 0..1 정규화. @ref ComputeUVRect 에 위임.
		/// @param frameIdx 0-based frame index (row-major).
		/// @return @c vmath::vec4(uMin, vMin, uSize, vSize).
		vmath::vec4 GetUVRect(int frameIdx) const;

		/// @brief atlas 에 등록된 전체 frame 수 (cols x rows).
		int FrameCount() const
		{
			return mCols * mRows;
		}

		// =====================================================================
		// Accessors
		// =====================================================================

		/// @brief GL 텍스처 오브젝트 ID - @c glBindTexture 등에 사용. 미로드 시 0.
		GLuint TextureId() const
		{
			return mTexture ? mTexture->GetTextureID() : 0;
		}

		/// @brief atlas 전체 가로 픽셀 수. @c LoadFromPNG 이전에는 0.
		int AtlasWidth() const
		{
			return mAtlasWidth;
		}

		/// @brief atlas 전체 세로 픽셀 수. @c LoadFromPNG 이전에는 0.
		int AtlasHeight() const
		{
			return mAtlasHeight;
		}

		/// @brief tile 가로(U) 픽셀 수. @c SetGrid / @c SetTileSize 이전에는 기본값 64.
		int TileSize() const
		{
			return mTileSize;
		}

		/// @brief 그리드 column 수. @c SetGrid / @c SetTileSize 이전에는 0.
		int Cols() const
		{
			return mCols;
		}

		/// @brief 그리드 row 수. @c SetGrid / @c SetTileSize 이전에는 0.
		int Rows() const
		{
			return mRows;
		}

		/// @brief 내부 @c SJH::Texture 포인터 - @c SpriteRenderer / @c ResourceRegistry 등이 사용.
		/// @return 미로드 시 @c nullptr.
		const SJH::Texture *GetTexture() const
		{
			return mTexture.get();
		}

	  private:
		SJH::TextureUPtr mTexture;           ///< GL 텍스처 RAII 소유
		int mAtlasWidth  = 0;                ///< atlas 전체 가로 픽셀 (LoadFromPNG 에서 채워짐)
		int mAtlasHeight = 0;                ///< atlas 전체 세로 픽셀 (LoadFromPNG 에서 채워짐)
		int mTileSize    = 64;               ///< tile 가로(U) 픽셀 - SetGrid/SetTileSize 가 덮어씀
		int mTileHeight  = 0;                ///< tile 세로(V) 픽셀 - 비정사각 폰트(6x10) 지원; SetGrid 가 atlasH/rows 로 설정
		int mCols        = 0;                ///< 그리드 column 수
		int mRows        = 0;                ///< 그리드 row 수
	};
} // namespace SJH::Sprite

#endif // __SJH_SPRITE_UNIFORM_ATLAS_H__
