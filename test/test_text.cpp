// SJH::Text::BitmapFont characterization (CPU-가능부 한정).
//
// ★ GL-결합 판정: 글리프 맵(mGlyphs)을 채우는 *유일* 공개 경로는 LoadFromBMFont 인데,
//   bitmap_font.cpp:108 에서 reg.CreateUniformAtlas(...) = GL 텍스처 생성을 강제한다.
//   GL 컨텍스트 없이 mGlyphs 를 채울 공개 경로가 없으므로 (생성자=빈 폰트뿐),
//   "채워진 폰트의 직접 조회" 와 "XML 파싱" 은 Phase C(GPU) 로 이연한다.
//   본 파일은 GL 미접촉 부분 = 기본 생성자 빈 폰트의 fallback / 접근자 계약만 잠근다.
//   (헤더는 ResourceRegistry / UniformAtlas 를 전방선언만 해 이 TU 는 GL 헤더 미포함.)
#include <catch2/catch_test_macros.hpp>

#include "text/bitmap_font.h"

using SJH::Text::BitmapFont;
using SJH::Text::Glyph;

// === 기본 생성자 = 빈(invalid) 폰트 ======================================
TEST_CASE("BitmapFont: 기본 생성은 invalid (atlas 없음 + 글리프 없음)", "[text]")
{
    BitmapFont font;
    REQUIRE_FALSE(font.IsValid()); // mAtlas==nullptr && mGlyphs.empty()
    REQUIRE(font.GetAtlas() == nullptr);
    REQUIRE(font.LineHeight() == 0);
    REQUIRE(font.CellW() == 0);
    REQUIRE(font.CellH() == 0);
}

// === 빈 폰트의 조회 - Find 는 항상 nullptr ===============================
TEST_CASE("BitmapFont: 빈 폰트의 Find 는 어떤 codepoint 든 nullptr", "[text]")
{
    BitmapFont font;
    REQUIRE(font.Find('A') == nullptr);
    REQUIRE(font.Find('?') == nullptr); // 대체 문자도 없음
    REQUIRE(font.Find(' ') == nullptr);
    REQUIRE(font.Find(0x0041u) == nullptr);
}

// === 빈 폰트의 fallback 최후값 ===========================================
// FrameOf: 직접->'?'->' ' 모두 miss -> -1 (skip).
TEST_CASE("BitmapFont: 빈 폰트의 FrameOf 는 -1 (모든 fallback miss)", "[text]")
{
    BitmapFont font;
    REQUIRE(font.FrameOf('A') == -1);
    REQUIRE(font.FrameOf('?') == -1);
    REQUIRE(font.FrameOf(' ') == -1);
}

// AdvanceOf: 직접->'?'->' ' 모두 miss -> mCellW (빈 폰트는 0).
TEST_CASE("BitmapFont: 빈 폰트의 AdvanceOf 는 mCellW(=0) 최후 fallback", "[text]")
{
    BitmapFont font;
    REQUIRE(font.AdvanceOf('A') == 0); // CellW()==0 과 동일
    REQUIRE(font.AdvanceOf('?') == 0);
    REQUIRE(font.AdvanceOf(font.CellW()) == 0);
}

// === Glyph POD 기본값 계약 ===============================================
TEST_CASE("Glyph: 기본값은 frameIndex=-1(없음) + xadvance=0", "[text]")
{
    Glyph g;
    REQUIRE(g.frameIndex == -1);
    REQUIRE(g.xadvance == 0);
}
