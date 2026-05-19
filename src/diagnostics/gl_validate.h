/**
 * @file gl_validate.h
 * @brief Mesh <-> Shader 정합성 진단 (Cat A-G) — 호출 시점 명시적.
 *
 * @details
 *  ### 책임
 *  - Cat A: CPU측 EBO indices 검증 (GL 무관)
 *  - Cat B: VS active attribute <-> VAO enabled attribute layout
 *  - Cat C: program active uniform 의 값이 default-0 인지 (CPU 미송신 의심)
 *  - Cat D: sampler2D uniform <-> glActiveTexture 바인딩 정합
 *  - Cat E: glGetError 폴링 + rate limit
 *  - Cat F: shader/program info log 캡처 (driver warning 포함)
 *  - Cat G: GL viewport <-> 기대 렌더 타깃 크기 정합 (HiDPI seed / 풀스크린 패스 왜곡)
 *
 *  ### 비-책임
 *  - 본 코드 (mesh.cpp / lighting.fs / context.cpp) 비즈니스 로직 수정 X
 *  - 자동 호출 X — caller가 명시 호출 (Mesh::Init, Context::Init 등)
 *  - 매 프레임 호출 금지 (CaptureGLError 만 예외 — rate-limited)
 *
 *  ### 설계 근거
 *  - 기존 `Diagnostics::GLDebug` 는 *per-call low-level* 에러 검사 (e.g. glBufferData 직후)
 *  - 본 모듈은 *Mesh <-> Shader contract* 영역 — 호출 단위가 program/mesh
 *  - 두 모듈은 *직교*적 — 통합 X, 둘 다 caller가 명시 호출
 *
 * @see [doc/testplan/2026-05-09-gl-validate-design.md](../../doc/testplan/2026-05-09-gl-validate-design.md)
 * @see [doc/inst.md](../../doc/inst.md) — 원본 프롬프트
 */

#ifndef __SJH_DIAGNOSTICS_GL_VALIDATE_H__
#define __SJH_DIAGNOSTICS_GL_VALIDATE_H__

#pragma once

#include "GL/gl3w.h"
#include <cstddef>
#include <vector>

namespace SJH::Diagnostics::GLValidate
{
    /// **Cat A** — EBO 인덱스 OOB / degenerate triangle 검사.
    /// @param indices     EBO에 업로드된 인덱스 vector (CPU 측 원본)
    /// @param vertexCount VBO에 들어 있는 정점 개수
    /// @param tag         로그 식별자 (예: 메시 이름)
    /// @return            발견된 위반 개수 (0 = clean)
    /// @note GL context 불필요 — 순수 CPU 검사.
    size_t CheckIndices(const std::vector<uint32_t>& indices,
                        size_t vertexCount,
                        const char* tag);

    /// **Cat B** — VS active attribute <-> 현재 VAO enabled attribute 정합.
    /// @pre  program 이 Use() 된 상태, VAO 가 Bind() 된 상태.
    /// @note program ID 만 인자로 — VAO는 *암묵적*으로 *현재 바인딩된 것* 사용.
    size_t CheckAttribLayout(GLuint program, const char* tag);

    /// **Cat C** — declared active uniform 중 *값이 default-0* 인 항목 보고.
    ///        sampler2D 는 unit 0이 합법이라 분기.
    /// @note 보수적 진단 — false positive 가능 (의도적 0). 셰이더 contract 검증의 보조 도구.
    size_t CheckUniformCoverage(GLuint program, const char* tag);

    /// **Cat D** — sampler2D uniform 각각이 가리키는 texture unit 에 바인딩 있는지.
    size_t CheckSamplerBindings(GLuint program, const char* tag);

    /// **Cat E** — glGetError() 호출 + 에러 enum 이름으로 변환해 보고.
    ///        같은 (코드 + tag) 조합은 *프로세스 단위 1회만* 보고 (rate limit — log spam 방지).
    /// @return true = clean (GL_NO_ERROR), false = 에러 있었음.
    bool CaptureGLError(const char* tag);

    /// **Cat F** — attached shader 와 program의 info log 캡처.
    ///        log 가 비어있지 않으면 spdlog::info (또는 "error"/"warning" 키워드 포함 시 warn).
    void DumpShaderInfoLogs(GLuint program, const char* tag);

    /// **Cat G** — 현재 GL viewport 가 기대 렌더 타깃 크기와 일치하는지 검사.
    ///        HiDPI/Retina 에서 viewport 를 *논리* 크기로 잘못 잡거나, FBO <-> viewport 가
    ///        어긋나 풀스크린 블릿이 기울거나 확대되는 버그를 탐지.
    /// @param expectedWidth   기대 너비 — FBO 패스면 FBO 텍스처 너비,
    ///                        화면 패스/초기 seed 검증이면 실제 프레임버퍼(윈도우) 너비.
    /// @param expectedHeight  기대 높이.
    /// @return 0 = 일치, 1 = 불일치.
    /// @note 순수 GL — caller 가 ground truth(예: glfwGetFramebufferSize 결과)를 인자로 전달.
    ///       불일치 시에만 warn, 일치 시 info — 매 프레임 호출해도 mismatch 없으면 조용.
    size_t CheckViewport(int expectedWidth, int expectedHeight, const char* tag);

    /// 통합 진단 — A·B·C·D·F 모두 실행. Init 직후 1회 호출 권장.
    /// @return 0 = 모두 clean, > 0 = 위반 합계 (E 제외 — E는 본 함수 호출 시 1회 실행)
    size_t RunFullSweep(GLuint program,
                        const std::vector<uint32_t>& indices,
                        size_t vertexCount,
                        const char* tag);

    /// (선택) rate limit cache 리셋 — 테스트나 새 frame 시작 시 호출.
    /// 매 frame 시작 시 호출하면 *프레임당* rate limit 으로 동작.
    void ResetRateLimitCache();
}

#endif // __SJH_DIAGNOSTICS_GL_VALIDATE_H__
