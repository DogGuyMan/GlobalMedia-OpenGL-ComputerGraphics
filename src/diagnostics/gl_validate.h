/**
 * @file gl_validate.h
 * @brief Mesh <-> Shader 정합성 진단 카테고리 A-G - 명시적 호출형.
 *
 * @details
 *  ### 책임
 *  - **Cat A** - CPU 측 EBO 인덱스 OOB / degenerate / 중복 삼각형 검사 (GL 무관).
 *  - **Cat B** - VS active attribute <-> 현재 VAO enabled attribute layout 정합.
 *  - **Cat C** - program active uniform 값이 default-0 인지 (CPU 미송신 의심).
 *  - **Cat D** - sampler2D uniform <-> @c glActiveTexture 바인딩 정합.
 *  - **Cat E** - @c glGetError 폴링 + (코드, tag) 조합 프로세스 단위 rate-limit.
 *  - **Cat F** - attached shader / program info log 캡처 (driver warning 포함).
 *  - **Cat G** - GL viewport <-> 기대 렌더 타깃 크기 정합 (HiDPI seed / FBO 크기 왜곡 탐지).
 *
 *  ### 비-책임
 *  - [X] 비즈니스 로직(@c mesh.cpp / 셰이더 / @c context.cpp) 수정 없음.
 *  - [X] 자동 호출 없음 - caller 가 @c Mesh::Init / @c Context::Init 등에서 명시 호출.
 *  - [X] 매 프레임 호출 금지 (@c CaptureGLError 만 예외 - rate-limited 이라 허용).
 *
 *  ### 설계 근거
 *  @c GLDebug 는 *per-call low-level* 에러 검사(예: @c glBufferData 직후). 본 모듈은
 *  *Mesh <-> Shader contract* 영역으로 호출 단위가 program/mesh - 두 모듈은 직교적.
 *
 * @see `doc/testplan/2026-05-09-gl-validate-design.md`
 * @note 챕터는 `namespace diag = SJH::Diagnostics;` 별칭 후 `diag::GLValidate::RunFullSweep(...)` 사용.
 */

#ifndef __SJH_DIAGNOSTICS_GL_VALIDATE_H__
#define __SJH_DIAGNOSTICS_GL_VALIDATE_H__

#pragma once

#include "GL/gl3w.h"
#include <cstddef>
#include <vector>

namespace SJH::Diagnostics::GLValidate
{
    /// @brief **Cat A** - CPU 측 EBO 인덱스 OOB / degenerate / 중복 삼각형 검사.
    /// @details GL context 불필요 - 순수 CPU 검사. 삼각형 단위(3 인덱스마다) 순회.
    ///          OOB(@p vertexCount 초과) / degenerate(두 인덱스 동일) / 중복(정렬된 트리플 집합) 탐지.
    /// @param indices     EBO 에 업로드된 인덱스 vector (CPU 측 원본).
    /// @param vertexCount VBO 에 들어 있는 정점 개수.
    /// @param tag         로그 식별자 (예: 메시 이름).
    /// @return 발견된 위반 개수 (0 = clean).
    size_t CheckIndices(const std::vector<uint32_t>& indices,
                        size_t vertexCount,
                        const char* tag);

    /// @brief **Cat B** - VS active attribute <-> 현재 VAO enabled attribute layout 정합.
    /// @details @c glGetActiveAttrib + @c glGetAttribLocation 으로 VS 선언 attribute 를 열거한 뒤
    ///          현재 바인딩된 VAO 의 enable 여부 + size(컴포넌트 수) 를 비교.
    ///          VAO 가 enable 했지만 VS 가 안 쓰는 location 은 @c spdlog::info 로만 표시(violation 제외).
    /// @pre @p program 이 @c Use() 된 상태, VAO 가 @c Bind() 된 상태.
    /// @param program 검사 대상 GL 프로그램 핸들.
    /// @param tag     로그 식별자.
    /// @return 발견된 위반 개수 (0 = clean).
    size_t CheckAttribLayout(GLuint program, const char* tag);

