/**
 * @file uniform_diagnostics.h
 * @brief uniform warn-once 진단 - program 별 누락/타입불일치 중복 제거.
 *
 * @details
 *  ### 책임
 *  - 누락 uniform(@c NotifyMissing): (program, name) 조합 첫 호출만 @c spdlog::warn, 이후 silent.
 *  - 타입 불일치(@c NotifyTypeMismatch): 동일 중복 제거. @c actual==0 (active 정보 없음) 은 skip.
 *  - program 파괴 시 트래커 정리(@c Invalidate) - 같은 @c GLuint 재발급 시 stale 방지.
 *
 *  ### 비-책임
 *  - [X] uniform 값 setter - @c SJH::Uniforms 자유 함수 family (@c src/program/ 모듈).
 *  - [X] location 캐싱 - 없음 (Phase C). @c Program::GetLocation 이 live @c glGetUniformLocation.
 *
 *  ### 설계 - 왜 static (인스턴스 X)
 *  본 클래스 자체에 *인스턴스 상태가 필요 없음* - program 별 dedup 은 익명 네임스페이스
 *  내부 @c unordered_map 으로. @c SJH::Uniforms 자유 함수가 본 헤더를 include 하지 않아도 되도록
 *  diagnostics 의존성을 @c .cpp 차원으로 가둠 (@c .claude/architecture.md sec.4 PRIVATE link 일관).
 *
 *  ### Lifecycle
 *  @c Program::~Program() 에서 반드시 @c Invalidate(handle) 을 명시 호출해야 함.
 *  누락 시 같은 @c GLuint 재발급 때 stale 트래커 -> 기대 warn 이 silently 묻힘.
 */

#ifndef __SJH_DIAGNOSTICS_UNIFORM_DIAGNOSTICS_H__
#define __SJH_DIAGNOSTICS_UNIFORM_DIAGNOSTICS_H__

#pragma once

#include "GL/gl3w.h"

namespace SJH::Diagnostics
{
    /**
     * @brief uniform warn-once 진단 - 누락/타입불일치를 program 별로 최초 1회만 보고.
     * @details
     *  순수 static 유틸 클래스. 내부 익명 네임스페이스 @c detail 맵이 (program -> 이름 집합) 으로
     *  중복을 제거한다. 인스턴스화 금지(@c GLObjectLog, @c EffekseerDiagnostics 컨벤션과 동일).
     */
    class UniformDiagnostics
    {
    public:
        UniformDiagnostics()                                         = delete;
        UniformDiagnostics(const UniformDiagnostics &)               = delete;
        UniformDiagnostics &operator=(const UniformDiagnostics &)    = delete;

        /// @brief 누락 uniform 최초 1회 보고.
        /// @details (@p program, @p name) 조합이 처음 보고될 때만 @c spdlog::warn. 이후 silent.
        /// @param program 해당 GL 프로그램 핸들.
        /// @param name    누락된 uniform 이름.
        static void NotifyMissing(GLuint program, const char *name);

        /// @brief uniform 타입 불일치 최초 1회 보고.
        /// @details (@p program, @p name) 조합이 처음 불일치 시에만 @c spdlog::warn.
        ///          @p actual == @c 0 (active 정보 없음 - lazy 보강 케이스) 이면 검증 skip.
        /// @param program  해당 GL 프로그램 핸들.
        /// @param name     uniform 이름.
        /// @param expected 호출자(setter)가 기대한 GL 타입 (예: @c GL_FLOAT_MAT4).
        /// @param actual   셰이더에서 실제로 선언된 타입. @c 0 이면 skip.
        static void NotifyTypeMismatch(GLuint program, const char *name,
                                       GLenum expected, GLenum actual);

        /// @brief 해당 program 의 모든 warn-once 트래커 정리.
        /// @details @c Program 소멸자에서 반드시 호출. 누락 시 같은 @c GLuint 재발급 때 stale.
        /// @param program 정리할 GL 프로그램 핸들.
        static void Invalidate(GLuint program);
    };
}

#endif // __SJH_DIAGNOSTICS_UNIFORM_DIAGNOSTICS_H__
