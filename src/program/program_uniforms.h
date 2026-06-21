/**
 * @file program_uniforms.h
 * @brief @c SJH::Uniforms 자유 함수 family - Program 의 uniform setter.
 *
 * @details
 *  ### 책임
 *  - uniform 값 setter 6종: @c SetMat4 / @c SetVec4 / @c SetVec3 / @c SetVec2 / @c SetFloat / @c SetInt.
 *  - 누락 uniform 시 @c Diagnostics::UniformDiagnostics::NotifyMissing 첫 호출 1회 warn.
 *
 *  ### 비-책임
 *  - [X] @c glUseProgram 바인딩 관리 - @c DeviceContext 가 담당.
 *  - [X] uniform location 캐시 - 없음 (Phase C). @c Program::GetLocation 이 live @c glGetUniformLocation.
 *
 *  ### Phase C 이후 잔존 사용처 (대부분 UBO 로 이전됨)
 *  값 uniform 은 전부 UBO 화돼 본 family 의 즉시-GL setter 는 *잔여 loose* 만 다룬다
 *  (주: sampler unit 설정 @c SetInt - @c ScreenQuadStage 의 uScene 등). 일반 material 값은
 *  @c MeshPassProcessor 가 UBO 멤버(@c Program::UpdateUniformMember)로 송신.
 *
 *  ### 디자인 동기 - 왜 멤버 함수가 아니라 자유 함수인가
 *  -# **책임 분리 (SRP)** - @c Program 의 본질은 *GL program 의 lifetime + link 상태*.
 *     uniform 값 설정은 *그 위에 얹는 별개 관심사* - Program 의 책임이 아님.
 *  -# **확장성 (OCP)** - 새 uniform 타입 (예: @c Mat3, @c IVec2) 추가 시 @c Program 헤더는
 *     *그대로*, 본 namespace 에만 한 줄 추가. 멤버로 두면 매번 Program API 가 비대해짐.
 *  -# **C# extension method 와 동등한 효과** - 클래스 내부를 건드리지 않고 *외부에서*
 *     동작을 덧붙이는 패턴. C++ 에선 *자유 함수 + ADL* 가 그 자연스러운 형태.
 *
 *  ### DeviceContext 와의 책임 경계 (SP-RenderFacadeBoundary)
 *  본 namespace 는 *shader uniform 상태* 의 단일 진입점. @c DeviceContext (pipeline
 *  state facade) **를 의도적으로 우회**한다. 같은 호출 사이트에서 두 책임이 섞이는
 *  형태 (예: SceneRenderer / MeshPassProcessor) 는 *디자인 의도된 분리* - 안티패턴
 *  아님. 근거:
 *    - **OCP** - 새 uniform 타입 추가 시 @c DeviceContext 헤더 변동 0.
 *    - **진단 가시성** - @c glUniform* 직전에 캐시 fallback (@c glGetUniformLocation
 *      직접 호출, 캐시 mutation 없음) 이 실행 - facade 가 중간에 끼면 이 fallback
 *      흐름이 가려져 누락 uniform 진단 (warn-once) 추적 곤란.
 *    - **bound state 분리** - @c DeviceContext 는 *어떤 program 이 bound 인가* 만
 *      알면 충분. 그 program 에 *무슨 값을 넣는가* 는 본 namespace 의 자율 영역.
 *
 *  호출자 가이드 - 어떤 경로로 무엇을:
 *  | 의도 | 경로 |
 *  |---|---|
 *  | program 활성화 / VAO/Tex/RT 바인딩 / draw | @c DeviceContext |
 *  | uniform 값 설정 | 본 namespace (자유 함수) |
 *  | uniform location 조회 | @c Program::GetLocation 직접 |
 *
 *  ### location 조회 (Phase C - 캐시 제거)
 *  - 자유 함수들은 @c Program::GetLocation (live @c glGetUniformLocation) 경유 - 멤버 캐시 없음.
 *  - **friend 선언 불필요** - @c GetLocation / @c GetProgramAddr 이 public.
 *
 *  ### 사용 예
 *  @code
 *    auto prog = Program::Create({vs, fs});
 *    Uniforms::SetInt(*prog, "uScene", 0);       // sampler unit (잔여 loose 송신)
 *  @endcode
 *
 * @note 본 헤더는 @c Program 을 forward declaration 만 사용 - @c program.h include 불필요 (의도된 decoupling).
 */

#ifndef __SJH_PROGRAM_UNIFORMS_H__
#define __SJH_PROGRAM_UNIFORMS_H__

#include "GL/gl3w.h"
#include <glm/glm.hpp>

namespace SJH
{
    class Program;   // forward - 본 헤더는 Program 의 정의에 의존하지 않음 (의도된 decoupling).

    /**
     * @brief GL 프로그램 uniform 값 setter 자유 함수 모음.
     * @details
     *  본 namespace 의 자유 함수 family 는 @c prog.GetLocation (live 조회) 을 경유해 즉시 @c glUniform*.
     *  Phase C 이후 값 uniform 은 UBO 로 이전되어 잔존 호출은 주로 sampler unit 설정 등 loose 송신.
     */
    namespace Uniforms
    {
        // --- setter family - prog.GetLocation 경유. 미존재 이름은 첫 호출 1회 warn ---

        /// @brief @c GL_FLOAT_MAT4 uniform 전송.
        /// @param prog 대상 프로그램. @param name 셰이더 uniform 이름. @param m4 4x4 행렬.
        void SetMat4 (const Program &prog, const char *name, const glm::mat4& m4);

        /// @brief @c GL_FLOAT_VEC4 uniform 전송.
        /// @param v4 4-컴포넌트 벡터.
        void SetVec4 (const Program &prog, const char *name, const glm::vec4& v4);

        /// @brief @c GL_FLOAT_VEC3 uniform 전송.
        /// @param v3 3-컴포넌트 벡터 (위치/색상/방향 등).
        void SetVec3 (const Program &prog, const char *name, const glm::vec3& v3);

        /// @brief @c GL_FLOAT_VEC2 uniform 전송.
        /// @param v2 2-컴포넌트 벡터 (UV 오프셋 등).
        void SetVec2 (const Program &prog, const char *name, const glm::vec2& v2);

        /// @brief @c GL_FLOAT uniform 전송.
        /// @param v float 스칼라 (시간/강도/감쇠 계수 등).
        void SetFloat(const Program &prog, const char *name, const float& v);

        /// @brief @c GL_INT / @c GL_SAMPLER_* uniform 전송.
        /// @details @c GL_SAMPLER_2D 등 샘플러 uniform 도 @c glUniform1i 로 전송 - 타입 검증 생략 (SP1 정책).
        /// @param v 정수 또는 텍스처 유닛 번호.
        void SetInt  (const Program &prog, const char *name, const int& v);

        /// @brief uniform location 조회 - @c Program::GetLocation (live @c glGetUniformLocation).
        /// @details 미존재면 @c Diagnostics::UniformDiagnostics::NotifyMissing (첫 호출 1회 warn).
        /// @return 찾은 location, 없으면 @c -1.
        GLint GetLocation(const Program &prog, const char *name);

        // (광원 struct -> uniform 헬퍼 SetDirLight/SetPointLight/SetSpotLight 는 D6 으로 light_ubo_uploader
        //  로 이주 후 Phase C 에서 LightBlock UBO 전환으로 제거됨 - loose lighting 경로 소멸.)
    }
}

#endif // __SJH_PROGRAM_UNIFORMS_H__
