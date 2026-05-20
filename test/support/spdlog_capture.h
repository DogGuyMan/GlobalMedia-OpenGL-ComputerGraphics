/**
 * @file spdlog_capture.h
 * @brief 테스트용 spdlog 출력 캡처 RAII 헬퍼.
 *
 * @details
 *  - 인스턴스 생성 시 default spdlog logger를 ostringstream sink로 교체.
 *  - 소멸 시 원래 logger 복원.
 *  - 테스트가 `Lines()` / `Contains()`로 캡처된 로그 출력을 단언 가능.
 *
 *  ### 동기 — 왜 필요한가
 *  - 기존 `test_uniform_diagnostics.cpp`의 `SUCCEED("...crash 없음")` 같은 스모크 테스트는
 *    *행동 회귀 감지력이 없다*. spdlog 출력을 캡처해서 *어떤 메시지가 나왔는가* /
 *    *몇 번 나왔는가*를 단언하는 행동 단언으로 교체 필요.
 *  - GL context 불필요 — 순수 spdlog 의존.
 *
 *  ### 사용 예
 *  @code
 *  TEST_CASE("...") {
 *      SJH::test::SpdlogCapture cap;
 *      SJH::Diagnostics::UniformDiagnostics::NotifyMissing(42, "uMissingA");
 *      REQUIRE(cap.Contains("uMissingA"));
 *      auto firstSize = cap.Lines().size();
 *      // 두 번째 호출은 warn-once로 silent
 *      SJH::Diagnostics::UniformDiagnostics::NotifyMissing(42, "uMissingA");
 *      REQUIRE(cap.Lines().size() == firstSize);
 *  }
 *  @endcode
 *
 *  ### 컨벤션
 *  - 단일 스레드 가정 (Catch2 v3 default).
 *  - 복사 금지 — RAII 소유권 한 곳에서.
 *
 * @see [doc/testplan/2026-05-07-gl-state-and-test-quality-design.md](../../doc/testplan/2026-05-07-gl-state-and-test-quality-design.md) §2.4
 */

#ifndef __SJH_TEST_SPDLOG_CAPTURE_H__
#define __SJH_TEST_SPDLOG_CAPTURE_H__

#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace spdlog { class logger; }

namespace SJH::test
{
    /// RAII로 default spdlog logger를 ostringstream sink로 교체.
    /// 소멸 시 원래 logger 복원. 단일 스레드 가정 (Catch2 v3 default).
    class SpdlogCapture
    {
    public:
        /// @brief 생성 시점의 default logger를 보존하고, ostringstream sink logger로 교체.
        SpdlogCapture();

        /// @brief 소멸 시 원래 default logger 복원.
        ~SpdlogCapture();

        SpdlogCapture(const SpdlogCapture&)            = delete;
        SpdlogCapture& operator=(const SpdlogCapture&) = delete;

        /// @brief 캡처된 모든 출력 (newline 포함).
        std::string Lines() const;

        /// @brief substring 포함 여부 — 가장 흔한 단언 패턴.
        bool Contains(std::string_view s) const;

    private:
        std::shared_ptr<spdlog::logger>     mPrev;
        std::shared_ptr<std::ostringstream> mStream;
    };
}

#endif // __SJH_TEST_SPDLOG_CAPTURE_H__
