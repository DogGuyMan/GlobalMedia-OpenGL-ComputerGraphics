/**
 * @file program.cpp
 * @brief Program 팩토리 + RAII 소멸자 + TryLink 구현.
 *
 * @details
 *  ### 책임
 *  - @c Create / @c CreateWithVSFS 팩토리 - 셰이더 attach + link + UniformCache eager build.
 *  - @c TryLink - @c glCreateProgram / @c glAttachShader / @c glLinkProgram 순서 캡슐화.
 *  - @c ~Program - @c glDeleteProgram + @c UniformDiagnostics::Invalidate 연계 해제.
 *
 *  ### 비-책임
 *  - [X] uniform 값 설정 - @c program_uniforms.cpp (@c SJH::Uniforms namespace) 에서 담당.
 *  - [X] @c UniformCache 빌드 로직 - @c UniformCache::Build 에 위임 (SP6).
 *
 *  ### 컴파일 타임 검증 (@c static_assert)
 *  - RAII 의미론: 복사/이동 생성/대입 모두 @c = delete 확인.
 *  - @c GetLocation / @c GetType 의 @c const 호출 가능성 확인.
 */
#include "program/program.h"
#include "diagnostics/gl_log.h"
#include "diagnostics/uniform_diagnostics.h"
#include <algorithm>
#include <type_traits>

// SP1 - RAII 의미론 컴파일 타임 검증.
// glDeleteProgram 이중 호출 위험 차단 - 명시적 = delete 가 필요.
static_assert(!std::is_copy_constructible_v<SJH::Program>,
              "SJH::Program must be non-copy-constructible (RAII)");
static_assert(!std::is_copy_assignable_v<SJH::Program>,
              "SJH::Program must be non-copy-assignable (RAII)");
static_assert(!std::is_move_constructible_v<SJH::Program>,
              "SJH::Program must be non-move-constructible (factory + UPtr only)");
static_assert(!std::is_move_assignable_v<SJH::Program>,
              "SJH::Program must be non-move-assignable (factory + UPtr only)");

// (SP2) GetLocation / GetType 이 const 호출 가능한지 컴파일 타임 검증.
static_assert(std::is_invocable_v<decltype(&SJH::Program::GetLocation), const SJH::Program&, const char*>,
              "Program::GetLocation must be const-callable (pure query)");
static_assert(std::is_invocable_v<decltype(&SJH::Program::GetType), const SJH::Program&, const char*>,
              "Program::GetType must be const-callable (pure query)");

namespace SJH
{
    ProgramUPtr Program::Create(const std::vector<ShaderPtr> &shaders)
    {
        auto program = ProgramUPtr(new Program());
        if (!program->TryLink(shaders))
            return nullptr;

        // SP6 - link 성공 직후 UniformCache eager build.
        program->mUniformCache.Build(*program);
        return program;
    }

    ProgramUPtr Program::CreateWithVSFS(const std::string &vertShaderFilename,
                                        const std::string &fragShaderFilename)
    {
        ShaderPtr vs = Shader::CreateFromFile(vertShaderFilename, GL_VERTEX_SHADER);
        ShaderPtr fs = Shader::CreateFromFile(fragShaderFilename, GL_FRAGMENT_SHADER);
        if (!vs || !fs)
            return nullptr;
        return Create({vs, fs});
    }

    Program::~Program()
    {
        if (mProgramAddr != 0)
        {
            Diagnostics::UniformDiagnostics::Invalidate(mProgramAddr);
            glDeleteProgram(mProgramAddr);
        }
    }

    bool Program::TryLink(const std::vector<ShaderPtr> &shaders)
    {
        mProgramAddr = glCreateProgram();
        for (auto &shader : shaders)
            glAttachShader(mProgramAddr, shader->GetShaderAddr());

        glLinkProgram(mProgramAddr);
        return SJH::Diagnostics::GLObjectLog::CheckProgramLink(mProgramAddr);
    }

}
