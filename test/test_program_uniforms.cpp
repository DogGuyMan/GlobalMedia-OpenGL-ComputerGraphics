/**
 * @file test_program_uniforms.cpp
 * @brief @c SJH::Uniforms 자유 함수 family 의 location 캐시 + setter 동작 검증.
 *
 * @details
 *  - 인라인 GLSL 셰이더를 @c Shader::CreateFromSource 로 컴파일 -> @c Program::Create 로 link.
 *  - link 직후 @c Program::Create 가 자동으로 멤버 @c BuildUniformCache 호출 -> 즉시 사용 가능.
 *  - 활성 uniform location 조회 / 미존재 uniform 핸들링 / setter crash 없음 검증.
 */

#include <catch2/catch_test_macros.hpp>

#include "support/gl_test_fixture.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "shader/shader.h"
#include "render/device_context.h"

#include "GL/gl3w.h"

namespace
{
    constexpr const char *kVertexSrc =
        "#version 330 core\n"
        "layout(location=0) in vec3 aPos;\n"
        "uniform mat4 uModel;\n"
        "uniform vec3 uTranslate;\n"
        "void main() { gl_Position = uModel * vec4(aPos + uTranslate, 1.0); }\n";

    constexpr const char *kFragmentSrc =
        "#version 330 core\n"
        "uniform vec4 uColor;\n"
        "uniform float uAlpha;\n"
        "out vec4 frag;\n"
        "void main() { frag = vec4(uColor.rgb, uColor.a * uAlpha); }\n";

    SJH::ProgramUPtr MakeTestProgram()
    {
        SJH::ShaderPtr vs = SJH::Shader::CreateFromSource(kVertexSrc,   GL_VERTEX_SHADER);
        SJH::ShaderPtr fs = SJH::Shader::CreateFromSource(kFragmentSrc, GL_FRAGMENT_SHADER);
        REQUIRE(vs != nullptr);
        REQUIRE(fs != nullptr);
        return SJH::Program::Create({vs, fs});
    }
}

TEST_CASE("Uniforms::Get — 활성 uniform 의 location 캐싱", "[program_uniforms]")
{
    SJH::test::GLContextFixture ctx;
    auto prog = MakeTestProgram();
    REQUIRE(prog != nullptr);

    SECTION("선언/사용된 uniform 은 location >= 0")
    {
        // 셰이더가 *실제로 사용* 하는 uniform 만 active. 옵티마이저가 미사용은 제거.
        REQUIRE(SJH::Uniforms::Get(*prog, "uModel")     >= 0);
        REQUIRE(SJH::Uniforms::Get(*prog, "uTranslate") >= 0);
        REQUIRE(SJH::Uniforms::Get(*prog, "uColor")     >= 0);
        REQUIRE(SJH::Uniforms::Get(*prog, "uAlpha")     >= 0);
    }

    SECTION("미존재 이름은 -1 + 두 번 호출도 안전 (lazy 보강 캐시 + warn-once)")
    {
        REQUIRE(SJH::Uniforms::Get(*prog, "uNonExistent") == -1);
        REQUIRE(SJH::Uniforms::Get(*prog, "uNonExistent") == -1);  // 동일 — silent
        REQUIRE(SJH::Uniforms::Get(*prog, "uAnotherMiss") == -1);  // 다른 이름 — warn
    }
}

TEST_CASE("Uniforms::Set* — setter family 가 crash 없이 GL 호출 위임",
          "[program_uniforms]")
{
    SJH::test::GLContextFixture ctx;
    auto prog = MakeTestProgram();
    REQUIRE(prog != nullptr);

    auto& rc = SJH::DeviceContext::Get();
    rc.UseProgram(*prog);   // setter 들이 의미 있게 동작하려면 use 상태여야 함.

    const vmath::mat4 identity = vmath::mat4::identity();
    const vmath::vec3 v3(1.0f, 2.0f, 3.0f);
    const vmath::vec4 v4(0.5f, 0.5f, 0.5f, 1.0f);

    SJH::Uniforms::SetMat4 (*prog, "uModel",     identity);
    SJH::Uniforms::SetVec3 (*prog, "uTranslate", v3);
    SJH::Uniforms::SetVec4 (*prog, "uColor",     v4);
    SJH::Uniforms::SetFloat(*prog, "uAlpha",     0.75f);

    // 미존재 이름 — diagnostics warn-once + GL 호출 skip (crash 없음).
    SJH::Uniforms::SetMat4(*prog, "uMissingMat", identity);
    SJH::Uniforms::SetVec4(*prog, "uMissingVec", v4);

    SUCCEED("모든 setter 호출이 crash 없이 완료");
    glUseProgram(0);
}

TEST_CASE("Uniforms::Get — 같은 이름 반복 호출은 캐시 히트", "[program_uniforms]")
{
    SJH::test::GLContextFixture ctx;
    auto prog = MakeTestProgram();
    REQUIRE(prog != nullptr);

    const GLint loc1 = SJH::Uniforms::Get(*prog, "uModel");
    const GLint loc2 = SJH::Uniforms::Get(*prog, "uModel");
    const GLint loc3 = SJH::Uniforms::Get(*prog, "uModel");

    REQUIRE(loc1 >= 0);
    REQUIRE(loc1 == loc2);
    REQUIRE(loc2 == loc3);
}
