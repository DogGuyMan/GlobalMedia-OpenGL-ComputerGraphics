/**
 * @file test_bitmap_font.cpp
 * @brief C-5 text BitmapFont GPU - 실제 BMFont 에셋(minogram)을 GL 위에서 로드해
 *        채워진 폰트의 조회 / fallback hit / XML 파싱 / V-flip 보정을 잠근다 (Phase A 이연분).
 *
 * @details
 *  ### 왜 GPU 인가
 *  @c LoadFromBMFont 가 @c CreateUniformAtlas (GL Texture) 를 강제하므로 (bitmap_font.cpp:108)
 *  채워진 폰트의 조회/fallback/파싱은 GL 컨텍스트가 필요하다 -> Phase C.
 *
 *  ### 에셋
 *  apps/_MyApp_/resources/font/minogram_6x10.{png,xml} 재사용.
 *  CMake 가 @c SJH_GPU_FONT_DIR 절대경로를 주입 (CWD 무관).
 *  minogram 메타: cellW=6, cellH=10, lineHeight=12, scaleW=78(cols=13), scaleH=70(rows=7).
 *  'A'(65) @ x=0,y=0 -> frameRow=(7-1)-0=6, frame=6*13+0=78 (V-flip 보정 후).
 *  '?'(63) / ' '(32) 모두 존재 -> fallback hit 경로 검증 가능.
 *
 *  ### 자원 보유
 *  UniformAtlas owner 는 @c ResourceRegistry (Meyer 싱글톤). 테스트는 고유 key 로 등록해
 *  케이스 간 캐시 충돌(이미 존재 -> Create 실패)을 회피한다.
 */

#include "gl_context_fixture.h"

#include "text/bitmap_font.h"
#include "resource_registry/resource_registry.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{
    /// @brief minogram 폰트를 고유 key 로 로드 - 호출마다 다른 key 로 캐시 충돌 회피.
    SJH::Text::BitmapFont LoadMinogram(const std::string &uniqueKey)
    {
        const std::string dir = SJH_GPU_FONT_DIR; // CMake 주입 절대경로 (끝에 '/').
        const std::string png = dir + "minogram_6x10.png";
        const std::string xml = dir + "minogram_6x10.xml";
        return SJH::Text::BitmapFont::LoadFromBMFont(
            SJH::ResourceRegistry::Get(), uniqueKey, png, xml);
    }
}

TEST_CASE("C-5 빈 폰트는 invalid - 모든 조회가 fallback (CPU 동작 재확인)", "[gpu][text]")
{
    using namespace SJH::Text;
    BitmapFont empty;
    CHECK_FALSE(empty.IsValid());
    CHECK(empty.Find('A') == nullptr);
    CHECK(empty.FrameOf('A') == -1);
    CHECK(empty.AdvanceOf('A') == 0); // mCellW = 0 (빈 폰트)
}

TEST_CASE("C-5 minogram 로드 - IsValid + XML 파싱 메타값", "[gpu][text]")
{
    using namespace SJH::Text;
    REQUIRE(SJH::Test::Gpu::GLContextFixture::IsValid());

    BitmapFont font = LoadMinogram("c5_minogram_meta");
    REQUIRE(font.IsValid());

    // <common lineHeight="12" .../> + 첫 <char width=6 height=10>.
    CHECK(font.LineHeight() == 12);
    CHECK(font.CellW() == 6);
    CHECK(font.CellH() == 10);
    CHECK(font.GetAtlas() != nullptr);
}

TEST_CASE("C-5 minogram codepoint 적중 + V-flip 행 보정", "[gpu][text]")
{
    using namespace SJH::Text;
    REQUIRE(SJH::Test::Gpu::GLContextFixture::IsValid());

    BitmapFont font = LoadMinogram("c5_minogram_lookup");
    REQUIRE(font.IsValid());

    // 'A'(65): x=0,y=0,xadvance=6. cols=13,rows=7 -> V-flip frameRow=6 -> frame=78.
    const Glyph *a = font.Find('A');
    REQUIRE(a != nullptr);
    CHECK(a->xadvance == 6);
    CHECK(a->frameIndex == 78); // V-flip 보정 잠금 (행 뒤집기)
    CHECK(font.FrameOf('A') == 78);
    CHECK(font.AdvanceOf('A') == 6);
}

TEST_CASE("C-5 fallback hit - 미존재 codepoint 는 '?' 로 폴백", "[gpu][text]")
{
    using namespace SJH::Text;
    REQUIRE(SJH::Test::Gpu::GLContextFixture::IsValid());

    BitmapFont font = LoadMinogram("c5_minogram_fallback");
    REQUIRE(font.IsValid());

    // minogram 에 없는 codepoint (한글 'A' 대신 비-ASCII). '?'(63) 가 존재하므로 그 값으로 폴백.
    const Glyph *q = font.Find('?');
    REQUIRE(q != nullptr); // '?' 자체는 존재해야 fallback hit 의미 있음.

    const std::uint32_t missing = 0xAC00; // '가' - minogram 에 없음.
    CHECK(font.Find(missing) == nullptr); // 직접 조회는 miss.
    // FrameOf/AdvanceOf 는 '?' fallback hit -> '?' 의 값과 동일.
    CHECK(font.FrameOf(missing) == q->frameIndex);
    CHECK(font.AdvanceOf(missing) == q->xadvance);
}
