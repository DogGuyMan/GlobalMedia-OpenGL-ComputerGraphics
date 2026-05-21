#include "program/program.h"
#include "material/material.h"   // SP6 Observer — Material::OnProgramReleased cascade
#include "diagnostics/gl_log.h"
#include "diagnostics/uniform_diagnostics.h"
#include <algorithm>
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

        // SP6 — link 성공 직후 UniformCache eager build.
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
        // SP6 Observer cascade — 의존 Material 들에 release 통지.
        // OnProgramReleased 안에서 Material 이 UnregisterMaterial 호출 가능 -> 복사본 순회.
        const auto dependents = mDependentMaterials;
        for (auto* m : dependents)
            if (m) m->OnProgramReleased(this);

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

    void Program::RegisterMaterial(Material* m) const
    {
        if (!m) return;
        // 중복 등록 방지 — Material::SetProgram 이 한 Program 에 두 번 등록 불가.
        if (std::find(mDependentMaterials.begin(), mDependentMaterials.end(), m)
            == mDependentMaterials.end())
        {
            mDependentMaterials.push_back(m);
        }
    }

    void Program::UnregisterMaterial(Material* m) const
    {
        mDependentMaterials.erase(
            std::remove(mDependentMaterials.begin(), mDependentMaterials.end(), m),
            mDependentMaterials.end());
    }
}
