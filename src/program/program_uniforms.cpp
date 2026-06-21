/**
 * @file program_uniforms.cpp
 * @brief @c SJH::Uniforms 자유 함수 family 구현 - setter 6종 + 광원 헬퍼 3종.
 *
 * @details
 *  ### 책임
 *  - setter (@c SetMat4 / @c SetVec4 / @c SetVec3 / @c SetVec2 / @c SetFloat / @c SetInt) 구현.
 *  - @c GetLocation 편의 함수 - @c Program::GetLocation (live) + 미존재 시 NotifyMissing warn.
 *
 *  ### 비-책임
 *  - [X] uniform location 캐시 - 없음 (Phase C). @c Program::GetLocation 이 live @c glGetUniformLocation.
 *  - [X] @c glUseProgram 바인딩 - @c DeviceContext 전담. 본 TU 는 bound state 를 가정만 함.
 *
 *  ### location 경로 (Phase C - 캐시 제거)
 *  - 각 setter 는 @c prog.GetLocation(name) (live @c glGetUniformLocation) 조회 후 @c glUniform*.
 *  - @c -1 이면 @c NotifyMissing 후 no-op. (타입불일치 진단은 Phase C 에서 제거 - UniformCache GetType 의존이었음.)
 *
 *  ### Program 과의 의존성
 *  - 본 TU 만 @c program/program.h 를 include - public 멤버만 호출.
 *  - 헤더 (@c program_uniforms.h) 는 forward declaration 만 사용 - Program 정의 의존 없음.
 *
 * @note 광원 struct -> uniform 헬퍼 3종은 D6 으로 light_ubo_uploader.cpp 이주 후
 *       Phase C 에서 LightBlock UBO 전환으로 제거됨 (loose lighting 경로 소멸).
 */

#include "program/program.h"
#include "program/program_uniforms.h"
#include "diagnostics/uniform_diagnostics.h"
#include "GL/gl3w.h"        // glGetUniformLocation, glUniform*, GL_FALSE 등 직접 include (strict includes)
#include <glm/gtc/type_ptr.hpp> // glm::value_ptr - glm 은 vmath 와 달리 암시적 float* 변환 없음

namespace SJH::Uniforms
{
    // === 진입점들 - prog.GetLocation (read-only) + -1 fallback + 진단. friend 권한 불필요. ===

    void SetMat4(const Program &prog, const char *name, const glm::mat4& m4)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0)
        {
            // Fallback: 비-active uniform (예: 배열 원소 `arr[3]`).
            // 캐시 miss 외부 fallback - Program 의 캐시는 mutation 하지 않음 (POLA).
            loc = glGetUniformLocation(pid, name);
        }
        if (loc < 0)
        {
            Diagnostics::UniformDiagnostics::NotifyMissing(pid, name);
            return;
        }
        // Phase C (D-DPP-5) - GetType 기반 타입불일치 진단 제거 (UniformCache 삭제). location 검증만 유지.
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m4));
    }

    void SetVec4(const Program &prog, const char *name, const glm::vec4& v4)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        glUniform4fv(loc, 1, glm::value_ptr(v4));
    }

    void SetVec3(const Program &prog, const char *name, const glm::vec3& v3)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        glUniform3fv(loc, 1, glm::value_ptr(v3));
    }

    void SetVec2(const Program &prog, const char *name, const glm::vec2& v2)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        glUniform2fv(loc, 1, glm::value_ptr(v2));
    }

    void SetFloat(const Program &prog, const char *name, const float& v)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        glUniform1f(loc, v);
    }

    void SetInt(const Program &prog, const char *name, const int& v)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        // GL_INT / GL_SAMPLER_2D 등 모두 glUniform1i 라 타입 검증 생략 (SP1 정책 유지).
        glUniform1i(loc, v);
    }

    GLint GetLocation(const Program &prog, const char *name)
    {
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(prog.GetProgramAddr(), name);
        if (loc < 0)
            Diagnostics::UniformDiagnostics::NotifyMissing(prog.GetProgramAddr(), name);
        return loc;
    }
}
