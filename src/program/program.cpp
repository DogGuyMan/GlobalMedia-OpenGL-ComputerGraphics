#include "program/program.h"
#include "diagnostics/gl_log.h"
#include "diagnostics/uniform_diagnostics.h"
#include <type_traits>

// SP1 — RAII 의미론 컴파일 타임 검증.
// glDeleteProgram 이중 호출 위험 차단 — 명시적 = delete 가 필요.
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

        // link 성공 직후 uniform 캐시 eager build — 호출자는 즉시 Uniforms::Set* 호출 가능.
        program->BuildUniformCache();
        return program;
    }

    ProgramUPtr Program::CreateWithVSFS(const std::string &vertShaderFilename,
                                        const std::string &fragShaderFilename)
    {
        ShaderPtr vs = Shader::CreateFromFile(vertShaderFilename,
                                              GL_VERTEX_SHADER);
        ShaderPtr fs = Shader::CreateFromFile(fragShaderFilename,
                                              GL_FRAGMENT_SHADER);
        if (!vs || !fs)
            return nullptr;
        return std::move(Create({vs, fs}));
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
        // 모든 셰이더를 program 에 attach — 링크 시 셰이더 단계가 결합됨
        for (auto &shader : shaders)
            glAttachShader(mProgramAddr, shader->GetShaderAddr());

        glLinkProgram(mProgramAddr);
        return SJH::Diagnostics::GLObjectLog::CheckProgramLink(mProgramAddr);
    }

    void Program::BuildUniformCache()
    {
        GLint count = 0;
        glGetProgramiv(mProgramAddr, GL_ACTIVE_UNIFORMS, &count);
        if (count <= 0) return;

        GLint maxNameLen = 0;
        glGetProgramiv(mProgramAddr, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLen);
        if (maxNameLen <= 0) return;

        std::vector<char> nameBuf(static_cast<size_t>(maxNameLen + 1), '\0');
        for (GLint i = 0; i < count; ++i)
        {
            GLsizei nameSize = 0;
            GLint   size     = 0;
            GLenum  type     = 0;
            glGetActiveUniform(mProgramAddr, static_cast<GLuint>(i),
                               maxNameLen, &nameSize, &size, &type, nameBuf.data());

            const GLint loc = glGetUniformLocation(mProgramAddr, nameBuf.data());
            mUniformCache.emplace(
                std::string(nameBuf.data(), static_cast<size_t>(nameSize)),
                UniformEntry{loc, type});
        }
    }

    GLint Program::GetLocation(const char* name) const
    {
        auto it = mUniformCache.find(name);
        if (it == mUniformCache.end()) return -1;
        return it->second.Location;
    }

    GLenum Program::GetType(const char* name) const
    {
        auto it = mUniformCache.find(name);
        if (it == mUniformCache.end()) return 0;
        return it->second.Type;
    }
}
