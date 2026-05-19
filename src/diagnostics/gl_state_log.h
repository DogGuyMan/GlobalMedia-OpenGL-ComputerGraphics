/**
 * @file gl_state_log.h
 * @brief Production 측 GL 상태 한 줄 덤프 + KHR_debug 콜백 통합 (옵션).
 *
 * @details
 *  ### 책임 (Task 5)
 *  - `Dump(tag)` — 현재 GL 상태를 spdlog::info로 한 번 출력. 디버깅 시 위치 식별용.
 *  - `EnableAutoOnError(bool)` — KHR_debug 콜백에서 GL_DEBUG_SEVERITY_HIGH 발생 시
 *    자동 Dump 활성화. macOS GL 3.3은 KHR_debug 미지원 -> std::call_once warn 후 no-op.
 *
 *  ### 비-책임
 *  - 테스트 측 RAII / Diff — 그건 `test/support/gl_state_snapshot.h` (Task 6).
 *  - State 캡처 자체 — `gl_state_fields.h`의 `CaptureGLState`에 위임.
 *
 *  ### 호출 시점
 *  - **개발 중 디버깅**: 의심스러운 draw 직전 `Dump("after_camera_setup")` 한 줄.
 *  - **매 프레임 호출 금지** — `glGet*`이 GPU stall 유발.
 *
 * @see `doc/testplan/2026-05-07-gl-state-and-test-quality-design.md` §2.2
 */

#ifndef __SJH_DIAGNOSTICS_GL_STATE_LOG_H__
#define __SJH_DIAGNOSTICS_GL_STATE_LOG_H__

#pragma once

#include <string_view>

namespace SJH::Diagnostics
{
    class GLStateLog
    {
    public:
        /// 현재 GL 상태 한 번 덤프 (spdlog::info). 매 프레임 호출 금지.
        /// @param tag 출력 prefix — 디버깅 시 위치 식별용
        static void Dump(std::string_view tag = {});

        /// KHR_debug 콜백에서 GL_DEBUG_SEVERITY_HIGH 발생 시 자동 Dump 활성화.
        /// macOS GL 3.3은 KHR_debug 미지원 -> std::call_once warn 후 no-op.
        /// @param enable true면 활성화 시도, false면 비활성화 (TODO: 비활성화는 미구현)
        static void EnableAutoOnError(bool enable);
    };
}

#endif // __SJH_DIAGNOSTICS_GL_STATE_LOG_H__
