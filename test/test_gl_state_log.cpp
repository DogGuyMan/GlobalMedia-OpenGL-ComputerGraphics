/**
 * @file test_gl_state_log.cpp
 * @brief GLStateLog::Dump + EnableAutoOnError 회귀 + FieldsToString 출력 형식 검증.
 *
 * @details
 *  - Dump가 spdlog::info로 *FieldsToString의 출력*을 그대로 흘려보내는지.
 *  - tag 인자가 출력 prefix에 포함되는지.
 *  - macOS GL 3.3 core profile (KHR_debug 미지원)에서 EnableAutoOnError가 1회 warn 후 no-op인지.
 *  - FieldsToString의 audit 트랙 A 출력 (attribute_layouts) 검증.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "support/gl_test_fixture.h"
#include "support/spdlog_capture.h"
#include "diagnostics/gl_state_log.h"
#include "diagnostics/gl_state_fields.h"
#include "GL/gl3w.h"

using SJH::Diagnostics::GLStateLog;
using SJH::Diagnostics::FieldsToString;
using SJH::Diagnostics::GLStateFields;
using Catch::Matchers::ContainsSubstring;

// ──────────────────────────────────────────────────────────────────────────
// GLStateLog::Dump
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("GLStateLog::Dump — tag가 출력 prefix에 포함", "[diagnostics][state_log]")
{
    SJH::test::GLContextFixture ctx;
    SJH::test::SpdlogCapture cap;

    GLStateLog::Dump("after_init");

    REQUIRE(cap.Contains("after_init"));
    // FieldsToString이 출력하는 핵심 라벨 — tag와 함께 흘러나와야 함
    REQUIRE(cap.Contains("vao:"));
    REQUIRE(cap.Contains("viewport:"));
}

TEST_CASE("GLStateLog::Dump — VAO 바인딩 후 핸들 raw 정수로 출력",
          "[diagnostics][state_log]")
{
    SJH::test::GLContextFixture ctx;
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    SJH::test::SpdlogCapture cap;
    GLStateLog::Dump();

    // 비대칭 정책: handle은 raw 정수로 출력됨
    REQUIRE(cap.Contains(std::to_string(vao)));

    glDeleteVertexArrays(1, &vao);
}

TEST_CASE("GLStateLog::Dump — tag 없이 호출 시 prefix 깔끔", "[diagnostics][state_log]")
{
    SJH::test::GLContextFixture ctx;
    SJH::test::SpdlogCapture cap;

    GLStateLog::Dump();  // empty tag

    // tag 없을 때는 [GLStateLog] 형식만 — [GLStateLog/...] 형식 없음
    REQUIRE(cap.Contains("[GLStateLog]"));
    REQUIRE_FALSE(cap.Contains("[GLStateLog/]"));  // 빈 tag 잘못 처리 방지
}

// ──────────────────────────────────────────────────────────────────────────
// GLStateLog::EnableAutoOnError — macOS GL 3.3 core profile 한정 검증
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("GLStateLog::EnableAutoOnError — KHR_debug 미지원 환경에서 1회 warn",
          "[diagnostics][state_log]")
{
    SJH::test::GLContextFixture ctx;
    SJH::test::SpdlogCapture cap;

    GLStateLog::EnableAutoOnError(true);

#if defined(__APPLE__)
    // macOS 환경: KHR_debug 콜백 함수 포인터가 nullptr -> no-op + warn
    REQUIRE((cap.Contains("KHR_debug") || cap.Contains("no-op")));
#endif
    // 다른 환경에서는 정상 등록되어 다른 메시지 출력 가능 — 본 케이스는 macOS 한정.
}

TEST_CASE("GLStateLog::EnableAutoOnError — 두 번째 호출은 silent (call_once)",
          "[diagnostics][state_log]")
{
    SJH::test::GLContextFixture ctx;

    // 본 테스트의 invariant는 *프로세스 단위* call_once. 다른 테스트가 먼저 EnableAutoOnError를
    // 호출했다면 첫 warn이 이미 사라진 상태. 그래도 추가 호출이 *추가 warn을 만들지 않는다*는
    // 단언은 유효 (전후 라인 길이 비교).
    SJH::test::SpdlogCapture cap;
    GLStateLog::EnableAutoOnError(true);
    auto sizeAfterFirst = cap.Lines().size();

    GLStateLog::EnableAutoOnError(true);
    GLStateLog::EnableAutoOnError(true);
    REQUIRE(cap.Lines().size() == sizeAfterFirst);  // 추가 출력 없음 (call_once)
}

// ──────────────────────────────────────────────────────────────────────────
// FieldsToString — 직접 검증 (audit 트랙 A의 attribute_layouts 포함)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("FieldsToString — VAO=0 시 EBO 주석 포함", "[diagnostics][fields_to_string]")
{
    GLStateFields f{};  // default init: vao=0
    auto str = FieldsToString(f);

    REQUIRE_THAT(str, ContainsSubstring("EBO state is per-VAO"));
}

TEST_CASE("FieldsToString — VAO≠0 시 EBO 주석 미포함", "[diagnostics][fields_to_string]")
{
    GLStateFields f{};
    f.vao = 3;
    auto str = FieldsToString(f);

    REQUIRE_FALSE(str.find("EBO state is per-VAO") != std::string::npos);
    // 그래도 element_buffer 라벨은 출력되어야 함
    REQUIRE_THAT(str, ContainsSubstring("element_buffer:"));
}

TEST_CASE("FieldsToString — enum 필드는 SymbolicName 적용", "[diagnostics][fields_to_string]")
{
    GLStateFields f{};
    f.depth_func = GL_LEQUAL;
    f.cull_face_mode = GL_FRONT;

    auto str = FieldsToString(f);

    REQUIRE_THAT(str, ContainsSubstring("GL_LEQUAL"));
    REQUIRE_THAT(str, ContainsSubstring("GL_FRONT"));
}

TEST_CASE("FieldsToString — texture unit 0 모두면 '(all units empty)'",
          "[diagnostics][fields_to_string]")
{
    GLStateFields f{};  // default: texture_2d_per_unit 모두 0
    auto str = FieldsToString(f);

    REQUIRE_THAT(str, ContainsSubstring("all units empty"));
}

TEST_CASE("FieldsToString — texture unit 일부 활성 시 unit 번호와 핸들 출력",
          "[diagnostics][fields_to_string]")
{
    GLStateFields f{};
    f.texture_2d_per_unit[3] = 42;
    f.texture_2d_per_unit[7] = 99;

    auto str = FieldsToString(f);

    REQUIRE_THAT(str, ContainsSubstring("tex_2d[unit 3]"));
    REQUIRE_THAT(str, ContainsSubstring("42"));
    REQUIRE_THAT(str, ContainsSubstring("tex_2d[unit 7]"));
    REQUIRE_THAT(str, ContainsSubstring("99"));
    REQUIRE_FALSE(str.find("all units empty") != std::string::npos);
}

// ──────────────────────────────────────────────────────────────────────────
// FieldsToString — audit 트랙 A: attribute_layouts 출력
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("FieldsToString — attribute 모두 disabled면 '(all disabled)'",
          "[diagnostics][fields_to_string][attribute_layouts]")
{
    GLStateFields f{};  // default: 모든 attribute disabled
    auto str = FieldsToString(f);

    REQUIRE_THAT(str, ContainsSubstring("attrib"));
    REQUIRE_THAT(str, ContainsSubstring("all disabled"));
}

TEST_CASE("FieldsToString — attribute enabled 시 size/type/stride/vbo 출력",
          "[diagnostics][fields_to_string][attribute_layouts]")
{
    GLStateFields f{};
    f.attribute_layouts[0].enabled = true;
    f.attribute_layouts[0].size = 3;
    f.attribute_layouts[0].type = GL_FLOAT;
    f.attribute_layouts[0].normalized = false;
    f.attribute_layouts[0].stride = 24;
    f.attribute_layouts[0].buffer_binding = 5;

    auto str = FieldsToString(f);

    REQUIRE_THAT(str, ContainsSubstring("attrib[0]"));
    REQUIRE_THAT(str, ContainsSubstring("vec3"));        // size 출력
    REQUIRE_THAT(str, ContainsSubstring("GL_FLOAT"));    // type SymbolicName
    REQUIRE_THAT(str, ContainsSubstring("stride=24"));
    REQUIRE_THAT(str, ContainsSubstring("vbo=5"));
    REQUIRE_FALSE(str.find("all disabled") != std::string::npos);
}

TEST_CASE("FieldsToString — multi-slot enabled 시 각 slot 별도 출력",
          "[diagnostics][fields_to_string][attribute_layouts]")
{
    GLStateFields f{};
    f.attribute_layouts[0].enabled = true;
    f.attribute_layouts[0].size = 3;
    f.attribute_layouts[0].buffer_binding = 1;
    f.attribute_layouts[2].enabled = true;
    f.attribute_layouts[2].size = 2;
    f.attribute_layouts[2].buffer_binding = 1;

    auto str = FieldsToString(f);

    REQUIRE_THAT(str, ContainsSubstring("attrib[0]"));
    REQUIRE_THAT(str, ContainsSubstring("attrib[2]"));
    // disabled인 slot 1은 출력 안 됨
    REQUIRE_FALSE(str.find("attrib[1]") != std::string::npos);
}
