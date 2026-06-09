/**
 * @file bitmap_font.h
 * @brief BMFont(AngelCode) 포맷 비트맵 폰트 - codepoint-to-Glyph 매핑 + 공유 @c UniformAtlas.
 *
 * @details
 *  ### 책임
 *  - PNG + XML(BMFont 포맷) 로드 후 codepoint -> (frameIndex, xadvance) 룩업 테이블 구축.
 *  - 내부 @c UniformAtlas 는 @c ResourceRegistry 에 위탁(비소유) - 사이클 회피(spec sec.2.2).
 *  - 균일 그리드 atlas 전용 서브셋 파서 (frostyfreeze / minogram 계열).
 *  - V-flip 보정: @c SJH::Image::Load 가 PNG 를 상하 반전해 GL 업로드하므로
 *    frameRow = (rows-1) - pngRow 로 행 인덱스를 교정.
 *
 *  ### 비-책임
 *  - [X] 범용 BMFont XML 파서 - kerning pair / 비균일 셀 / 다중 texture page 미지원.
 *  - [X] 글리프 quad 발행 / 렌더링 - @c TextRenderer 책임.
 *  - [X] @c UniformAtlas 소유 - @c ResourceRegistry 가 owner (dangling 차단).
 *
 * @note 2026-06-02 spec sec.2: @c SJH::Text World Text 모듈 일원.
 */
#ifndef __SJH_TEXT_BITMAP_FONT_H__
#define __SJH_TEXT_BITMAP_FONT_H__

#include <cstdint>
#include <string>
#include <unordered_map>

namespace SJH { class ResourceRegistry; }
namespace SJH::Sprite { class UniformAtlas; }

namespace SJH::Text
{
    /**
     * @brief 한 글리프의 atlas 프레임 인덱스와 수평 전진 폭(픽셀).
     * @details
     *  @c BitmapFont::mGlyphs 테이블의 값 타입.
     *  @c frameIndex 는 @c UniformAtlas 의 grid 인덱스 (0-based row-major),
     *  @c xadvance 는 BMFont XML @c xadvance 속성 - 다음 글자까지의 수평 이동 거리(px).
     */
    struct Glyph
    {
        int frameIndex = -1; ///< @brief atlas 그리드 프레임 인덱스. -1 = 글리프 없음(skip).
        int xadvance   = 0;  ///< @brief 수평 전진 폭 (px). 다음 글자 펜 위치 산출에 사용.
    };

    /**
     * @brief BMFont(AngelCode) PNG+XML 비트맵 폰트 -
     *        codepoint -> (frameIndex, xadvance) 룩업 + 공유 @c UniformAtlas.
     *
     * @details
     *  ### 생성
     *  정적 팩터리 @c LoadFromBMFont 로만 유효 인스턴스 생성 - 기본 생성자는 빈 폰트.
     *  @c UniformAtlas 는 @c ResourceRegistry 에 @p key 로 캐시 (Find 우선, 없으면 Create).
     *
     *  ### 글리프 조회 우선순위
     *  1. 해당 codepoint 직접 조회.
     *  2. @c '?' (대체 문자) fallback.
     *  3. @c ' ' (공백) fallback.
     *  4. -1 / mCellW 최후 fallback (skip 또는 기본 폭).
     *
     *  ### 제약
     *  균일 그리드 BMFont 서브셋 전용 - kerning pair / 비균일 셀 / 다중 texture page 미지원.
     *
     * @note spec sec.2.2 - 내부 @c UniformAtlas 는 비소유(non-owning) raw pointer.
     *       @c ResourceRegistry 가 실제 owner - BitmapFont 소멸 시 atlas 해제 안 됨.
     */
    class BitmapFont
    {
      public:
        /// @brief 기본 생성자 - 빈(invalid) 폰트. @c IsValid() == false.
        BitmapFont() = default;

