/**
 * @file test_keyboard_input.cpp
 * @brief SJH::KeyboardInput<TAction> — 키->액션->핸들러 디스패치 (GL 컨텍스트 불요).
 */
#include "input/keyboard_input.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
    // KeyboardInput 은 TAction 이 enum 이라는 점만 가정 — 테스트용 액션 enum.
    enum class TestAction { Jump, Crouch };
}

TEST_CASE("KeyboardInput press — BindKey+BindPressHandler+Dispatch(PRESS)", "[keyboard_input]")
{
    SJH::KeyboardInput<TestAction> kb;
    int jumps = 0;
    kb.BindKey(GLFW_KEY_SPACE, TestAction::Jump);
    kb.BindPressHandler(TestAction::Jump, [&] { ++jumps; });

    kb.Dispatch(GLFW_KEY_SPACE, GLFW_PRESS);
    REQUIRE(jumps == 1);
}

TEST_CASE("KeyboardInput release — Dispatch(RELEASE) 가 release 핸들러", "[keyboard_input]")
{
    SJH::KeyboardInput<TestAction> kb;
    int releases = 0;
    kb.BindKey(GLFW_KEY_SPACE, TestAction::Jump);
    kb.BindReleaseHandler(TestAction::Jump, [&] { ++releases; });

    kb.Dispatch(GLFW_KEY_SPACE, GLFW_RELEASE);
    REQUIRE(releases == 1);
}

TEST_CASE("KeyboardInput 페이즈 격리 — press 만 바인딩 시 RELEASE/REPEAT 무동작", "[keyboard_input]")
{
    SJH::KeyboardInput<TestAction> kb;
    int presses = 0;
    kb.BindKey(GLFW_KEY_SPACE, TestAction::Jump);
    kb.BindPressHandler(TestAction::Jump, [&] { ++presses; });

    kb.Dispatch(GLFW_KEY_SPACE, GLFW_RELEASE); // release 핸들러 없음
    REQUIRE(presses == 0);
    kb.Dispatch(GLFW_KEY_SPACE, GLFW_REPEAT);  // repeat 은 무시
    REQUIRE(presses == 0);
    kb.Dispatch(GLFW_KEY_SPACE, GLFW_PRESS);
    REQUIRE(presses == 1);
}

TEST_CASE("KeyboardInput 키->액션 해석 — 두 키를 같은 액션에", "[keyboard_input]")
{
    SJH::KeyboardInput<TestAction> kb;
    int jumps = 0;
    kb.BindKey(GLFW_KEY_SPACE, TestAction::Jump);
    kb.BindKey(GLFW_KEY_W, TestAction::Jump);
    kb.BindPressHandler(TestAction::Jump, [&] { ++jumps; });

    kb.Dispatch(GLFW_KEY_SPACE, GLFW_PRESS);
    kb.Dispatch(GLFW_KEY_W, GLFW_PRESS);
    REQUIRE(jumps == 2);
}

TEST_CASE("KeyboardInput 미바인딩 키 — Dispatch 무동작 (크래시 없음)", "[keyboard_input]")
{
    SJH::KeyboardInput<TestAction> kb;
    kb.Dispatch(GLFW_KEY_ESCAPE, GLFW_PRESS); // 바인딩 없음
    SUCCEED("미바인딩 키 Dispatch 가 안전");
}

TEST_CASE("KeyboardInput UnbindKey — 해제 후 Dispatch 무동작", "[keyboard_input]")
{
    SJH::KeyboardInput<TestAction> kb;
    int jumps = 0;
    kb.BindKey(GLFW_KEY_SPACE, TestAction::Jump);
    kb.BindPressHandler(TestAction::Jump, [&] { ++jumps; });
    kb.UnbindKey(GLFW_KEY_SPACE);

    kb.Dispatch(GLFW_KEY_SPACE, GLFW_PRESS);
    REQUIRE(jumps == 0);
}
