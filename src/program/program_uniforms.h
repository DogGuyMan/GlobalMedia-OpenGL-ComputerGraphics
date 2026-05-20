/**
 * @file program_uniforms.h
 * @brief @c SJH::Uniforms 자유 함수 family — Program 의 uniform setter (시그니처 보존).
 *
 * @details
 *  ### 디자인 동기 — 왜 멤버 함수가 아니라 자유 함수인가
 *  -# **책임 분리 (SRP)** — @c Program 의 본질은 *GL program 의 lifetime + link 상태*.
 *     uniform 값 설정은 *그 위에 얹는 별개 관심사* — Program 의 책임이 아님.
 *  -# **확장성 (OCP)** — 새 uniform 타입 (예: @c Mat3, @c IVec2) 추가 시 @c Program 헤더는
 *     *그대로*, 본 namespace 에만 한 줄 추가. 멤버로 두면 매번 Program API 가 비대해짐.
 *  -# **C# extension method 와 동등한 효과** — 클래스 내부를 건드리지 않고 *외부에서*
 *     동작을 덧붙이는 패턴. C++ 에선 *자유 함수 + ADL* 가 그 자연스러운 형태.
 *
 *  ### 캐시 위치 — Program *내부* 멤버 (SP2 완료)
 *  - 캐시는 @c Program::mUniformCache (private) — @c Program::GetLocation / @c GetType 으로 공개.
 *  - 자유 함수들은 @c Program::GetLocation (const read-only) 경유 — TU-local static 캐시 제거됨.
 *  - **friend 선언 불필요** — @c GetLocation / @c GetType / @c GetProgramAddr 이 public.
 *
 *  ### Lifetime — Program 멤버 자동 소멸
 *  - 배열 원소 (@c "arr[3]") 같은 비-canonical 이름 → GetLocation 이 -1 반환 →
 *    setter 가 fallback 으로 @c glGetUniformLocation 직접 호출 (캐시 mutation 없음, POLA).
 *
 *  ### 책임
 *  - **uniform 값 setter** — 6종 (Mat4 / Vec4 / Vec3 / Vec2 / Float / Int).
 *  - **진단 위임** — 누락 / 타입 불일치 시 @c Diagnostics::UniformDiagnostics 가 첫 호출 1회 warn.
 *
 *  ### 사용
 *  @code
 *    auto prog = Program::Create({vs, fs});      // mUniformCache 자동 빌드
 *    Uniforms::SetMat4(*prog, "uModel", data);
 *  @endcode
 */

#ifndef __SJH_PROGRAM_UNIFORMS_H__
#define __SJH_PROGRAM_UNIFORMS_H__

#include "GL/gl3w.h"
#include <vmath.h>

namespace SJH
{
    class Program;   // forward — 본 헤더는 Program 의 정의에 의존하지 않음 (의도된 decoupling).
    class DirLight;   // forward — 광원 struct 정의는 object/light.h. .cpp 만 include.
    class PointLight;
    class SpotLight;

    /// @note (SP2 완료) 캐시는 Program 의 멤버로 이전됨. 본 namespace 의 자유 함수
    ///       family 는 *시그니처 보존* — 내부 구현이 @c prog.GetLocation 을 경유.
    ///       @c BuildCache / @c Forget 자유 함수는 폐기 (멤버 흡수).
    namespace Uniforms
    {
        // --- setter family — prog.GetLocation 경유. 미존재 이름은 첫 호출 1회 warn ---
        void SetMat4 (const Program &prog, const char *name, const vmath::mat4& m4); ///< GL_FLOAT_MAT4
        void SetVec4 (const Program &prog, const char *name, const vmath::vec4& v4);   ///< GL_FLOAT_VEC4
        void SetVec3 (const Program &prog, const char *name, const vmath::vec3& v3);   ///< GL_FLOAT_VEC3
        void SetVec2 (const Program &prog, const char *name, const vmath::vec2& v2);   ///< GL_FLOAT_VEC2
        void SetFloat(const Program &prog, const char *name, const float& v);           ///< GL_FLOAT
        void SetInt  (const Program &prog, const char *name, const int& v);             ///< GL_INT / GL_SAMPLER_*

        /// @brief 캐시된 location 반환. 미존재면 -1 (+ 첫 호출 시 diagnostics 가 warn).
        GLint Get(const Program &prog, const char *name);

        // --- 광원 struct -> uniform block 일괄 전송 helpers ----------------------------
        // 각 helper 는 `<prefix>.<field>` 형태로 셰이더 struct 멤버에 1:1 대응. 내부적으로
        // SetVec3/SetFloat 를 호출 — 누락/타입불일치 진단도 자동 적용.

        /// @brief DirLight -> `<prefix>.{direction,ambient,diffuse,specular}` 4 uniform 전송.
        /// @param worldDir 라이트 노드의 월드 전방 벡터 (방향). @c prefix 예: @c "dirLight".
        void SetDirLight(const Program &prog, const char *prefix,
                         const DirLight &light, const vmath::vec3 &worldDir);

        /// @brief PointLight -> `<prefix>.{position,attenuation,ambient,diffuse,specular}` 5 uniform 전송.
        /// @param worldPos 라이트 노드의 월드 위치. @c Distance -> (Kc,Kl,Kq) 는 helper 내부 도출.
        void SetPointLight(const Program &prog, const char *prefix,
                           const PointLight &light, const vmath::vec3 &worldPos);

        /// @brief SpotLight -> 8 uniform 전송
        ///        (`<prefix>.{position,direction,cutoff,outerCutoff,attenuation,ambient,diffuse,specular}`).
        /// @param worldPos 라이트 노드의 월드 위치. @param worldDir 라이트 노드의 월드 전방.
        /// @details CPU 는 degree 보관 / 셰이더는 cosine 비교 — 송신 시점에 @c cosf(vmath::radians(...)) 변환.
        void SetSpotLight(const Program &prog, const char *prefix,
                          const SpotLight &light, const vmath::vec3 &worldPos, const vmath::vec3 &worldDir);
    }
}

#endif // __SJH_PROGRAM_UNIFORMS_H__