        /**
         * @brief PNG + XML(BMFont 포맷) 로드 후 @c BitmapFont 인스턴스 반환.
         * @details
         *  1. @p xmlPath 를 바이너리 모드로 열어 라인 단위 파싱.
         *  2. @c <common> 태그에서 scaleW / scaleH / lineHeight 추출 -> cols/rows 산출.
         *  3. @c <char> 태그 전체 순회 -> codepoint-Glyph 맵 구축 (V-flip 행 보정 포함).
         *  4. @c reg 에서 @p key 로 @c UniformAtlas Find 우선, 없으면 Create.
         *  5. 어느 단계든 실패 시 빈 폰트(@c IsValid()==false) 반환 + @c spdlog::warn.
         *
         * @param reg     @c ResourceRegistry 참조 - @c UniformAtlas 캐시 소유자.
         * @param key     @c ResourceRegistry 캐시 키 (atlas + font 공유 키).
         * @param pngPath 폰트 텍스처 PNG 파일 경로.
         * @param xmlPath BMFont XML 파일 경로 (AngelCode @c .fnt / @c .xml 포맷).
         * @return 유효 또는 빈 @c BitmapFont 값 객체.
         */
        static BitmapFont LoadFromBMFont(SJH::ResourceRegistry& reg, const std::string& key,
                                         const std::string& pngPath, const std::string& xmlPath);

        /**
         * @brief codepoint 에 대응하는 @c Glyph 포인터 반환. 없으면 @c nullptr.
         * @param codepoint UTF-32 코드포인트 (ASCII 범위는 char 캐스트).
         * @return 캐시 내 @c Glyph 포인터(비소유) 또는 @c nullptr.
         */
        const Glyph* Find(std::uint32_t codepoint) const;

        /**
         * @brief codepoint 의 atlas 프레임 인덱스 반환. 없으면 @c '?' -> @c ' ' 순 fallback, 모두 없으면 -1(skip).
         * @param codepoint UTF-32 코드포인트.
         * @return atlas grid 인덱스 (0-based) 또는 -1.
         */
        int FrameOf(std::uint32_t codepoint) const;

        /**
         * @brief codepoint 의 수평 전진 폭(px) 반환. 없으면 @c '?' -> @c ' ' 순 fallback, 모두 없으면 @c mCellW.
         * @param codepoint UTF-32 코드포인트.
         * @return xadvance (px).
         */
        int AdvanceOf(std::uint32_t codepoint) const;

        /// @brief 공유 @c UniformAtlas 포인터 반환 (비소유 - @c ResourceRegistry 가 owner).
        SJH::Sprite::UniformAtlas* GetAtlas() const { return mAtlas; }

        /// @brief BMFont XML @c lineHeight 속성 (px). 줄 간격 계산 기준.
        int LineHeight() const { return mLineHeight; }

        /// @brief atlas 그리드 셀 폭 (px). 첫 번째 @c <char> 의 @c width 값.
        int CellW() const { return mCellW; }

        /// @brief atlas 그리드 셀 높이 (px). 첫 번째 @c <char> 의 @c height 값.
        int CellH() const { return mCellH; }

        /// @brief atlas 가 연결되고 글리프 맵이 비어 있지 않으면 @c true.
        bool IsValid() const { return mAtlas != nullptr && !mGlyphs.empty(); }

      private:
        SJH::Sprite::UniformAtlas* mAtlas = nullptr;         ///< @brief 비소유 atlas 포인터 (registry 보유).
        std::unordered_map<std::uint32_t, Glyph> mGlyphs;   ///< @brief codepoint -> Glyph 룩업 테이블.
        int mLineHeight = 0; ///< @brief 줄 간격 (px).
        int mCellW      = 0; ///< @brief 그리드 셀 폭 (px).
        int mCellH      = 0; ///< @brief 그리드 셀 높이 (px).
    };

} // namespace SJH::Text

#endif // __SJH_TEXT_BITMAP_FONT_H__
