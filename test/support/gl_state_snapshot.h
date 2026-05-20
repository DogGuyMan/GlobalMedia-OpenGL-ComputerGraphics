/**
 * @file gl_state_snapshot.h
 * @brief 테스트 측 RAII 친화 wrapper + Diff (Task 6).
 *
 * @details
 *  ### 책임
 *  - `GLStateSnapshot::Capture()` — production `CaptureGLState()` 위임.
 *  - `ToString()` — `FieldsToString` 위임 (DRY).
 *  - `Diff(before, after)` — *변화한 필드만* 출력. Catch2 `INFO()` 와 결합 시 회귀 가시화.
 *
 *  ### 비-책임
 *  - State 캡처 자체 — `CaptureGLState()`에 위임.
 *  - 형식 결정 — `FieldsToString`에 위임.
 *
 *  ### 비대칭 정책 (spec 2.3)
 *  - enum 필드: `SymbolicName` 적용 (예: "GL_LESS")
 *  - GLuint 핸들: raw 정수 (식별자에 의미 없음)
 *  - VAO=0인 경우 element_buffer 변화에 주석 (사용자 EBO incident memory 반영)
 *
 *  ### Catch2 통합 패턴
 *  @code
 *  auto before = SJH::test::GLStateSnapshot::Capture();
 *  buf->Bind();
 *  auto after = SJH::test::GLStateSnapshot::Capture();
 *  INFO(SJH::test::Diff(before, after));   // PASS면 출력 X, FAIL이면 자동 출력
 *  REQUIRE(after.fields.element_buffer == buf->Get());
 *  @endcode
 *
 * @see [doc/testplan/2026-05-07-gl-state-and-test-quality-design.md](../../doc/testplan/2026-05-07-gl-state-and-test-quality-design.md) §2.3
 */

#ifndef __SJH_TEST_GL_STATE_SNAPSHOT_H__
#define __SJH_TEST_GL_STATE_SNAPSHOT_H__

#pragma once

#include <GL/glcorearb.h>
#include "diagnostics/gl_state_fields.h"
#include <string>

namespace SJH::test
{
    /// production GLStateFields의 *얇은 wrapper* — Catch2 친화 메서드 추가.
    class GLStateSnapshot
    {
    public:
        SJH::Diagnostics::GLStateFields fields;

        /// `CaptureGLState()` 호출 — caller가 GL context 보장.
        static GLStateSnapshot Capture();

        /// 사람이 읽는 다중라인. INFO()로 던지기 좋음. `FieldsToString`에 위임.
        std::string ToString() const;
    };

    /// 변화한 필드만 출력. 변화 0건이면 "(no GL state change)\n".
    /// enum 필드: SymbolicName 적용. GLuint 핸들: raw 정수 (의도된 비대칭).
    /// VAO=0 인 경우 element_buffer 라인에 주석 자동 포함 (변화 발생 시에 한해).
    /// audit 트랙 A: `attribute_layouts[i]` 변화도 출력.
    std::string Diff(const GLStateSnapshot& before, const GLStateSnapshot& after);
}

#endif // __SJH_TEST_GL_STATE_SNAPSHOT_H__
