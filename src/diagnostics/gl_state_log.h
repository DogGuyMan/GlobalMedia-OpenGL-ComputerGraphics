/**
 * @file gl_state_log.h
 * @brief Production 측 GL 상태 한 줄 덤프 + KHR_debug 콜백 통합 (옵션).
 *
 * @details
 *  ### 책임
 *  - @c Dump(tag) - 현재 GL 상태를 @c spdlog::info 로 한 번 출력. 디버깅 시 위치 식별용.
 *  - @c EnableAutoOnError(bool) - KHR_debug 콜백에서 @c GL_DEBUG_SEVERITY_HIGH 발생 시
 *    자동 @c Dump 활성화 시도. macOS GL 3.3 은 KHR_debug 미지원 ->
 *    @c std::call_once warn 후 no-op.
 *
 *  ### 비-책임
 *  - [X] 테스트 측 RAII Snapshot / Diff - @c test/support/gl_state_snapshot.h.
 *  - [X] 상태 캡처 자체 - @c gl_state_fields.h 의 @c CaptureGLState 에 위임.
 *
 *  ### 호출 시점
 *  - **개발 중 디버깅**: 의심스러운 draw 직전 @c Dump("after_camera_setup") 한 줄.
 *  - **매 프레임 호출 금지** - @c glGet* 가 GPU stall 유발.
 *
 * @see `doc/testplan/2026-05-07-gl-state-and-test-quality-design.md` sec.2.2
 */

#ifndef __SJH_DIAGNOSTICS_GL_STATE_LOG_H__
#define __SJH_DIAGNOSTICS_GL_STATE_LOG_H__

#pragma once

#include <string_view>

namespace SJH::Diagnostics
{
    /**
     * @brief Production 측 GL 상태 덤프 + KHR_debug 자동 트리거 유틸.
     * @details
     *  내부에서 @c CaptureGLState() -> @c FieldsToString() -> @c spdlog::info 파이프라인으로
     *  현재 GL 상태를 한 번에 출력. 모든 메서드가 @c static - 인스턴스화 불필요.
     */
    class GLStateLog
    {
    public:
        /// @brief 현재 GL 상태를 @c spdlog::info 로 한 번 출력.
        /// @details 내부적으로 @c CaptureGLState() -> @c FieldsToString() 파이프라인.
        ///          매 프레임 호출 금지 - @c glGet* 계열이 GPU stall 유발.
        /// @param tag 출력 prefix - 디버깅 시 위치 식별용 (비우면 @c "[GLStateLog]" 고정).
        static void Dump(std::string_view tag = {});

        /// @brief @c GL_DEBUG_SEVERITY_HIGH 발생 시 자동 @c Dump 활성화 시도.
        /// @details macOS GL 3.3 은 KHR_debug 미지원 -> @c std::call_once 로 1회 warn 후 no-op.
        ///          KHR_debug 지원 환경에서 @c glDebugMessageCallback 등록 예정 (현재 TODO).
        /// @param enable @c true 면 활성화 시도. @c false 는 비활성화 (미구현).
        static void EnableAutoOnError(bool enable);
    };
}

#endif // __SJH_DIAGNOSTICS_GL_STATE_LOG_H__