    /// @brief **Cat C** - active uniform 중 값이 default-0 인 항목 보고.
    /// @details sampler2D 는 unit 0 이 합법이므로 Cat D 에서 별도 처리, 본 함수는 skip.
    ///          보수적 진단 - 의도적 0 도 false positive 로 잡힐 수 있음 (보조 도구).
    /// @param program 검사 대상 GL 프로그램 핸들.
    /// @param tag     로그 식별자.
    /// @return 의심 항목 개수 (0 = 모두 비-zero 또는 sampler).
    size_t CheckUniformCoverage(GLuint program, const char* tag);

    /// @brief **Cat D** - sampler2D uniform 이 가리키는 texture unit 에 텍스처 바인딩 여부 확인.
    /// @details sampler type(@c GL_SAMPLER_2D / @c GL_SAMPLER_CUBE / @c GL_SAMPLER_3D) 만 대상.
    ///          unit 값 조회(@c glGetUniformiv) -> 해당 unit 활성화 -> @c GL_TEXTURE_BINDING_* 확인.
    ///          active_texture 를 저장/복원해 부수효과 최소화.
    /// @param program 검사 대상 GL 프로그램 핸들.
    /// @param tag     로그 식별자.
    /// @return 텍스처 미바인딩 sampler 개수 (0 = clean).
    size_t CheckSamplerBindings(GLuint program, const char* tag);

    /// @brief **Cat E** - @c glGetError() 폴링 + (에러코드, tag) 조합 프로세스 단위 1회 보고.
    /// @details rate-limit: 같은 (코드, tag) 조합은 첫 발생만 @c spdlog::warn - log spam 방지.
    ///          에러 큐에 복수 에러가 있으면 모두 drain 해 보고.
    /// @param tag 로그 식별자.
    /// @return @c true = @c GL_NO_ERROR (clean), @c false = 에러 있었음.
    bool CaptureGLError(const char* tag);

    /// @brief **Cat F** - attached shader + program 의 info log 캡처.
    /// @details log 가 비어있지 않으면 @c spdlog::info 출력.
    ///          "error" / "warning" 키워드(대소문자 무시) 포함 시 @c spdlog::warn 으로 격상.
    /// @param program 검사 대상 GL 프로그램 핸들.
    /// @param tag     로그 식별자.
    void DumpShaderInfoLogs(GLuint program, const char* tag);

    /// @brief **Cat G** - 현재 GL viewport 가 기대 렌더 타깃 크기와 일치하는지 검사.
    /// @details HiDPI/Retina 에서 viewport 를 *논리* 픽셀 크기로 잘못 잡거나,
    ///          FBO <-> viewport 가 어긋나 풀스크린 블릿이 기울거나 확대되는 버그 탐지.
    ///          불일치 시 @c spdlog::warn, 일치 시 @c spdlog::info - mismatch 없으면 조용.
    /// @param expectedWidth  기대 너비 - FBO 패스면 FBO 텍스처 너비,
    ///                       화면 패스/seed 검증이면 @c glfwGetFramebufferSize 결과.
    /// @param expectedHeight 기대 높이.
    /// @param tag            로그 식별자.
    /// @return 0 = 일치, 1 = 불일치.
    size_t CheckViewport(int expectedWidth, int expectedHeight, const char* tag);

    /// @brief 통합 진단 - Cat A + B + C + D + F 를 순서대로 실행.
    /// @details @c Mesh::Init / @c Context::Init 직후 1회 호출 권장.
    ///          Cat E 는 별도(@c CaptureGLError) - 본 함수 내부에서 실행하지 않음.
    /// @param program     검사 대상 GL 프로그램 핸들.
    /// @param indices     Cat A 용 CPU 측 인덱스 vector.
    /// @param vertexCount Cat A 용 정점 개수.
    /// @param tag         로그 식별자.
    /// @return 0 = 모두 clean, > 0 = 위반 합계.
    size_t RunFullSweep(GLuint program,
                        const std::vector<uint32_t>& indices,
                        size_t vertexCount,
                        const char* tag);

    /// @brief Cat E rate-limit 캐시 리셋.
    /// @details 테스트 격리 또는 매 프레임 시작 시 호출 -> *프레임당* rate-limit 동작.
    void ResetRateLimitCache();
}

#endif // __SJH_DIAGNOSTICS_GL_VALIDATE_H__
