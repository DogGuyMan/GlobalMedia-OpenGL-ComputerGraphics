/**
 * @file gl_state_log.cpp
 * @brief @c GLStateLog 구현 - @c Dump + @c EnableAutoOnError.
 *
 * @details
 *  ### 구현 노트
 *  - @c Dump - @c CaptureGLState() 호출 후 @c FieldsToString() 멀티라인을 @c spdlog::info 로 출력.
 *    @p tag 비어있으면 @c "[GLStateLog]", 아니면 @c "[GLStateLog/<tag>]" 프리픽스.
 *  - @c EnableAutoOnError - @c GL_VERSION_4_3 또는 @c GL_KHR_debug 정의 환경에서만
 *    @c glDebugMessageCallback 존재 검사. macOS GL 3.3 은 미지원 ->
 *    @c std::once_flag 로 1회 warn 후 no-op (매 호출 noise 방지).
 */

#include "diagnostics/gl_state_log.h"
#include "diagnostics/gl_state_fields.h"

#include "GL/gl3w.h"
#include <spdlog/spdlog.h>
#include <mutex>

namespace SJH::Diagnostics
{
    void GLStateLog::Dump(std::string_view tag)
    {
        auto fields = CaptureGLState();
        if (!tag.empty()) {
            spdlog::info("[GLStateLog/{}]\n{}", tag, FieldsToString(fields));
        } else {
            spdlog::info("[GLStateLog]\n{}", FieldsToString(fields));
        }
    }

    void GLStateLog::EnableAutoOnError(bool /*enable*/)
    {
#if defined(GL_VERSION_4_3) || defined(GL_KHR_debug)
        if (glDebugMessageCallback != nullptr) {
            // TODO(future): KHR_debug callback 등록. 현재는 macOS 우선 - 미구현.
            // 구현 시 기존 GLDebug::Init과 통합 (architecture.md sec.6 Layer 1).
            spdlog::info("[GLStateLog] EnableAutoOnError: KHR_debug 콜백 등록 (TODO)");
            return;
        }
#endif
        // macOS arm64 GL 3.3 등 KHR_debug 미지원 환경
        // std::call_once로 1회만 warn - 매 호출마다 noise 방지
        static std::once_flag warned;
        std::call_once(warned, []() {
            spdlog::warn("[GLStateLog] EnableAutoOnError: KHR_debug 미지원 환경 - no-op");
        });
    }
}
