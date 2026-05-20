/**
 * @file test_gl_state_snapshot.cpp
 * @brief GLStateSnapshot::ToString + Diff 회귀.
 *        대부분 GL context 불필요 (struct 직접 구성으로 path 강제).
 *
 * @details
 *  Plan Task 6의 8 케이스 + audit 트랙 A의 attribute_layouts Diff 케이스 3개.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "support/gl_state_snapshot.h"

using SJH::test::GLStateSnapshot;
using SJH::test::Diff;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::Equals;

// ──────────────────────────────────────────────────────────────────────────
// Plan Task 6 — 8 케이스
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("Diff — 동일 snapshot은 '(no GL state change)' 단일 줄", "[snapshot][diff]")
{
    GLStateSnapshot a{};
    GLStateSnapshot b{};
    REQUIRE_THAT(Diff(a, b), Equals("(no GL state change)\n"));
}

TEST_CASE("Diff — handle 변화는 raw 정수 (비대칭 정책)", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.vao = 3;
    b.fields.vao = 5;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("vao:"));
    REQUIRE_THAT(d, ContainsSubstring("3"));
    REQUIRE_THAT(d, ContainsSubstring("5"));
    REQUIRE_THAT(d, ContainsSubstring("->"));
}

TEST_CASE("Diff — enum 변화는 SymbolicName (비대칭 정책)", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.depth_func = GL_LESS;
    b.fields.depth_func = GL_LEQUAL;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("depth_func"));
    REQUIRE_THAT(d, ContainsSubstring("GL_LESS"));
    REQUIRE_THAT(d, ContainsSubstring("GL_LEQUAL"));
}

TEST_CASE("Diff — element_buffer 변화 + VAO=0 시 EBO 주석", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.vao = 0;
    a.fields.element_buffer = 0;
    b.fields.vao = 0;
    b.fields.element_buffer = 7;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("element_buffer"));
    REQUIRE_THAT(d, ContainsSubstring("EBO state is per-VAO"));
}

TEST_CASE("Diff — element_buffer 변화 + VAO≠0 이면 주석 미포함", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.vao = 3;
    a.fields.element_buffer = 0;
    b.fields.vao = 3;
    b.fields.element_buffer = 7;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("element_buffer"));
    REQUIRE_FALSE(d.find("EBO state is per-VAO") != std::string::npos);
}

TEST_CASE("Diff — texture unit 변화는 unit 번호 표시", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.texture_2d_per_unit[3] = 0;
    b.fields.texture_2d_per_unit[3] = 42;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("tex_2d[unit 3]"));
    REQUIRE_THAT(d, ContainsSubstring("42"));
}

TEST_CASE("ToString VAO=0 — 'EBO state is per-VAO' 주석 포함", "[snapshot][tostring]")
{
    GLStateSnapshot s{};
    s.fields.vao = 0;
    auto str = s.ToString();
    REQUIRE_THAT(str, ContainsSubstring("EBO state is per-VAO"));
}

TEST_CASE("ToString VAO≠0 — 주석 미포함", "[snapshot][tostring]")
{
    GLStateSnapshot s{};
    s.fields.vao = 3;
    auto str = s.ToString();
    REQUIRE_FALSE(str.find("EBO state is per-VAO") != std::string::npos);
}

// ──────────────────────────────────────────────────────────────────────────
// audit 트랙 A — attribute_layouts Diff (카테고리 C 사보타지 가시화)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("Diff — attribute slot 변화 시 slot 번호 + before/after 출력",
          "[snapshot][diff][attribute_layouts]")
{
    GLStateSnapshot a{}, b{};
    // before: disabled, after: enabled with vec3 GL_FLOAT
    b.fields.attribute_layouts[0].enabled = true;
    b.fields.attribute_layouts[0].size = 3;
    b.fields.attribute_layouts[0].type = GL_FLOAT;
    b.fields.attribute_layouts[0].stride = 24;
    b.fields.attribute_layouts[0].buffer_binding = 5;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("attrib[0]"));
    REQUIRE_THAT(d, ContainsSubstring("disabled"));    // before
    REQUIRE_THAT(d, ContainsSubstring("vec3"));        // after
    REQUIRE_THAT(d, ContainsSubstring("GL_FLOAT"));
    REQUIRE_THAT(d, ContainsSubstring("stride=24"));
    REQUIRE_THAT(d, ContainsSubstring("vbo=5"));
}

TEST_CASE("Diff — attribute size 변화 (vec3 -> vec2 사보타지 가시화)",
          "[snapshot][diff][attribute_layouts]")
{
    // STUDY_NOTE Ex6 R-3 회귀 가시화 — UV가 size=4로 잘못 읽혀지는 사건
    GLStateSnapshot a{}, b{};
    a.fields.attribute_layouts[1].enabled = true;
    a.fields.attribute_layouts[1].size = 2;  // vec2 — 정상 UV
    a.fields.attribute_layouts[1].type = GL_FLOAT;

    b.fields.attribute_layouts[1].enabled = true;
    b.fields.attribute_layouts[1].size = 4;  // 사보타지 — vec4로 잘못 읽음
    b.fields.attribute_layouts[1].type = GL_FLOAT;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("attrib[1]"));
    REQUIRE_THAT(d, ContainsSubstring("vec2"));   // before (정상)
    REQUIRE_THAT(d, ContainsSubstring("vec4"));   // after (사보타지)
}

TEST_CASE("Diff — disabled인 다른 slot은 출력 안 됨", "[snapshot][diff][attribute_layouts]")
{
    // 변화가 없는 slot은 노이즈로 출력 안 됨 — 여러 slot 중 변화한 것만 표시.
    GLStateSnapshot a{}, b{};
    a.fields.attribute_layouts[0].enabled = true;
    a.fields.attribute_layouts[0].size = 3;
    b.fields.attribute_layouts[0].enabled = true;
    b.fields.attribute_layouts[0].size = 3;
    // slot 0은 동일, slot 1만 변화
    b.fields.attribute_layouts[1].enabled = true;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("attrib[1]"));
    // slot 0은 동일하므로 출력에 없어야 함
    REQUIRE_FALSE(d.find("attrib[0]") != std::string::npos);
}

TEST_CASE("Diff — attribute 모든 필드 동일하면 변화 출력 안 됨",
          "[snapshot][diff][attribute_layouts]")
{
    // operator== 가 모든 필드를 비교 -> 동일 시 침묵 (sanity)
    GLStateSnapshot a{}, b{};
    a.fields.attribute_layouts[0].enabled = true;
    a.fields.attribute_layouts[0].size = 3;
    a.fields.attribute_layouts[0].type = GL_FLOAT;
    a.fields.attribute_layouts[0].stride = 24;
    a.fields.attribute_layouts[0].buffer_binding = 1;

    b.fields.attribute_layouts[0] = a.fields.attribute_layouts[0]; // 복사

    auto d = Diff(a, b);
    REQUIRE_THAT(d, Equals("(no GL state change)\n"));
}

// ──────────────────────────────────────────────────────────────────────────
// Capture — GL ctx 필요한 행동 단언 (sanity check)
// ──────────────────────────────────────────────────────────────────────────

#include "support/gl_test_fixture.h"
#include "GL/gl3w.h"

TEST_CASE("Capture — production CaptureGLState에 위임", "[snapshot][capture]")
{
    SJH::test::GLContextFixture ctx;

    // 두 번 캡처 -> 동일 결과 (production CaptureGLState의 결정성 의존)
    auto a = GLStateSnapshot::Capture();
    auto b = GLStateSnapshot::Capture();

    REQUIRE_THAT(Diff(a, b), Equals("(no GL state change)\n"));
}
