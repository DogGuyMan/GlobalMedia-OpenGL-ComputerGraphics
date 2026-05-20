/**
 * @file test_uniform_diagnostics.cpp
 * @brief @c SJH::Diagnostics::UniformDiagnostics 의 warn-once 트래커 *행동 단언* 검증.
 *
 * @details
 *  - 기존 SUCCEED-only 스모크 테스트(R2 smell)를 SpdlogCapture 기반 행동 단언으로 교체 (Task 4).
 *  - 본 클래스는 spdlog 로그만 발생 (GL 호출 X) — GL context fixture 불필요.
 *  - `detail::warnedMissing/warnedTypeMismatch`는 *프로세스 전역 static* — 테스트 간 상태 누설을
 *    피하기 위해 각 케이스 시작 시 `Invalidate(handle)` 호출로 클린업.
 *
 * @see [doc/testplan/2026-05-07-gl-state-and-test-quality-design.md](../doc/testplan/2026-05-07-gl-state-and-test-quality-design.md) §5.2
 */

#include <catch2/catch_test_macros.hpp>

#include "diagnostics/uniform_diagnostics.h"
#include "support/spdlog_capture.h"

TEST_CASE("UniformDiagnostics::NotifyMissing 다중 호출 — warn-once 트래커",
          "[diagnostics][uniform]")
{
    using SJH::Diagnostics::UniformDiagnostics;

    // 안전 시작 — 다른 테스트가 같은 핸들을 썼을 수 있으므로 트래커 클린업
    UniformDiagnostics::Invalidate(42);
    UniformDiagnostics::Invalidate(99);

    SJH::test::SpdlogCapture cap;

    // (program=42, name="uMissingA") 첫 호출 -> warn 출력
    UniformDiagnostics::NotifyMissing(42, "uMissingA");
    REQUIRE(cap.Contains("uMissingA"));
    REQUIRE(cap.Contains("42"));        // 메시지에 program 핸들도 포함
    auto firstSize = cap.Lines().size();

    // 두 번째 동일 (program, name) — warn-once 로 silent (출력 길이 변화 없음)
    UniformDiagnostics::NotifyMissing(42, "uMissingA");
    REQUIRE(cap.Lines().size() == firstSize);

    // 같은 program, 다른 name — 새 warn (출력 길이 증가)
    UniformDiagnostics::NotifyMissing(42, "uMissingB");
    REQUIRE(cap.Lines().size() > firstSize);
    REQUIRE(cap.Contains("uMissingB"));

    // 다른 program, 같은 name — 다른 키, warn 발생
    auto sizeBeforeOtherProg = cap.Lines().size();
    UniformDiagnostics::NotifyMissing(99, "uMissingA");
    REQUIRE(cap.Lines().size() > sizeBeforeOtherProg);
    REQUIRE(cap.Contains("99"));        // 새 program 핸들이 메시지에 출력됨

    // 정리: 후속 테스트가 stale 트래커 상속 안 하게
    UniformDiagnostics::Invalidate(42);
    UniformDiagnostics::Invalidate(99);
}

TEST_CASE("UniformDiagnostics::NotifyTypeMismatch 시나리오 — warn 조건 행동 단언",
          "[diagnostics][uniform]")
{
    using SJH::Diagnostics::UniformDiagnostics;
    constexpr GLuint kProgram = 142; // 다른 테스트와 충돌 회피 (전역 static)

    UniformDiagnostics::Invalidate(kProgram);
    SJH::test::SpdlogCapture cap;

    // 불일치 — 첫 호출 warn
    UniformDiagnostics::NotifyTypeMismatch(kProgram, "uMat", GL_FLOAT_MAT4, GL_FLOAT_VEC4);
    REQUIRE(cap.Contains("uMat"));
    auto afterFirst = cap.Lines().size();

    // 같은 (prog, name) 재호출 — warn-once silent
    UniformDiagnostics::NotifyTypeMismatch(kProgram, "uMat", GL_FLOAT_MAT4, GL_FLOAT_VEC4);
    REQUIRE(cap.Lines().size() == afterFirst);

    // 일치 — silent (warn 안 일어남)
    UniformDiagnostics::NotifyTypeMismatch(kProgram, "uOk", GL_FLOAT, GL_FLOAT);
    REQUIRE(cap.Lines().size() == afterFirst);
    REQUIRE_FALSE(cap.Contains("uOk"));

    // actual==0 (active 정보 없음, lazy 보강 케이스) — silent (검증 skip)
    UniformDiagnostics::NotifyTypeMismatch(kProgram, "uUnknown", GL_FLOAT, 0);
    REQUIRE(cap.Lines().size() == afterFirst);
    REQUIRE_FALSE(cap.Contains("uUnknown"));

    UniformDiagnostics::Invalidate(kProgram);
}

TEST_CASE("UniformDiagnostics::Invalidate 멱등 + Invalidate 후 warn 재발 가능",
          "[diagnostics][uniform]")
{
    using SJH::Diagnostics::UniformDiagnostics;
    constexpr GLuint kProgram = 207;

    UniformDiagnostics::Invalidate(kProgram);
    SJH::test::SpdlogCapture cap;

    // 첫 NotifyMissing — warn 발생
    UniformDiagnostics::NotifyMissing(kProgram, "uX");
    REQUIRE(cap.Contains("uX"));
    auto sizeBeforeInvalidate = cap.Lines().size();

    // Invalidate — 트래커 정리, 추가 출력 없음
    UniformDiagnostics::Invalidate(kProgram);
    UniformDiagnostics::Invalidate(kProgram);  // 멱등 (idempotent)
    UniformDiagnostics::Invalidate(99999);     // 미존재 program — 안전 (crash X, 출력 없음)
    REQUIRE(cap.Lines().size() == sizeBeforeInvalidate);

    // Invalidate 후 같은 (prog, name) NotifyMissing — 트래커 클리어됐으므로 다시 warn
    UniformDiagnostics::NotifyMissing(kProgram, "uX");
    REQUIRE(cap.Lines().size() > sizeBeforeInvalidate);

    UniformDiagnostics::Invalidate(kProgram);
}
