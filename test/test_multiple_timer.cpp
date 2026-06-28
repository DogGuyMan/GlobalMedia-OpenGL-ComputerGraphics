// SJH::Timer::MultipleTimer characterization - 다중 트랙 등록/일괄 틱/조회 계약 잠금.
// 단일 Timer 동작은 test_timer.cpp 가 커버 - 여기서는 MultipleTimer 의 컨테이너 책임만.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "timer/multiple_timer.h"

using Catch::Matchers::WithinAbs;
using SJH::Timer::MultipleTimer;
using SJH::Timer::Timer;

TEST_CASE("MultipleTimer: 2 트랙 Register 후 Update 가 각각 독립 진행", "[multiple_timer]")
{
    MultipleTimer mt;
    Timer *attack = mt.Register("player.attack", Timer(1.0f));
    Timer *iframe = mt.Register("life.iframe", Timer(2.0f));
    REQUIRE(mt.Count() == 2);

    mt.Update(0.5f); // 모든 등록 타이머를 일괄 Tick(0.5)
    REQUIRE_THAT(attack->GetPassedTime(), WithinAbs(0.5f, 1e-7f));
    REQUIRE_THAT(iframe->GetPassedTime(), WithinAbs(0.5f, 1e-7f));
    // 진행도는 base 가 다르므로 독립적
    REQUIRE_THAT(attack->GetProgress(), WithinAbs(0.5f, 1e-7f));  // 0.5/1.0
    REQUIRE_THAT(iframe->GetProgress(), WithinAbs(0.25f, 1e-7f)); // 0.5/2.0

    mt.Update(0.5f); // 누적 1.0
    REQUIRE(attack->IsTimesUp());      // 1.0 >= 1.0
    REQUIRE_FALSE(iframe->IsTimesUp()); // 1.0 < 2.0
}

TEST_CASE("MultipleTimer: baseTime 편의 오버로드 등록", "[multiple_timer]")
{
    MultipleTimer mt;
    Timer *t = mt.Register("foo", 4.0f); // Timer(4.0f) 위임
    REQUIRE_THAT(t->GetBaseTime(), WithinAbs(4.0f, 1e-7f));
}

TEST_CASE("MultipleTimer: Unregister 후 Tick 영향 없음 + Count 감소", "[multiple_timer]")
{
    MultipleTimer mt;
    Timer *attack = mt.Register("player.attack", Timer(1.0f));
    mt.Register("life.iframe", Timer(2.0f));
    REQUIRE(mt.Count() == 2);

    // attack 의 누적값을 살아있는 동안 확인
    mt.Update(0.5f);
    REQUIRE_THAT(attack->GetPassedTime(), WithinAbs(0.5f, 1e-7f));

    mt.Unregister("life.iframe"); // 제거 - 이후 Tick 미적용
    REQUIRE(mt.Count() == 1);
    REQUIRE_FALSE(mt.Has("life.iframe"));
    REQUIRE(mt.Find("life.iframe") == nullptr);

    mt.Update(0.5f); // attack 만 Tick (누적 1.0)
    REQUIRE_THAT(attack->GetPassedTime(), WithinAbs(1.0f, 1e-7f));
}

TEST_CASE("MultipleTimer: Find 로 등록 키 핸들 조회", "[multiple_timer]")
{
    MultipleTimer mt;
    Timer *registered = mt.Register("player.attack", Timer(1.0f));
    REQUIRE(mt.Has("player.attack"));
    REQUIRE(mt.Find("player.attack") == registered); // 동일 포인터
    REQUIRE(mt.Find("nope") == nullptr);             // 미존재
}

TEST_CASE("MultipleTimer: Clear 는 전부 제거", "[multiple_timer]")
{
    MultipleTimer mt;
    mt.Register("a", Timer(1.0f));
    mt.Register("b", Timer(1.0f));
    REQUIRE(mt.Count() == 2);
    mt.Clear();
    REQUIRE(mt.Count() == 0);
    REQUIRE_FALSE(mt.Has("a"));
}

TEST_CASE("MultipleTimer: SetEnabled(false) 면 Update 가 Tick 스킵", "[multiple_timer]")
{
    MultipleTimer mt;
    Timer *t = mt.Register("a", Timer(2.0f));
    mt.SetEnabled(false);
    mt.Update(1.0f); // !IsEnabled() -> 조기 반환
    REQUIRE_THAT(t->GetPassedTime(), WithinAbs(0.0f, 1e-7f));
    mt.SetEnabled(true);
    mt.Update(1.0f);
    REQUIRE_THAT(t->GetPassedTime(), WithinAbs(1.0f, 1e-7f));
}
