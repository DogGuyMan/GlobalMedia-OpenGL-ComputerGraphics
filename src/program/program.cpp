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
#include <cctype>      // Phase 2 T3 - NormalizeBlockName 의 isdigit 판정.
#include <cstdio>      // Phase 2 T3 - UBO 생성 실패 시 stderr 출력 (fail-fast 로그).
#include <cstdlib>     // Phase 2 T3 - std::abort (UBO 생성 실패 정통 fail-fast).
#include <type_traits>
#include <utility>     // Phase 2 T3 - std::move (UniformBlock 보관).
#include <vector>      // Phase 2 T3 - BuildUniformBlocks 의 이름 버퍼.

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

        // Phase 2 T3 - Slang UBO 블록 introspection + UBO 객체 생성 (비-UBO 셰이더는 빈 벡터).
        program->BuildUniformBlocks();
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

    // ─────────────────────────────────────────────────────────────────────────
    // Phase 2 T3 - Slang UBO 블록 introspection + 자기기술
    // ─────────────────────────────────────────────────────────────────────────
    namespace
    {
        /**
         * @brief Slang GLSL 출력 블록명 "block_<T>_<digits>" 에서 정규화 이름 "<T>" 추출.
         * @param raw Slang 출력 그대로의 블록명 (예: "block_FrameBlock_0").
         * @return 정규화 이름 (예: "FrameBlock"). 접두 "block_" 없으면 그대로, 접미 "_<숫자>" 없으면 그대로.
         * @details 셰이더 측 @c struct 이름과 동일 문자열 키로 호출자가 조회 가능하게 매핑.
         *          호출자는 @c FindUniformBlock("FrameBlock") 으로 검색.
         */
        std::string NormalizeBlockName(const std::string& raw)
        {
            std::string s = raw;
            const std::string prefix = "block_";
            if (s.rfind(prefix, 0) == 0)
                s = s.substr(prefix.size());

            const std::size_t us = s.find_last_of('_');
            if (us != std::string::npos && us + 1 < s.size())
            {
                bool allDigit = true;
                for (std::size_t i = us + 1; i < s.size(); ++i)
                {
                    if (!std::isdigit(static_cast<unsigned char>(s[i])))
                    {
                        allDigit = false;
                        break;
                    }
                }
                if (allDigit)
                    s = s.substr(0, us);
            }
            return s;
        }

        /**
         * @brief UBO 멤버 GL 이름에서 author 멤버명 추출 (Phase 3 Slice 0).
         * @param raw GL active uniform 이름 (예: "block_MaterialBlock_0.baseColor_0" 또는 "baseColor_0").
         * @return author 이름 (예: "baseColor"). '.' 뒤(블록 접두 제거) + 접미 "_<숫자>" 제거.
         * @details @ref NormalizeBlockName 의 멤버 버전 - 셰이더 저작 이름(material Properties 키)과 일치시킨다.
         */
        std::string NormalizeMemberName(const std::string& raw)
        {
            std::string s = raw;
            const std::size_t dot = s.find_last_of('.');
            if (dot != std::string::npos && dot + 1 < s.size())
                s = s.substr(dot + 1);

            const std::size_t us = s.find_last_of('_');
            if (us != std::string::npos && us + 1 < s.size())
            {
                bool allDigit = true;
                for (std::size_t k = us + 1; k < s.size(); ++k)
                {
                    if (!std::isdigit(static_cast<unsigned char>(s[k])))
                    {
                        allDigit = false;
                        break;
                    }
                }
                if (allDigit)
                    s = s.substr(0, us);
            }
            return s;
        }
    }

    /// @copydoc Program::BuildUniformBlocks
    void Program::BuildUniformBlocks()
    {
        GLint numBlocks = 0;
        glGetProgramiv(mProgramAddr, GL_ACTIVE_UNIFORM_BLOCKS, &numBlocks);
        for (GLint i = 0; i < numBlocks; ++i)
        {
            GLint nameLen = 0;
            glGetActiveUniformBlockiv(mProgramAddr, static_cast<GLuint>(i),
                                      GL_UNIFORM_BLOCK_NAME_LENGTH, &nameLen);
            std::string rawName(static_cast<std::size_t>(nameLen > 0 ? nameLen - 1 : 0), '\0');
            GLsizei written = 0;
            glGetActiveUniformBlockName(mProgramAddr, static_cast<GLuint>(i),
                                        nameLen, &written,
                                        rawName.empty() ? nullptr : &rawName[0]);

            GLint dataSize = 0;
            glGetActiveUniformBlockiv(mProgramAddr, static_cast<GLuint>(i),
                                      GL_UNIFORM_BLOCK_DATA_SIZE, &dataSize);

            UniformBlock blk;
            blk.normalizedName = NormalizeBlockName(rawName);
            blk.blockIndex     = static_cast<GLuint>(i);
            blk.bindingPoint   = static_cast<GLuint>(i);   // 블록 인덱스 그대로 binding point (D12).
            blk.dataSize       = dataSize;

            // 셰이더 측 block index <-> binding point 매핑 결속.
            glUniformBlockBinding(mProgramAddr, blk.blockIndex, blk.bindingPoint);

            blk.ubo = UniformBuffer::Create(static_cast<std::size_t>(dataSize));
            if (!blk.ubo)
            {
                // fail-fast (사용자 결정 - InitScheduler 와 동일 정책).
                // src/program 은 spdlog 미사용 (game_deps 미링크). stderr + abort 로 동등.
                std::fprintf(stderr,
                             "[Program::BuildUniformBlocks] UBO 생성 실패 - block '%s', "
                             "dataSize=%d, programAddr=%u\n",
                             blk.normalizedName.c_str(), dataSize, mProgramAddr);
                std::abort();
            }
            // Phase 3 Slice 0 - 블록 멤버 introspection: author 이름 -> {정규화 블록명, std140 offset}.
            //   D-DPP-1(b) 일반 material 업로드의 offset 출처. (refl.json 파일 대신 GL introspection -
            //   Program 자기완결, 새 의존 0. 멤버명은 NormalizeMemberName 으로 author 이름 복원.)
            GLint memberCount = 0;
            glGetActiveUniformBlockiv(mProgramAddr, static_cast<GLuint>(i),
                                      GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &memberCount);
            if (memberCount > 0)
            {
                std::vector<GLint> indices(static_cast<std::size_t>(memberCount));
                glGetActiveUniformBlockiv(mProgramAddr, static_cast<GLuint>(i),
                                          GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, indices.data());
                for (const GLint mi : indices)
                {
                    const GLuint uidx = static_cast<GLuint>(mi);
                    GLint memberOffset = 0;
                    glGetActiveUniformsiv(mProgramAddr, 1, &uidx, GL_UNIFORM_OFFSET, &memberOffset);

                    GLint memberNameLen = 0;
                    glGetActiveUniformsiv(mProgramAddr, 1, &uidx, GL_UNIFORM_NAME_LENGTH, &memberNameLen);
                    std::string memberRaw(static_cast<std::size_t>(memberNameLen > 0 ? memberNameLen - 1 : 0), '\0');
                    GLsizei memberWritten = 0;
                    glGetActiveUniformName(mProgramAddr, uidx, memberNameLen, &memberWritten,
                                           memberRaw.empty() ? nullptr : &memberRaw[0]);

                    mUniformMembers[NormalizeMemberName(memberRaw)] =
                        UniformMember{ blk.normalizedName, static_cast<std::size_t>(memberOffset) };
                }
            }

            mUniformBlocks.push_back(std::move(blk));
        }
    }

    /// @copydoc Program::FindUniformBlock
    const Program::UniformBlock* Program::FindUniformBlock(const std::string& normalizedName) const
    {
        for (const auto& b : mUniformBlocks)
            if (b.normalizedName == normalizedName)
                return &b;
        return nullptr;
    }

    /// @copydoc Program::BindUniformBlocks
    void Program::BindUniformBlocks() const
    {
        for (const auto& b : mUniformBlocks)
            if (b.ubo)
                b.ubo->BindBase(b.bindingPoint);
    }

    /// @copydoc Program::UpdateUniformBlock
    void Program::UpdateUniformBlock(const std::string& normalizedName,
                                     const void* data,
                                     std::size_t bytes,
                                     std::size_t offset) const
    {
        const UniformBlock* b = FindUniformBlock(normalizedName);
        if (b && b->ubo)
            b->ubo->Update(data, bytes, offset);
    }

    /// @copydoc Program::UpdateUniformMember
    void Program::UpdateUniformMember(const std::string& memberName,
                                      const void* data, std::size_t bytes) const
    {
        const auto it = mUniformMembers.find(memberName);
        if (it == mUniformMembers.end())
            return;   // 비-UBO 멤버(sampler/loose 값) - 안전 skip.
        UpdateUniformBlock(it->second.normalizedBlock, data, bytes, it->second.offset);
    }

    /// @copydoc Program::DisownUniformBlock
    void Program::DisownUniformBlock(const std::string& normalizedName)
    {
        for (auto& b : mUniformBlocks)
            if (b.normalizedName == normalizedName)
            {
                // per-program UBO 해제 - BindUniformBlocks 의 if(b.ubo) 가 자동 skip.
                // blockIndex/bindingPoint 유지 -> 외부 owner(LightUboUploader)가 그 point 에 공유 UBO 결속.
                b.ubo.reset();
                return;
            }
    }
}
