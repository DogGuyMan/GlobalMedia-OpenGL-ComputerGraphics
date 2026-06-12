/**
 * @file program_uniforms.cpp
 * @brief @c SJH::Uniforms 자유 함수 family 구현 - setter 6종 + 광원 헬퍼 3종.
 *
 * @details
 *  ### 책임
 *  - setter (@c SetMat4 / @c SetVec4 / @c SetVec3 / @c SetVec2 / @c SetFloat / @c SetInt) 구현.
 *  - 광원 struct -> uniform block 일괄 전송 (@c SetDirLight / @c SetPointLight / @c SetSpotLight).
 *  - @c GetLocation 편의 함수 - 캐시 우선, 미존재 시 @c glGetUniformLocation fallback + warn.
 *
 *  ### 비-책임
 *  - [X] uniform location 캐시 빌드/삽입 - @c UniformCache::Build (@c Program::Create 내부) 전담.
 *  - [X] @c glUseProgram 바인딩 - @c DeviceContext 전담. 본 TU 는 bound state 를 가정만 함.
 *
 *  ### 캐시 경로 (SP2 이후 / SP6 UniformCache 분리 후 동일)
 *  - 각 setter 는 @c prog.GetLocation(name) 으로 캐시를 read-only 조회.
 *  - @c -1 반환 시 @c glGetUniformLocation fallback (배열 원소 등 비-canonical 이름 대응).
 *  - TU-local static @c sCacheRegistry 제거됨 - Program 소멸 시 멤버가 자동 파괴.
 *
 *  ### Program 과의 의존성
 *  - 본 TU 만 @c program/program.h 를 include - public 멤버만 호출.
 *  - 헤더 (@c program_uniforms.h) 는 forward declaration 만 사용 - Program 정의 의존 없음.
 *
 * @note 광원 헬퍼의 셰이더 struct 필드 이름 접미사는 @c common/constants.h 의 @c Const::SHADER_PROPERTIE_* 상수 사용.
 */

#include "program/program.h"
#include "program/program_uniforms.h"
#include "diagnostics/uniform_diagnostics.h"
#include "object/light.h"   // DirLight/PointLight/SpotLight + GetAttenuationCoeff (헤더는 forward decl 만)
#include "common/constants.h"
#include "GL/gl3w.h"        // glGetUniformLocation, glUniform*, GL_FALSE 등 직접 include (strict includes)

#include <cmath>            // cosf - SpotLight degree->cosine 변환
#include <string>

namespace SJH::Uniforms
{
    // === 진입점들 - prog.GetLocation (read-only) + -1 fallback + 진단. friend 권한 불필요. ===

    void SetMat4(const Program &prog, const char *name, const vmath::mat4& m4)
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
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT_MAT4, prog.GetType(name));
        glUniformMatrix4fv(loc, 1, GL_FALSE, (const float*)m4);
    }

    void SetVec4(const Program &prog, const char *name, const vmath::vec4& v4)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT_VEC4, prog.GetType(name));
        glUniform4fv(loc, 1, (const float*)v4);
    }

    void SetVec3(const Program &prog, const char *name, const vmath::vec3& v3)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT_VEC3, prog.GetType(name));
        glUniform3fv(loc, 1, (const float*)v3);
    }

    void SetVec2(const Program &prog, const char *name, const vmath::vec2& v2)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT_VEC2, prog.GetType(name));
        glUniform2fv(loc, 1, (const float*)v2);
    }

    void SetFloat(const Program &prog, const char *name, const float& v)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT, prog.GetType(name));
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

    // === 광원 struct -> uniform block 일괄 전송 helpers ==========================
    // 책임 분리: 셰이더 struct 멤버 이름과의 *문자열 결합* 만 본 TU 가 담당, 실제
    // GL 호출은 SetVec3/SetFloat 가 재사용 - 캐시/진단/타입체크 경로 그대로 통과.

    void SetDirLight(const Program &prog, const char *prefix,
                     const DirLight &light, const vmath::vec3 &worldDir)
    {
        const std::string base = prefix;
        SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIRECTION).c_str(), worldDir);
        SetVec3(prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(),   light.Ambient);
        SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(),   light.Diffuse);
        SetVec3(prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(),  light.Specular);
    }

    void SetPointLight(const Program &prog, const char *prefix,
                       const PointLight &light, const vmath::vec3 &worldPos)
    {
        const std::string base = prefix;
        SetVec3(prog, (base + Const::SHADER_PROPERTIE_POSITION).c_str(),    worldPos);
        SetVec3(prog, (base + Const::SHADER_PROPERTIE_ATTENUATION).c_str(), GetAttenuationCoeff(light.Distance));
        SetVec3(prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(),     light.Ambient);
        SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(),     light.Diffuse);
        SetVec3(prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(),    light.Specular);
    }

    void SetSpotLight(const Program &prog, const char *prefix,
                      const SpotLight &light, const vmath::vec3 &worldPos, const vmath::vec3 &worldDir)
    {
        const std::string base = prefix;
        SetVec3 (prog, (base + Const::SHADER_PROPERTIE_POSITION).c_str(),     worldPos);
        SetVec3 (prog, (base + Const::SHADER_PROPERTIE_DIRECTION).c_str(),    worldDir);
        // CPU 는 degree, 셰이더는 cosine - 송신 시점에 변환 (struct 정의 시 의도된 분업).
        SetFloat(prog, (base + Const::SHADER_PROPERTIE_CUTOFF).c_str(),       cosf(vmath::radians(light.CutoffAngleDeg)));
        SetFloat(prog, (base + Const::SHADER_PROPERTIE_OUTER_CUTOFF).c_str(), cosf(vmath::radians(light.OuterCutoffAngleDeg)));
        SetVec3 (prog, (base + Const::SHADER_PROPERTIE_ATTENUATION).c_str(),  GetAttenuationCoeff(light.Distance));
        SetVec3 (prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(),      light.Ambient);
        SetVec3 (prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(),      light.Diffuse);
        SetVec3 (prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(),     light.Specular);
    }
}
