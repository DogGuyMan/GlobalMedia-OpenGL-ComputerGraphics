/**
 * @file gl_state_log.cpp
 * @brief @c GLStateLog 구현 — Dump + EnableAutoOnError.
 */

#include "diagnostics/gl_state_log.h"
#include "diagnostics/gl_state_fields.h"

#include "log_util.h"

#include <GL/gl3w.h>
#include <mutex>

namespace SJH::Diagnostics
{
    void GLStateLog::Dump(std::string_view tag)
    {
        auto fields = CaptureGLState();
        if (!tag.empty()) {
            Log::Info("[GLStateLog/{}]\n{}", tag, FieldsToString(fields));
        } else {
            Log::Info("[GLStateLog]\n{}", FieldsToString(fields));
        }
    }

    void GLStateLog::EnableAutoOnError(bool /*enable*/)
    {
#if defined(GL_VERSION_4_3) || defined(GL_KHR_debug)
        if (glDebugMessageCallback != nullptr) {
            // TODO(future): KHR_debug callback 등록. 현재는 macOS 우선 — 미구현.
            // 구현 시 기존 GLDebug::Init과 통합 (architecture.md §6 Layer 1).
            Log::Info("[GLStateLog] EnableAutoOnError: KHR_debug 콜백 등록 (TODO)");
            return;
        }
#endif
        // macOS arm64 GL 3.3 등 KHR_debug 미지원 환경
        // std::call_once로 1회만 warn — 매 호출마다 noise 방지
        static std::once_flag warned;
        std::call_once(warned, []() {
            Log::Warn("[GLStateLog] EnableAutoOnError: KHR_debug 미지원 환경 — no-op");
        });
    }
}
