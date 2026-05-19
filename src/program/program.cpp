#include "program/program.h"
#include "diagnostics/gl_log.h"
#include "diagnostics/uniform_diagnostics.h"
#include "program/program_uniforms.h"

namespace SJH
{
    ProgramUPtr Program::Create(const std::vector<ShaderPtr> &shaders)
    {
        auto program = ProgramUPtr(new Program());
        if (!program->TryLink(shaders))
            return nullptr;

        // link 성공 직후 uniform 캐시 eager build — 호출자는 즉시 Uniforms::Set* 호출 가능.
        Uniforms::BuildCache(*program);
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
            // GL ID 가 다른 프로그램에 재할당될 가능성 -> 외부 캐시 항목 먼저 제거.
            Uniforms::Forget(mProgramAddr);
            Diagnostics::UniformDiagnostics::Invalidate(mProgramAddr);
            glDeleteProgram(mProgramAddr);
        }
    }

    void Program::Use() const
    {
        glUseProgram(mProgramAddr);
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
}
