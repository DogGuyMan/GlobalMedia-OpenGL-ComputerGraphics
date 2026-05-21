/**
 * @file uniform_cache.cpp
 * @brief UniformCache::Build / GetLocation / GetType 구현.
 * @details SP1~SP5 시점 Program::BuildUniformCache 의 로직을 그대로 이전.
 */
#include "program/uniform_cache.h"
#include "program/program.h"   // Program::GetProgramAddr — Build 가 사용
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
