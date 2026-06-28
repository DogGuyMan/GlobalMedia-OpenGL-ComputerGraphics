/**
 * @file test_shader_link.cpp
 * @brief C-1 셰이더 링크검증 - 독립 재컴파일된 GLSL410 .vert/.frag 쌍을
 *        raw GL 로 컴파일/링크하고 diagnostics 오라클로 판정한다.
 *
 * @details
 *  ### 왜 raw GL 인가 (★ 핵심)
 *  @c SJH::Shader::CreateFromSource / @c SJH::Program::Create 는 F-2 정책상
 *  컴파일/링크 실패를 hard-fail(abort/throw) 로 드러낸다. 테스트에서 그대로 쓰면
 *  실패 셰이더 하나가 *테스트 러너 자체를 죽인다*. 따라서 본 테스트는
 *  @c glCreateShader / @c glCompileShader / @c glLinkProgram 을 직접 호출하고,
 *  판정만 diagnostics 의 *bool / 구조화 반환* 오라클에 위임한다.
 *    - 컴파일: @c GLObjectLog::CheckShaderCompile (bool)
 *    - 링크  : @c CheckProgramLinkReport -> @c LinkReport{ok, infoLog, hasError}
 *  이렇게 하면 macOS Slang varying/sampler 함정(glslang 통과 + GL 런타임만 거부)을
 *  *abort 가 아닌 graceful 한 테스트 실패* 로 잡는다.
 *
 *  ### 입력
 *  CMake 사전스텝(TX-C1)이 @c apps/_MyApp_/shaders_slang 의 .slang 들을 프로젝트 SSOT
 *  파이프라인(@c sjh_compile_slang -> @c scripts/slang_compile.py post-process)으로
 *  GLSL410 재컴파일해 @c SJH_GPU_SHADER_DIR 에 @c <name>.vert / @c <name>.frag 로 산출.
 *  (post-process 정규화는 SSOT 스크립트 재사용 - 재구현 금지.)
 *
 *  ### 입력 셰이더 목록
 *  컴파일 가능한 entry(vsMain/fsMain)를 가진 16종. (phong_lighting 은 import 전용
 *  모듈이라 entry 가 없어 제외 - 프로덕션 CMakeLists 도 직접 컴파일 안 함.)
 *  목록은 CMake 가 생성하는 @c gpu_shader_names.generated.h (kGpuShaderNames 배열) 로 주입.
 */

#include "gl_context_fixture.h"
#include "gpu_shader_names.generated.h"

#include "diagnostics/gl_log.h"
#include "diagnostics/gl_validate.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace diag = SJH::Diagnostics;

namespace
{
    /// @brief 파일 전체를 문자열로 읽기 (바이너리 모드 - 크로스플랫폼 규칙).
    /// @return 읽기 성공 시 내용, 실패 시 빈 optional 대신 (found=false).
    bool ReadFile(const std::string &path, std::string &outContent)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
        {
            return false;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        outContent = ss.str();
        return true;
    }
}

TEST_CASE("Slang 재컴파일 GLSL410 셰이더가 GL 에서 컴파일+링크된다", "[gpu][shader]")
{
    // 컨텍스트 픽스처(C-0)가 세션 시작에 init 되어 있어야 한다.
    REQUIRE(SJH::Test::Gpu::GLContextFixture::IsValid());

    const std::string shaderDir = SJH_GPU_SHADER_DIR; // CMake 주입 절대경로.

    // 셰이더 basename 목록 - CMake 생성 헤더(kGpuShaderNames).
    const std::vector<std::string> names(
        std::begin(kGpuShaderNames), std::end(kGpuShaderNames));

    REQUIRE_FALSE(names.empty()); // CMake 배선 누락 시 즉시 드러남.

    // 각 셰이더 basename 을 한 케이스로 펼친다 (실패 시 어느 셰이더인지 명확).
    const std::string name = GENERATE_REF(Catch::Generators::from_range(names));

    SECTION(name)
    {
        const std::string vsPath = shaderDir + "/" + name + ".vert";
        const std::string fsPath = shaderDir + "/" + name + ".frag";

        std::string vsSrc, fsSrc;
        INFO("vert: " << vsPath);
        INFO("frag: " << fsPath);
        REQUIRE(ReadFile(vsPath, vsSrc));
        REQUIRE(ReadFile(fsPath, fsSrc));
        REQUIRE_FALSE(vsSrc.empty());
        REQUIRE_FALSE(fsSrc.empty());

        // --- raw GL: VS 컴파일 ---
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        REQUIRE(vs != 0u);
        const char *vsPtr = vsSrc.c_str();
        glShaderSource(vs, 1, &vsPtr, nullptr);
        glCompileShader(vs);
        const bool vsOk = diag::GLObjectLog::CheckShaderCompile(vs, vsPath);
        INFO("(실패 시) vertex 셰이더 컴파일 대상: " << vsPath);
        CHECK(vsOk);

        // --- raw GL: FS 컴파일 ---
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        REQUIRE(fs != 0u);
        const char *fsPtr = fsSrc.c_str();
        glShaderSource(fs, 1, &fsPtr, nullptr);
        glCompileShader(fs);
        const bool fsOk = diag::GLObjectLog::CheckShaderCompile(fs, fsPath);
        INFO("(실패 시) fragment 셰이더 컴파일 대상: " << fsPath);
        CHECK(fsOk);

        // --- raw GL: 링크 + LinkReport 오라클 ---
        GLuint prog = glCreateProgram();
        REQUIRE(prog != 0u);
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);

        const diag::GLValidate::LinkReport report = diag::GLValidate::CheckProgramLinkReport(prog);
        INFO("(실패 시) 링크 대상 셰이더: " << name << "\nInfoLog:\n" << report.infoLog);
        CHECK(report.ok);
        CHECK_FALSE(report.hasError);

        // 정리 (셰이더는 attach 후 detach 불요 - delete 로 즉시 표시, program delete 가 마무리).
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(prog);
    }
}
