/**
 * @file test_gl_state_capture.cpp
 * @brief CaptureGLState 회귀 — 결정성, 부수효과 0, GL_NO_ERROR, fresh default, bind 반영.
 */

#include <catch2/catch_test_macros.hpp>

#include "diagnostics/gl_state_fields.h"
#include "support/gl_test_fixture.h"
#include "GL/gl3w.h"

using SJH::Diagnostics::CaptureGLState;

namespace
{
    void DrainGLErrors()
    {
        while (glGetError() != GL_NO_ERROR)
        {
        }
    }
}

TEST_CASE("CaptureGLState 결정성 — 두 번 연속 호출 byte-equal", "[diagnostics][capture]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    auto a = CaptureGLState();
    auto b = CaptureGLState();

    // struct 비교 — std::array는 == 지원
    REQUIRE(a.vao == b.vao);
    REQUIRE(a.program == b.program);
    REQUIRE(a.active_texture == b.active_texture);
    REQUIRE(a.texture_2d_per_unit == b.texture_2d_per_unit);
    REQUIRE(a.viewport == b.viewport);
    REQUIRE(a.depth_test_enabled == b.depth_test_enabled);
    REQUIRE(a.blend_enabled == b.blend_enabled);
    REQUIRE(a.clear_color == b.clear_color);
}

TEST_CASE("CaptureGLState 부수효과 0 — active_texture 변하지 않음", "[diagnostics][capture]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    glActiveTexture(GL_TEXTURE5); // 의도적으로 unit 5로 변경
    GLint before = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &before);

    CaptureGLState(); // 내부에서 16 unit 순회 후 복원해야 함

    GLint after = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &after);
    REQUIRE(before == after);
    REQUIRE(after == GL_TEXTURE5);
}

TEST_CASE("CaptureGLState 후 GL_NO_ERROR", "[diagnostics][capture]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    auto fields = CaptureGLState();

    GLenum err = glGetError();
    REQUIRE(err == GL_NO_ERROR);
}

TEST_CASE("fresh fixture default — VAO=0, program=0, viewport는 actual GL 상태와 일치",
          "[diagnostics][capture]")
{
    // viewport: macOS Retina에서는 logical 256 -> physical 512 (HiDPI 2x). plan 원안의
    // (256,256) 직접 비교는 Retina에서 깨짐. *capture가 actual GL 상태를 그대로 반영*하는지를
    // 검증하는 게 더 본질적 (사보타지 잡는 데 충분).
    SJH::test::GLContextFixture ctx(256, 256);
    DrainGLErrors();

    GLint actual_vp[4] = {};
    glGetIntegerv(GL_VIEWPORT, actual_vp);

    auto f = CaptureGLState();
    REQUIRE(f.vao == 0u);
    REQUIRE(f.program == 0u);
    REQUIRE(f.viewport[0] == actual_vp[0]);
    REQUIRE(f.viewport[1] == actual_vp[1]);
    REQUIRE(f.viewport[2] == actual_vp[2]);
    REQUIRE(f.viewport[3] == actual_vp[3]);
    REQUIRE(f.viewport[2] > 0);  // sanity — 0 viewport는 비정상
    REQUIRE(f.viewport[3] > 0);
}

TEST_CASE("CaptureGLState — VAO 바인딩 후 fields.vao 반영", "[diagnostics][capture]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    auto f = CaptureGLState();
    REQUIRE(f.vao == vao);

    glDeleteVertexArrays(1, &vao);
}

// ──────────────────────────────────────────────────────────────────────────
// audit 트랙 A — Vertex Attribute Layouts (bug-coverage-audit.md 카테고리 C)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("attribute_layouts — fresh fixture (VAO=0)에서 16개 모두 disabled",
          "[diagnostics][capture][attribute_layouts]")
{
    // macOS GL 3.3 core profile: VAO=0 상태에서는 default VAO가 valid 하지 않음 ->
    // glGetVertexAttribiv 반환값이 driver-dependent (Apple은 size=0 등 반환).
    // 따라서 VAO=0 상태에서 검증 가능한 *유일한 invariant*는 enabled=false 뿐.
    // 다른 필드의 spec default는 별도 테스트 (VAO 바인딩 후)에서 검증.
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    auto f = CaptureGLState();
    REQUIRE(f.attribute_layouts.size() == 16);

    for (size_t i = 0; i < f.attribute_layouts.size(); ++i)
    {
        const auto& a = f.attribute_layouts[i];
        INFO("attribute slot " << i << " (VAO=0, macOS core profile)");
        REQUIRE_FALSE(a.enabled);
    }
}

