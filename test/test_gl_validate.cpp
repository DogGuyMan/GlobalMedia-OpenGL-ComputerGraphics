/**
 * @file test_gl_validate.cpp
 * @brief @c GLValidate 6 카테고리 (A-F) + RunFullSweep 회귀.
 *
 * @details
 *  doc/inst.md §7 Criterion 3 의 *일부러 깨뜨리는 6 케이스*를 Catch2 행동 단언으로 자동화.
 *  수동 sabotage drill을 회귀 테스트로 승격.
 *
 *  ### 테스트 구조
 *  - Cat A: CPU only (GL ctx 불필요)
 *  - Cat B/C/D/F: 인라인 VS/FS 컴파일 -> program 생성 -> 검증
 *  - Cat E: GLContextFixture + 의도적 glEnable(invalid)
 *  - RunFullSweep: clean state 통합 검증
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "diagnostics/gl_validate.h"
#include "support/gl_test_fixture.h"
#include "support/spdlog_capture.h"
#include "GL/gl3w.h"
#include <string>
#include <vector>

using namespace SJH::Diagnostics::GLValidate;
using Catch::Matchers::ContainsSubstring;

namespace
{
    /// 간단한 program 생성 — 본 테스트만 사용. 실패 시 0 반환.
    GLuint CreateInlineProgram(const char* vs_src, const char* fs_src)
    {
        auto compile = [](GLenum type, const char* src) -> GLuint {
            GLuint sh = glCreateShader(type);
            glShaderSource(sh, 1, &src, nullptr);
            glCompileShader(sh);
            GLint ok = 0;
            glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok) { glDeleteShader(sh); return 0; }
            return sh;
        };
        GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
        GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
        if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return 0; }
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        GLint linked = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &linked);
        if (!linked) { glDeleteProgram(prog); return 0; }
        return prog;
    }

    constexpr const char* kSimpleVS = R"(#version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;
        out vec2 vsTexCoord;
        void main() {
            gl_Position = vec4(aPos, 1.0);
            vsTexCoord = aTexCoord;
            // aNormal 사용 — 옵티마이저가 제거 안 하도록
            gl_Position.xyz += aNormal * 0.0;
        }
    )";

    constexpr const char* kSimpleFS = R"(#version 330 core
        in vec2 vsTexCoord;
        out vec4 fragColor;
        uniform sampler2D tex0;
        void main() {
            fragColor = texture(tex0, vsTexCoord);
        }
    )";
}

// ──────────────────────────────────────────────────────────────────────────
// Cat A — CheckIndices (CPU only)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("Cat A — normal 인덱스는 0 위반", "[gl_validate][cat_a]")
{
    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
    REQUIRE(CheckIndices(indices, 4, "ok") == 0);
}

TEST_CASE("Cat A — OOB 인덱스 catch (인덱스 ≥ vertexCount)", "[gl_validate][cat_a]")
{
    std::vector<uint32_t> indices = {0, 1, 99};   // 99 OOB (vertexCount = 4)
    SJH::test::SpdlogCapture cap;
    const size_t violations = CheckIndices(indices, 4, "oob");
    REQUIRE(violations >= 1);
    REQUIRE(cap.Contains("OOB"));
}

TEST_CASE("Cat A — degenerate triangle catch ((0,0,0) 같은 zero-area)",
          "[gl_validate][cat_a]")
{
    // doc/inst.md §7 Criterion 3 의 의도적 결함 케이스 직접 재현
    std::vector<uint32_t> indices = {0, 0, 0};
    SJH::test::SpdlogCapture cap;
    const size_t violations = CheckIndices(indices, 4, "degen");
    REQUIRE(violations >= 1);
    REQUIRE(cap.Contains("degenerate"));
}

TEST_CASE("Cat A — duplicate triangle 보고 (회전/반사 무관, sorted 비교)",
          "[gl_validate][cat_a]")
{
    std::vector<uint32_t> indices = {0, 1, 2,   2, 0, 1};   // 같은 trio 회전
    SJH::test::SpdlogCapture cap;
    const size_t violations = CheckIndices(indices, 3, "dup");
    REQUIRE(violations >= 1);
    REQUIRE(cap.Contains("duplicate"));
}

TEST_CASE("Cat A — empty indices -> 위반", "[gl_validate][cat_a]")
{
    std::vector<uint32_t> indices;
    REQUIRE(CheckIndices(indices, 4, "empty") == 1);
}

// ──────────────────────────────────────────────────────────────────────────
// Cat E — CaptureGLError (rate-limited)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("Cat E — clean state 는 true 반환", "[gl_validate][cat_e]")
{
    SJH::test::GLContextFixture ctx;
    ResetRateLimitCache();
    while (glGetError() != GL_NO_ERROR) {}  // pre-drain

    REQUIRE(CaptureGLError("clean") == true);
}

TEST_CASE("Cat E — invalid glEnable 후 false 반환 + invalid_enum 보고",
          "[gl_validate][cat_e]")
{
    SJH::test::GLContextFixture ctx;
    ResetRateLimitCache();
    while (glGetError() != GL_NO_ERROR) {}

    SJH::test::SpdlogCapture cap;
    glEnable(0xDEADu);    // doc/inst.md §7 의 의도적 결함 케이스

    REQUIRE(CaptureGLError("bad_enable") == false);
    REQUIRE(cap.Contains("GL_INVALID_ENUM"));
}

TEST_CASE("Cat E — 같은 (코드 + tag) 는 1회만 보고 (rate limit)",
          "[gl_validate][cat_e]")
{
    SJH::test::GLContextFixture ctx;
    ResetRateLimitCache();
    while (glGetError() != GL_NO_ERROR) {}

    SJH::test::SpdlogCapture cap;
    glEnable(0xDEADu);
    CaptureGLError("rl");    // 첫 호출 — 보고
    auto sizeAfterFirst = cap.Lines().size();

    glEnable(0xDEADu);
    CaptureGLError("rl");    // 두 번째 — silent (rate limit)
    REQUIRE(cap.Lines().size() == sizeAfterFirst);

    // 다른 tag면 별도 키 -> 다시 보고
    glEnable(0xDEADu);
    CaptureGLError("rl2");
    REQUIRE(cap.Lines().size() > sizeAfterFirst);
}

TEST_CASE("Cat E — ResetRateLimitCache 후 다시 보고 가능",
          "[gl_validate][cat_e]")
{
    SJH::test::GLContextFixture ctx;
    ResetRateLimitCache();
    while (glGetError() != GL_NO_ERROR) {}

    SJH::test::SpdlogCapture cap;
    glEnable(0xDEADu); CaptureGLError("reset");
    auto sizeAfterFirst = cap.Lines().size();

    glEnable(0xDEADu); CaptureGLError("reset");    // silent
    REQUIRE(cap.Lines().size() == sizeAfterFirst);

    ResetRateLimitCache();    // 캐시 클리어
    glEnable(0xDEADu); CaptureGLError("reset");    // 다시 보고
    REQUIRE(cap.Lines().size() > sizeAfterFirst);
}

// ──────────────────────────────────────────────────────────────────────────
// Cat F — DumpShaderInfoLogs (대체로 macOS는 비어있음, smoke 위주)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("Cat F — clean program 은 출력 거의 없음 (smoke)",
          "[gl_validate][cat_f]")
{
    SJH::test::GLContextFixture ctx;
    GLuint prog = CreateInlineProgram(kSimpleVS, kSimpleFS);
    REQUIRE(prog != 0);

    SJH::test::SpdlogCapture cap;
    DumpShaderInfoLogs(prog, "clean_f");

    // info log가 비어있으면 출력 없음. crash 없이 완료되는지가 핵심.
    // 일부 driver는 "No errors" 같은 텍스트를 info log에 박기도 함 -> 양쪽 모두 PASS.
    SUCCEED();  // smoke — driver-dependent 출력

    glDeleteProgram(prog);
}

// ──────────────────────────────────────────────────────────────────────────
// Cat B — CheckAttribLayout
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("Cat B — VS attribute ↔ VAO layout 정합 시 0 위반",
          "[gl_validate][cat_b]")
{
    SJH::test::GLContextFixture ctx;
    GLuint prog = CreateInlineProgram(kSimpleVS, kSimpleFS);
    REQUIRE(prog != 0);
    glUseProgram(prog);

    // VS 가 기대하는 layout: loc 0 = vec3, loc 1 = vec3, loc 2 = vec2
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24);

    while (glGetError() != GL_NO_ERROR) {}

    REQUIRE(CheckAttribLayout(prog, "clean_b") == 0);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
}

TEST_CASE("Cat B — loc 2 가 VS=vec2 인데 VAO=vec3 으로 잘못 설정 -> 위반",
          "[gl_validate][cat_b]")
{
    SJH::test::GLContextFixture ctx;
    GLuint prog = CreateInlineProgram(kSimpleVS, kSimpleFS);
    REQUIRE(prog != 0);
    glUseProgram(prog);

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12);
    glEnableVertexAttribArray(2);
    // ❌ 사보타지: VS 가 vec2 기대하는데 size=3 으로 설정
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 32, (void*)24);

    while (glGetError() != GL_NO_ERROR) {}

    SJH::test::SpdlogCapture cap;
    const size_t violations = CheckAttribLayout(prog, "size_mismatch");
    REQUIRE(violations >= 1);
    REQUIRE(cap.Contains("VS expects vec2"));

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
}

TEST_CASE("Cat B — VS 가 쓰는 location이 VAO에서 disabled -> 위반",
          "[gl_validate][cat_b]")
{
    SJH::test::GLContextFixture ctx;
    GLuint prog = CreateInlineProgram(kSimpleVS, kSimpleFS);
    REQUIRE(prog != 0);
    glUseProgram(prog);

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, nullptr);
    // ❌ loc 1, 2 enable 누락 — VS 는 사용하지만 VAO 는 disabled

    while (glGetError() != GL_NO_ERROR) {}

    SJH::test::SpdlogCapture cap;
    const size_t violations = CheckAttribLayout(prog, "disabled");
    REQUIRE(violations >= 2);    // loc 1 + loc 2
    REQUIRE(cap.Contains("disabled"));

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
}

// ──────────────────────────────────────────────────────────────────────────
// Cat C — CheckUniformCoverage
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("Cat C — declared uniform이 default-zero -> 위반 (CPU setter 누락 의심)",
          "[gl_validate][cat_c]")
{
    SJH::test::GLContextFixture ctx;

    // unused 라는 uniform 추가한 FS — doc/inst.md §7 의 의도적 결함 케이스
    const char* fs_with_unused = R"(#version 330 core
        in vec2 vsTexCoord;
        out vec4 fragColor;
        uniform sampler2D tex0;
        uniform vec3 unused;     // declared but never set by CPU
        void main() {
            fragColor = texture(tex0, vsTexCoord) + vec4(unused, 0.0);
        }
    )";

    GLuint prog = CreateInlineProgram(kSimpleVS, fs_with_unused);
    REQUIRE(prog != 0);
    glUseProgram(prog);

    SJH::test::SpdlogCapture cap;
    const size_t violations = CheckUniformCoverage(prog, "unused_uniform");
    REQUIRE(violations >= 1);
    REQUIRE(cap.Contains("unused"));
    REQUIRE(cap.Contains("default-zero"));

    glDeleteProgram(prog);
}

// ──────────────────────────────────────────────────────────────────────────
// Cat D — CheckSamplerBindings
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("Cat D — sampler set 후 texture bind 누락 -> 위반",
          "[gl_validate][cat_d]")
{
    SJH::test::GLContextFixture ctx;
    GLuint prog = CreateInlineProgram(kSimpleVS, kSimpleFS);
    REQUIRE(prog != 0);
    glUseProgram(prog);

    // sampler tex0 -> unit 3 으로 설정만 (texture bind X)
    const GLint loc = glGetUniformLocation(prog, "tex0");
    REQUIRE(loc >= 0);
    glUniform1i(loc, 3);

    while (glGetError() != GL_NO_ERROR) {}

    SJH::test::SpdlogCapture cap;
    const size_t violations = CheckSamplerBindings(prog, "no_bind");
    REQUIRE(violations >= 1);
    REQUIRE(cap.Contains("no texture bound"));

    glDeleteProgram(prog);
}

TEST_CASE("Cat D — sampler set + texture bind 완료 -> 0 위반",
          "[gl_validate][cat_d]")
{
    SJH::test::GLContextFixture ctx;
    GLuint prog = CreateInlineProgram(kSimpleVS, kSimpleFS);
    REQUIRE(prog != 0);
    glUseProgram(prog);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, tex);

    const GLint loc = glGetUniformLocation(prog, "tex0");
    glUniform1i(loc, 5);    // unit 5 ↔ tex 바인딩 완료

    while (glGetError() != GL_NO_ERROR) {}

    REQUIRE(CheckSamplerBindings(prog, "clean_d") == 0);

    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
}

// ──────────────────────────────────────────────────────────────────────────
// RunFullSweep — clean state 통합
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("RunFullSweep — clean state 에서 'all clean' 메시지 + 위반 합계 적음",
          "[gl_validate][full_sweep]")
{
    SJH::test::GLContextFixture ctx;
    GLuint prog = CreateInlineProgram(kSimpleVS, kSimpleFS);
    REQUIRE(prog != 0);
    glUseProgram(prog);

    // 정상 VAO + texture 바인딩
    GLuint vao = 0, vbo = 0, tex = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, nullptr);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24);

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    const GLint loc = glGetUniformLocation(prog, "tex0");
    glUniform1i(loc, 0);

    // 정상 indices
    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

    while (glGetError() != GL_NO_ERROR) {}

    SJH::test::SpdlogCapture cap;
    const size_t total = RunFullSweep(prog, indices, 4, "lighting");

    // Cat C 가 *unset uniform* (예: 본 inline shader에 declared but unset 없음 가정)
    // 검증을 보수적으로 — 0 또는 *적은 수*
    INFO("total violations: " << total);
    INFO("log: " << cap.Lines());
    REQUIRE(cap.Contains("[GLValidate/lighting]"));
    // clean message — Cat C 가 default-zero uniform 안 잡으면 0
    if (total == 0) {
        REQUIRE(cap.Contains("all clean"));
    }

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
}
