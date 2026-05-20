/**
 * @file test_gl_state_fields.cpp
 * @brief SymbolicName 사전 적중/미적중/GL_ZERO 정책 검증.
 * @details GL context 불필요 — 순수 함수 테스트.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "diagnostics/gl_state_fields.h"
#include "GL/gl3w.h"

using SJH::Diagnostics::SymbolicName;
using Catch::Matchers::Equals;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("SymbolicName 사전 적중", "[diagnostics][state_fields]")
{
    REQUIRE_THAT(SymbolicName(GL_LESS),    Equals("GL_LESS"));
    REQUIRE_THAT(SymbolicName(GL_LEQUAL),  Equals("GL_LEQUAL"));
    REQUIRE_THAT(SymbolicName(GL_BACK),    Equals("GL_BACK"));
    REQUIRE_THAT(SymbolicName(GL_FRONT),   Equals("GL_FRONT"));
    REQUIRE_THAT(SymbolicName(GL_CCW),     Equals("GL_CCW"));
    REQUIRE_THAT(SymbolicName(GL_CW),      Equals("GL_CW"));
    REQUIRE_THAT(SymbolicName(GL_ONE),     Equals("GL_ONE"));
    REQUIRE_THAT(SymbolicName(GL_SRC_ALPHA), Equals("GL_SRC_ALPHA"));
}

TEST_CASE("SymbolicName(0) -> GL_ZERO 정책 (blend factor 컨텍스트)", "[diagnostics][state_fields]")
{
    // 본 프로젝트 17개 캡처 필드 한정 시 0이 enum 값으로 합법 발생하는 곳은
    // blend_src_rgb / blend_dst_rgb 뿐 — GL_ZERO 가 정확.
    // 미래 GL_TEXTURE_COMPARE_MODE 등 GL_NONE-context 추가 시 필드별 분기 (현재 YAGNI).
    REQUIRE_THAT(SymbolicName(0), Equals("GL_ZERO"));
}

TEST_CASE("SymbolicName 미적중 -> hex fallback", "[diagnostics][state_fields]")
{
    REQUIRE_THAT(SymbolicName(0xDEAD),  Equals("0xDEAD"));
    REQUIRE_THAT(SymbolicName(0xBEEF),  Equals("0xBEEF"));
    // 4자리 hex (대문자) — 사람이 매뉴얼 검색 가능한 형식
    REQUIRE_THAT(SymbolicName(0xABCD),  Equals("0xABCD"));
}

TEST_CASE("SymbolicName GL_TEXTUREn 동적 표현", "[diagnostics][state_fields]")
{
    // active_texture 필드는 GL_TEXTURE0..GL_TEXTURE15 범위 — 사전 16개 등록 비효율,
    // 동적으로 "GL_TEXTURE{n}" 포맷.
    REQUIRE_THAT(SymbolicName(GL_TEXTURE0),  Equals("GL_TEXTURE0"));
    REQUIRE_THAT(SymbolicName(GL_TEXTURE1),  Equals("GL_TEXTURE1"));
    REQUIRE_THAT(SymbolicName(GL_TEXTURE15), Equals("GL_TEXTURE15"));
}

TEST_CASE("SymbolicName 사전 boundary — depth_func 모든 8개", "[diagnostics][state_fields]")
{
    REQUIRE_THAT(SymbolicName(GL_NEVER),    Equals("GL_NEVER"));
    REQUIRE_THAT(SymbolicName(GL_LESS),     Equals("GL_LESS"));
    REQUIRE_THAT(SymbolicName(GL_EQUAL),    Equals("GL_EQUAL"));
    REQUIRE_THAT(SymbolicName(GL_LEQUAL),   Equals("GL_LEQUAL"));
    REQUIRE_THAT(SymbolicName(GL_GREATER),  Equals("GL_GREATER"));
    REQUIRE_THAT(SymbolicName(GL_NOTEQUAL), Equals("GL_NOTEQUAL"));
    REQUIRE_THAT(SymbolicName(GL_GEQUAL),   Equals("GL_GEQUAL"));
    REQUIRE_THAT(SymbolicName(GL_ALWAYS),   Equals("GL_ALWAYS"));
}

// ──────────────────────────────────────────────────────────────────────────
// audit 트랙 A — VertexAttribInfo (bug-coverage-audit.md 카테고리 C 대응)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("VertexAttribInfo default 생성자 값", "[diagnostics][attribute_info]")
{
    // GL spec default state: attribute disabled, size=4, type=GL_FLOAT, normalized=false,
    // stride=0, buffer_binding=0. 본 default가 spec과 일치해야 fresh fixture에서 캡처 결과가
    // 정확.
    SJH::Diagnostics::VertexAttribInfo info{};

    REQUIRE_FALSE(info.enabled);
    REQUIRE(info.size == 4);
    REQUIRE(info.type == GL_FLOAT);
    REQUIRE_FALSE(info.normalized);
    REQUIRE(info.stride == 0);
    REQUIRE(info.buffer_binding == 0u);
}

TEST_CASE("VertexAttribInfo operator== identity", "[diagnostics][attribute_info]")
{
    SJH::Diagnostics::VertexAttribInfo a{};
    SJH::Diagnostics::VertexAttribInfo b{};
    REQUIRE(a == b);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("VertexAttribInfo operator== — 단일 필드 변화 감지 (사보타지 방지)",
          "[diagnostics][attribute_info]")
{
    using SJH::Diagnostics::VertexAttribInfo;

    SECTION("enabled 변화")
    {
        VertexAttribInfo a{}; VertexAttribInfo b{}; b.enabled = true;
        REQUIRE(a != b);
    }
    SECTION("size 변화 (vec3 -> vec2 사보타지 — 카테고리 C2)")
    {
        VertexAttribInfo a{}; a.size = 3;
        VertexAttribInfo b{}; b.size = 2;
        REQUIRE(a != b);
    }
    SECTION("type 변화 (GL_FLOAT -> GL_INT)")
    {
        VertexAttribInfo a{}; a.type = GL_FLOAT;
        VertexAttribInfo b{}; b.type = GL_INT;
        REQUIRE(a != b);
    }
    SECTION("stride 변화 (offset 누적 계산 사보타지 — 카테고리 C1)")
    {
        VertexAttribInfo a{}; a.stride = 24;
        VertexAttribInfo b{}; b.stride = 32;
        REQUIRE(a != b);
    }
    SECTION("buffer_binding 변화 (잘못된 VBO에서 읽음 — 카테고리 C3)")
    {
        VertexAttribInfo a{}; a.buffer_binding = 1;
        VertexAttribInfo b{}; b.buffer_binding = 2;
        REQUIRE(a != b);
    }
}

TEST_CASE("GLStateFields default — attribute_layouts 16개 모두 disabled",
          "[diagnostics][state_fields][attribute_layouts]")
{
    // 사보타지: capture loop의 i<16을 i<1로 줄이면 [1..15]가 *capture 안 된 default*가 되는데,
    // default 자체는 disabled라서 우연 통과 가능. 따라서 capture 검증은 별도로 (GL ctx 필요)
    // — 여기서는 *struct default*만 박는다.
    SJH::Diagnostics::GLStateFields f{};
    REQUIRE(f.attribute_layouts.size() == 16);

    for (size_t i = 0; i < f.attribute_layouts.size(); ++i)
    {
        const auto& a = f.attribute_layouts[i];
        INFO("attribute slot " << i);
        REQUIRE_FALSE(a.enabled);
        REQUIRE(a.size == 4);
        REQUIRE(a.type == GL_FLOAT);
    }
}
