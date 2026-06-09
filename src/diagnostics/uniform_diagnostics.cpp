/**
 * @file uniform_diagnostics.cpp
 * @brief @c UniformDiagnostics 구현 - program 키 기반 글로벌 warn-once 트래커.
 *
 * @details
 *  ### 구현 노트
 *  - 익명 네임스페이스 @c detail - @c warnedMissing / @c warnedTypeMismatch :
 *    `unordered_map<GLuint, unordered_set<string>>` 구조로 (program, name) 중복 제거.
 *  - @c insert().second 패턴 - 집합에 처음 삽입될 때만 @c true 반환 -> 최초 1회만 warn.
 *  - @c Invalidate - @c erase(program) 으로 두 맵에서 모두 제거. @c Program::~Program 짝꿍.
 */

#include "uniform_diagnostics.h"

#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

// 공개 불능시키고 사용하기.
namespace
{
    namespace detail
    {
        std::unordered_map<GLuint, std::unordered_set<std::string>> warnedMissing;
        std::unordered_map<GLuint, std::unordered_set<std::string>> warnedTypeMismatch;
    }
}

namespace SJH::Diagnostics
{
    void UniformDiagnostics::NotifyMissing(GLuint program, const char *name)
    {
        if (detail::warnedMissing[program].insert(name).second)
        {
            spdlog::warn("프로그램 {}에 uniform 누락: '{}'", program, name);
        }
    }

    void UniformDiagnostics::NotifyTypeMismatch(GLuint program, const char *name,
                                                GLenum expected, GLenum actual)
    {
        if (actual == 0)
            return; // active 정보 없음 (lazy 보강 케이스) - 검증 skip
        if (actual == expected)
            return; // 일치 -> no-op

        if (detail::warnedTypeMismatch[program].insert(name).second)
        {
            spdlog::warn("프로그램 {} '{}'의 uniform 타입 불일치: 기대 0x{:x}, 실제 0x{:x}",
                         program, name, expected, actual);
        }
    }

    void UniformDiagnostics::Invalidate(GLuint program)
    {
        detail::warnedMissing.erase(program);
        detail::warnedTypeMismatch.erase(program);
    }
}
