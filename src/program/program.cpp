/**
 * @file program.cpp
 * @brief Program 팩토리 + RAII 소멸자 + TryLink 구현.
 *
 * @details
 *  ### 책임
 *  - @c Create / @c CreateWithVSFS 팩토리 - 셰이더 attach + link + @c BuildUniformBlocks (UBO introspect).
 *  - @c TryLink - @c glCreateProgram / @c glAttachShader / @c glLinkProgram 순서 캡슐화.
 *  - @c ~Program - @c glDeleteProgram + @c UniformDiagnostics::Invalidate 연계 해제.
 *  - @c BuildUniformBlocks - UBO 블록(의미명 binding point) + 멤버 offset 맵 introspect (Phase 2/3).
 *
 *  ### 비-책임
 *  - [X] uniform 값 설정 - @c program_uniforms.cpp (@c SJH::Uniforms namespace) 에서 담당.
 *  - [X] sampler location 캐시 - 없음 (Phase C - @c GetLocation 이 live @c glGetUniformLocation).
 *
 *  ### 컴파일 타임 검증 (@c static_assert)
 *  - RAII 의미론: 복사/이동 생성/대입 모두 @c = delete 확인.
 *  - @c GetLocation 의 @c const 호출 가능성 확인.
 */
#include "program/program.h"
#include "diagnostics/gl_log.h"
#include "diagnostics/uniform_diagnostics.h"
#include <algorithm>
#include <cctype>      // Phase 2 T3 - NormalizeBlockName 의 isdigit 판정.
#include <cstdio>      // Phase 2 T3 - UBO 생성 실패 시 stderr 출력 (fail-fast 로그).
#include <cstdlib>     // Phase 2 T3 - std::abort (UBO 생성 실패 정통 fail-fast).
#include <stdexcept>   // F-2 fail-fast - Release 빌드 std::runtime_error.
#include <string>      // F-2 fail-fast - 실패 메시지 조립.
#include <type_traits>
#include <utility>     // Phase 2 T3 - std::move (UniformBlock 보관).
#include <vector>      // Phase 2 T3 - BuildUniformBlocks 의 이름 버퍼.

namespace
{
    /// @brief F-2 프로그램 링크 실패 = fail-fast (silent nullptr 금지).
    /// @details 정책: Debug(NDEBUG 미정의) = 메시지 출력 후 abort, Release = runtime_error throw.
    ///          이미 BuildUniformBlocks 의 UBO 생성 실패가 동일 패턴(abort) - link 실패도 통일.
    ///          진단 로그(CheckProgramLink)는 이미 출력된 상태 - 여기선 tag 를 한 번 더 명시.
    /// @param what 프로그램 식별자 (예: "vs=foo.vert fs=foo.frag").
    [[noreturn]] void FailFastProgram(const std::string& what)
    {
        const std::string msg =
            "[SJH::Program] 프로그램 링크 실패 (fail-fast): " + what
            + " - 상세 InfoLog 는 직전 진단 로그 참조.";
#ifndef NDEBUG
        std::fprintf(stderr, "%s\n", msg.c_str());
        std::abort();
#else
        throw std::runtime_error(msg);
#endif
    }
}

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

// (SP2) GetLocation 이 const 호출 가능한지 컴파일 타임 검증. (Phase C - GetType 제거됨)
static_assert(std::is_invocable_v<decltype(&SJH::Program::GetLocation), const SJH::Program&, const char*>,
              "Program::GetLocation must be const-callable (pure query)");

namespace SJH
{
    ProgramUPtr Program::Create(const std::vector<ShaderPtr> &shaders)
    {
        auto program = ProgramUPtr(new Program());
        if (!program->TryLink(shaders))
            FailFastProgram("Program::Create (셰이더 " + std::to_string(shaders.size()) + "개)"); // F-2

        // Phase 2 T3 - Slang UBO 블록 introspection + UBO 객체 생성 (비-UBO 셰이더는 빈 벡터).
        //   Phase C (D-DPP-5) - 구 UniformCache eager build 제거. sampler location 은 GetLocation(live).
        program->BuildUniformBlocks();
        return program;
    }

    ProgramUPtr Program::CreateWithVSFS(const std::string &vertShaderFilename,
                                        const std::string &fragShaderFilename)
    {
        // CreateFromFile 은 컴파일/로드 실패 시 fail-fast(noreturn) - 여기 도달하면 vs/fs 는 항상 유효.
        ShaderPtr vs = Shader::CreateFromFile(vertShaderFilename, GL_VERTEX_SHADER);
        ShaderPtr fs = Shader::CreateFromFile(fragShaderFilename, GL_FRAGMENT_SHADER);
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

        /**
         * @brief 블록 *의미명* -> 전역 고정 binding point (Phase C 암전 fix, 2026-06-21).
         * @details glBindBufferBase 의 binding point 는 *전역* GL state 다. 구 `bindingPoint = blockIndex`
         *          (program-local 인덱스)는 program 마다 0,1,2.. 를 재사용해 **다른 program 의 블록이 같은
         *          전역 point 를 덮어쓰는 충돌**을 일으켰다 (예: phong LightBlock 이 GL 열거상 point 1 -
         *          simple 불릿의 DrawBlock(point 1)이 공유 LightBlock 을 덮어써 phong 암전).
         *          의미명으로 전역 고정하면 모든 program 의 같은 의미 블록이 같은 point, 다른 의미는 다른 point
         *          -> 충돌 불가. (Unity per-semantic constant buffer slot / Unreal uniform buffer slot 정통.)
         * @param nextUnknown 미지 블록명용 순차 카운터 (예약 0~3 위 4부터). 현재 셰이더엔 미사용이나 방어적.
         */
        GLuint SemanticBindingPoint(const std::string& normalizedName, GLuint& nextUnknown)
        {
            if (normalizedName == "FrameBlock")    return 0;
            if (normalizedName == "DrawBlock")     return 1;
            if (normalizedName == "MaterialBlock") return 2;
            if (normalizedName == "LightBlock")    return 3;
            return nextUnknown++;   // 알려지지 않은 블록 - 예약 0~3 위로 순차 배정.
        }
    }

    /// @copydoc Program::BuildUniformBlocks
    void Program::BuildUniformBlocks()
    {
        GLint numBlocks = 0;
        glGetProgramiv(mProgramAddr, GL_ACTIVE_UNIFORM_BLOCKS, &numBlocks);
        GLuint nextUnknownBinding = 4;   // 의미명 외 블록용 (예약 0~3 위). 현재 셰이더엔 미발생.
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
            // Phase C 암전 fix - binding point 를 의미명으로 전역 고정 (구 `= i` 는 program-local 충돌).
            blk.bindingPoint   = SemanticBindingPoint(blk.normalizedName, nextUnknownBinding);
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
