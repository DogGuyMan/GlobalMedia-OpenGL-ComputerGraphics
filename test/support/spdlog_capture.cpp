/**
 * @file spdlog_capture.cpp
 * @brief @c SJH::test::SpdlogCapture 구현.
 *
 * @details
 *  - 생성자: default logger를 보존 + 새로 만든 ostringstream sink logger로 교체.
 *  - 소멸자: 원래 default logger 복원.
 *  - sink pattern은 `[%l] %v` — 시간/스레드 ID 등 비결정적 필드 제거 (테스트 결정성).
 *  - level은 trace — 모든 레벨 캡처 (debug/info/warn/error 다 잡음).
 */

#include "support/spdlog_capture.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/ostream_sink.h>

namespace SJH::test
{
    SpdlogCapture::SpdlogCapture()
        : mStream(std::make_shared<std::ostringstream>())
    {
        // 1. 현재 default logger 보존 (소멸 시 복원용)
        mPrev = spdlog::default_logger();

        // 2. ostringstream sink — 출력을 mStream에 누적
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(*mStream);

        // 3. 결정적 출력 패턴 — 시간/스레드 제외 (테스트 byte-equal 비교 가능)
        sink->set_pattern("[%l] %v");

        // 4. 새 logger 생성 + sink 연결 + 모든 level 캡처
        auto logger = std::make_shared<spdlog::logger>("test_capture", sink);
        logger->set_level(spdlog::level::trace);

        // 5. default 교체 — 이후 spdlog::info(), spdlog::warn() 등 모든 free-function 호출이
        //    이 logger로 향함
        spdlog::set_default_logger(logger);
    }

    SpdlogCapture::~SpdlogCapture()
    {
        spdlog::set_default_logger(mPrev);
    }

    std::string SpdlogCapture::Lines() const
    {
        return mStream->str();
    }

    bool SpdlogCapture::Contains(std::string_view s) const
    {
        const auto& full = mStream->str();
        return full.find(s) != std::string::npos;
    }
}