TEST_CASE("attribute_layouts — VAO 바인딩 직후 spec default (size=4, type=GL_FLOAT)",
          "[diagnostics][capture][attribute_layouts]")
{
    // VAO 바인딩 후 (attribute 미설정) -> GL spec 명시 default가 적용된다:
    //   enabled=false, size=4, type=GL_FLOAT, normalized=false, stride=0, buffer=0.
    // VAO=0 케이스(macOS core profile)와 다르게 *spec이 보장*하는 default.
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    auto f = CaptureGLState();

    for (size_t i = 0; i < f.attribute_layouts.size(); ++i)
    {
        const auto& a = f.attribute_layouts[i];
        INFO("attribute slot " << i << " (VAO=" << vao << ", spec default)");
        REQUIRE_FALSE(a.enabled);
        REQUIRE(a.size == 4);
        REQUIRE(a.type == GL_FLOAT);
        REQUIRE_FALSE(a.normalized);
        REQUIRE(a.stride == 0);
        REQUIRE(a.buffer_binding == 0u);
    }

    glDeleteVertexArrays(1, &vao);
}

TEST_CASE("attribute_layouts — VAO 바인딩 + glVertexAttribPointer 후 layout 반영",
          "[diagnostics][capture][attribute_layouts]")
{
    // 핵심 회귀 검증: glVertexAttribPointer가 호출되면 캡처에 *그대로* 반영되어야 함.
    // 이게 깨지면 vertex layout 사보타지 (size=2 vs 3, stride 잘못, buffer_binding 잘못 등)를
    // 잡는 모든 후속 검증이 무효화됨.
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // attribute 0: vec3 GL_FLOAT, stride=24 (vec3 pos + vec3 color = 6 float = 24 byte)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (const void*)0);
    DrainGLErrors();

    auto f = CaptureGLState();

    const auto& a0 = f.attribute_layouts[0];
    REQUIRE(a0.enabled);              // glEnableVertexAttribArray 호출됨
    REQUIRE(a0.size == 3);            // vec3 (사보타지: size=2/4 다른 값으로 바뀌면 잡힘)
    REQUIRE(a0.type == GL_FLOAT);
    REQUIRE_FALSE(a0.normalized);
    REQUIRE(a0.stride == 24);         // 사보타지: stride 잘못 계산하면 잡힘
    REQUIRE(a0.buffer_binding == vbo); // 어느 VBO에서 오는지 (잘못된 VBO bind 시 잡힘)

    // 다른 slot (1)은 여전히 disabled — capture loop가 i<1로 줄어들면 잡힘
    REQUIRE_FALSE(f.attribute_layouts[1].enabled);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

TEST_CASE("attribute_layouts — multi-slot 활성화 + 다른 layout으로 차별",
          "[diagnostics][capture][attribute_layouts]")
{
    // 두 attribute가 *서로 다른* size/stride로 설정 — capture 결과의 [0]과 [1]이
    // 각자의 설정을 그대로 반영하는지 검증. 한 slot의 query 결과가 다른 slot에
    // 누설되지 않음을 보장.
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // attribute 0: vec3 (position), offset=0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (const void*)0);

    // attribute 1: vec2 (uv), offset=12 — stride 32 안에서 pos(12) 다음
    // 정수 타입 GL_UNSIGNED_BYTE + GL_TRUE로 normalized 검증
    // (GL_FLOAT 타입은 normalized 플래그가 spec상 ignored — driver마다 저장값 다름)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_TRUE, 32, (const void*)12);
    DrainGLErrors();

    auto f = CaptureGLState();

    REQUIRE(f.attribute_layouts[0].enabled);
    REQUIRE(f.attribute_layouts[0].size == 3);
    REQUIRE(f.attribute_layouts[0].type == GL_FLOAT);
    REQUIRE_FALSE(f.attribute_layouts[0].normalized);

    REQUIRE(f.attribute_layouts[1].enabled);
    REQUIRE(f.attribute_layouts[1].size == 2);   // vec2 — STUDY_NOTE Ex6 R-3의 사보타지가 size=4면 잡힘
    REQUIRE(f.attribute_layouts[1].type == GL_UNSIGNED_BYTE);
    REQUIRE(f.attribute_layouts[1].normalized);  // 정수 타입에서 normalized=GL_TRUE는 spec 보장

    // 두 attribute 같은 stride, 같은 VBO
    REQUIRE(f.attribute_layouts[0].stride == 32);
    REQUIRE(f.attribute_layouts[1].stride == 32);
    REQUIRE(f.attribute_layouts[0].buffer_binding == vbo);
    REQUIRE(f.attribute_layouts[1].buffer_binding == vbo);

    // [2..15]는 default
    for (size_t i = 2; i < 16; ++i)
    {
        INFO("attribute slot " << i);
        REQUIRE_FALSE(f.attribute_layouts[i].enabled);
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}
