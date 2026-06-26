// SJH::Timer::Timer characterization - 현 HEAD 동작 잠금 (spec D3/D8a).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "timer/timer.h"

using Catch::Matchers::WithinAbs;
using SJH::Timer::Timer;

TEST_CASE("Timer: 기본 누적과 진행도", "[timer]")
{
    Timer t(2.0f);
    REQUIRE_THAT(t.GetBaseTime(), WithinAbs(2.0f, 1e-7f));
    REQUIRE_THAT(t.GetProgress(), WithinAbs(0.0f, 1e-7f));
    REQUIRE_FALSE(t.IsTimesUp());

    t.Tick(0.5f); // accel 기본 1.0 -> mPassedTime = 0.5
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.5f, 1e-7f));
    REQUIRE_THAT(t.GetProgress(), WithinAbs(0.25f, 1e-7f)); // 0.5/2.0
    REQUIRE_FALSE(t.IsTimesUp());
}

TEST_CASE("Timer: 가속", "[timer]")
{
    Timer t(2.0f);
    t.SetAcceleration(2.0f);
    t.Tick(0.5f); // 0.5 * 2 = 1.0
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(1.0f, 1e-7f));
    REQUIRE_THAT(t.GetProgress(), WithinAbs(0.5f, 1e-7f));
}

TEST_CASE("Timer: 음수 가속은 0 으로 클램프", "[timer]")
{
    Timer t(2.0f);
    t.SetAcceleration(-3.0f); // -> 0
    t.Tick(1.0f);             // 1.0 * 0 = 0
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.0f, 1e-7f));
}

TEST_CASE("Timer: BASE_TIME 상한 클램프 + IsTimesUp", "[timer]")
{
    Timer t(2.0f);
    t.Tick(10.0f); // 10 > 2 -> clamp 2.0
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(2.0f, 1e-7f));
    REQUIRE_THAT(t.GetProgress(), WithinAbs(1.0f, 1e-7f));
    REQUIRE(t.IsTimesUp());
}

TEST_CASE("Timer: 음수 dt 하한 가드", "[timer]")
{
    Timer t(2.0f);
    t.Tick(-5.0f); // mPassedTime = -5 -> 0 으로 가드
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.0f, 1e-7f));
}

TEST_CASE("Timer: Pause/Resume", "[timer]")
{
    Timer t(2.0f);
    t.Pause();
    REQUIRE(t.IsBlocked());
    t.Tick(1.0f); // blocked -> no-op
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.0f, 1e-7f));
    t.Resume();
    REQUIRE_FALSE(t.IsBlocked());
    t.Tick(1.0f);
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(1.0f, 1e-7f));
}

TEST_CASE("Timer: PollInterval 은 호출당 1회 true, 임계 전진", "[timer]")
{
    Timer t(10.0f);
    t.SetInterval(0.5f); // 0.5/1.0/1.5 모두 정확 이진분수

    t.Tick(0.5f);                    // passed = 0.5
    REQUIRE(t.PollInterval());       // 0.5 >= 0.5 -> true, iext -> 1.0
    REQUIRE_FALSE(t.PollInterval()); // 0.5 >= 1.0 -> false (catch-up 없음)

    t.Tick(0.5f);                    // passed = 1.0
    REQUIRE(t.PollInterval());       // 1.0 >= 1.0 -> true, iext -> 1.5
    REQUIRE_FALSE(t.PollInterval());
}

TEST_CASE("Timer: 인터벌 미설정이면 PollInterval 항상 false", "[timer]")
{
    Timer t(10.0f);
    t.Tick(5.0f);
    REQUIRE_FALSE(t.PollInterval()); // mIntervalTime <= 0
}

TEST_CASE("Timer: Reset", "[timer]")
{
    Timer t(10.0f);
    t.SetInterval(0.5f);
    t.Tick(0.5f);
    t.Tick(0.5f); // passed = 1.0
    t.Reset();
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.0f, 1e-7f));
    REQUIRE_FALSE(t.IsBlocked());
    // Reset 후 인터벌 임계가 초기값으로 복원 -> 다시 0.5 에서 첫 true
    t.Tick(0.5f);
    REQUIRE(t.PollInterval());
}
