/**
 * @file uniform_cache.cpp
 * @brief @c UniformCache::Build / @c GetLocation / @c GetType 구현.
 *
 * @details
 *  ### 책임
 *  - @c Build - @c glGetActiveUniform 루프로 active uniform 전체 수집 후 @c mEntries 에 저장.
 *  - @c GetLocation / @c GetType - @c mEntries unordered_map 단순 조회 (O(1) 평균).
 *
 *  ### 비-책임
 *  - [X] 비-active uniform 수집 - dead-code elimination 된 uniform 은 @c glGetActiveUniform 결과에 없음.
 *  - [X] 배열 원소 개별 저장 (@c "arr[0]" 등) - @c glGetActiveUniform 은 배열 기저 이름(@c "arr[0]")만
 *    보고하는 드라이버도 있으나 원소 인덱스별 별도 저장은 하지 않음.
 *    @c GetLocation 미캐시 -> 호출자(@c Uniforms setter)가 @c glGetUniformLocation fallback.
 *
 * @note SP1~SP5 시점 @c Program::BuildUniformCache 의 로직을 SP6 에서 본 클래스로 이전.
 *       로직 변경 없음 - 소유 구조만 변경 (@c Program 멤버 unordered_map -> @c UniformCache).
 */
#include "program/uniform_cache.h"
#include "program/program.h"   // Program::GetProgramAddr - Build 가 사용
#include <vector>

namespace SJH
{
    void UniformCache::Build(const Program& prog)
    {
        const GLuint programAddr = prog.GetProgramAddr();
        if (programAddr == 0) return;

        GLint count = 0;
        glGetProgramiv(programAddr, GL_ACTIVE_UNIFORMS, &count);
        if (count <= 0) return;

        GLint maxNameLen = 0;
        glGetProgramiv(programAddr, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLen);
        if (maxNameLen <= 0) return;

        std::vector<char> nameBuf(static_cast<std::size_t>(maxNameLen + 1), '\0');
        for (GLint i = 0; i < count; ++i)
        {
            GLsizei nameSize = 0;
            GLint   size     = 0;
            GLenum  type     = 0;
            glGetActiveUniform(programAddr, static_cast<GLuint>(i),
                               maxNameLen, &nameSize, &size, &type, nameBuf.data());

            const GLint loc = glGetUniformLocation(programAddr, nameBuf.data());
            mEntries.emplace(
                std::string(nameBuf.data(), static_cast<std::size_t>(nameSize)),
                Entry{ loc, type });
        }
    }

    GLint UniformCache::GetLocation(const char* name) const
    {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return -1;
        return it->second.Location;
    }

    GLenum UniformCache::GetType(const char* name) const
    {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return 0;
        return it->second.Type;
    }
}
