/**
 * @file uniform_diagnostics.h
 * @brief uniform 관련 warn-once 진단 — *프로세스 단일* static 인터페이스.
 *
 * @details
 *  ### 책임
 *  - 누락 uniform 의 첫 호출만 warn, 이후 silent — program 단위 deduplication.
 *  - 타입 불일치 첫 호출만 warn (@c actual==0 즉 active 정보 없음 경우 skip).
 *  - program 파괴 시 해당 키의 트래커 정리 (@c Invalidate).
 *
 *  ### 설계 — 왜 static (인스턴스 X)
 *  - 본 클래스 자체에 *인스턴스 상태가 필요 없음* — program 별 dedup 은 내부 map 으로.
 *  - @c SJH::Uniforms 자유 함수 family 가 본 헤더를 *include 하지 않아도* 되게 함 ->
 *    diagnostics 의존성을 cpp 차원으로 가둠
 *    ([architecture.md §4](../../.claude/architecture.md) PRIVATE link 일관).
 *  - 기존 @c GLObjectLog::CheckExpectedUniforms 도 동일 패턴 (static + 내부 program 키 map).
 *
 *  ### 비-책임
 *  - ❌ uniform 값 setter — @c SJH::Uniforms 자유 함수 family 에서 (@c program/ 모듈).
 *  - ❌ location 캐싱 — @c SJH::Uniforms 의 TU-local 정적 캐시 (@c program_uniforms.cpp) 에서.
 *
 *  ### Lifecycle
 *  - 호출자(@c SJH::Program::~Program())가 파괴 시 @c Invalidate(mProgramAddr) 명시 호출 필요.
 *    안 부르면 같은 @c GLuint 가 재발급될 때 stale 트래커 -> 기대 warn 이 silently 묻힐 수 있음.
 *  - 짝꿍: @c Uniforms::Forget — uniform location 캐시 정리. 두 함수 모두 destructor 에서 호출되어야 일관.
 *  @see SJH::Uniforms::Forget
 */

#ifndef __SJH_DIAGNOSTICS_UNIFORM_DIAGNOSTICS_H__
#define __SJH_DIAGNOSTICS_UNIFORM_DIAGNOSTICS_H__

#pragma once

#include "GL/gl3w.h"

namespace SJH::Diagnostics
{
    class UniformDiagnostics
    {
    public:
        UniformDiagnostics()                                         = delete;
        UniformDiagnostics(const UniformDiagnostics &)               = delete;
        UniformDiagnostics &operator=(const UniformDiagnostics &)    = delete;

        /// @brief 누락 uniform 보고. (program, name) 조합 첫 호출만 spdlog::warn.
        static void NotifyMissing(GLuint program, const char *name);

        /// @brief 타입 불일치 보고. (program, name) 조합 첫 호출만 spdlog::warn.
        /// @param expected 호출자(setter)가 *기대* 한 GL 타입 (예: @c GL_FLOAT_MAT4).
        /// @param actual   셰이더에서 *실제로* 선언된 타입. @c 0 이면 active 정보 없음 -> 검증 skip.
        static void NotifyTypeMismatch(GLuint program, const char *name,
                                       GLenum expected, GLenum actual);

        /// @brief 해당 program 의 모든 warn-once 트래커 정리.
        /// @note @c Program 소멸자에서 호출. 안 부르면 같은 GLuint 재발급 시 stale.
        static void Invalidate(GLuint program);
    };
}

#endif // __SJH_DIAGNOSTICS_UNIFORM_DIAGNOSTICS_H__
