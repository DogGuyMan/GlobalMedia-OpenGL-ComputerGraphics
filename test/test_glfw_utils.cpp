#include <catch2/catch_test_macros.hpp>

#include "input/glfw_input_utils.h"

namespace gu = glfw_utils;

// ============================================================================
// ActionToString — switch 분기 4개 검증 (PRESS / RELEASE / REPEAT / default)
//
// constexpr 이므로 STATIC_REQUIRE 로 컴파일 타임 검증.
// constexpr 키워드가 떼지면 컴파일 에러 -> 런타임까지 갈 필요 없음.
// ============================================================================

TEST_CASE("ActionToString maps known GLFW actions", "[glfw_utils][ActionToString]") {
    STATIC_REQUIRE(gu::ActionToString(GLFW_PRESS)   == "Pressed");
    STATIC_REQUIRE(gu::ActionToString(GLFW_RELEASE) == "Released");
    STATIC_REQUIRE(gu::ActionToString(GLFW_REPEAT)  == "Repeat");
}

TEST_CASE("ActionToString returns Unknown for invalid action", "[glfw_utils][ActionToString]") {
    // default 분기 — 다양한 잘못된 값으로 일관성 검증.
    STATIC_REQUIRE(gu::ActionToString(-1)     == "Unknown");
    STATIC_REQUIRE(gu::ActionToString(999)    == "Unknown");
    STATIC_REQUIRE(gu::ActionToString(0xDEAD) == "Unknown");
}

// ============================================================================
// Mod{Ctrl,Shift,Alt} — bitmask & 연산 검증
//
// 각 함수의 계약: "자기 비트만 검사한다."
// -> 다른 modifier 가 함께 눌려도 영향 없어야 하고,
//   엉뚱한 비트만 켜져 있을 때 자기 함수는 "-" 반환해야 함.
// ============================================================================

TEST_CASE("ModCtrl detects Control bit", "[glfw_utils][Mod]") {
    STATIC_REQUIRE(gu::ModCtrl(0)                                == "-"); // 비트 없음
    STATIC_REQUIRE(gu::ModCtrl(GLFW_MOD_CONTROL)                 == "C"); // Ctrl 단독
    STATIC_REQUIRE(gu::ModCtrl(GLFW_MOD_SHIFT)                   == "-"); // 다른 비트만 -> "-"
    STATIC_REQUIRE(gu::ModCtrl(GLFW_MOD_CONTROL | GLFW_MOD_SHIFT) == "C"); // 다른 비트와 동시
}

TEST_CASE("ModShift detects Shift bit", "[glfw_utils][Mod]") {
    STATIC_REQUIRE(gu::ModShift(0)                                == "-");
    STATIC_REQUIRE(gu::ModShift(GLFW_MOD_SHIFT)                   == "S");
    STATIC_REQUIRE(gu::ModShift(GLFW_MOD_CONTROL)                 == "-");
    STATIC_REQUIRE(gu::ModShift(GLFW_MOD_CONTROL | GLFW_MOD_SHIFT) == "S");
}

TEST_CASE("ModAlt detects Alt bit", "[glfw_utils][Mod]") {
    STATIC_REQUIRE(gu::ModAlt(0)                                              == "-");
    STATIC_REQUIRE(gu::ModAlt(GLFW_MOD_ALT)                                   == "A");
    STATIC_REQUIRE(gu::ModAlt(GLFW_MOD_CONTROL)                               == "-");
    STATIC_REQUIRE(gu::ModAlt(GLFW_MOD_CONTROL | GLFW_MOD_SHIFT | GLFW_MOD_ALT) == "A");
}

// ============================================================================
// 함수 간 독립성 — 적대적 사고로 도출된 케이스.
//
// "ModCtrl 가 실수로 Shift 비트를 검사하도록 누군가 오타를 만들면" 같은
// 회귀를 잡는 *교차 검증*. 단일 함수만 보면 못 잡는다.
// ============================================================================

TEST_CASE("Mod functions are independent — each inspects its own bit only",
          "[glfw_utils][Mod][independence]") {
    // 모든 비트 동시 켜졌을 때 — 각 함수가 자기 비트만 보고 응답해야 함.
    constexpr int all = GLFW_MOD_CONTROL | GLFW_MOD_SHIFT | GLFW_MOD_ALT;
    STATIC_REQUIRE(gu::ModCtrl(all)  == "C");
    STATIC_REQUIRE(gu::ModShift(all) == "S");
    STATIC_REQUIRE(gu::ModAlt(all)   == "A");

    // Ctrl 만 켜져 있을 때 — Shift/Alt 가 비트 누설 없이 "-" 반환해야 함.
    STATIC_REQUIRE(gu::ModCtrl(GLFW_MOD_CONTROL)  == "C");
    STATIC_REQUIRE(gu::ModShift(GLFW_MOD_CONTROL) == "-");
    STATIC_REQUIRE(gu::ModAlt(GLFW_MOD_CONTROL)   == "-");
}
