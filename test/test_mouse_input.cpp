/**
 * @file test_mouse_input.cpp
 * @brief SJH::MouseInput — 드래그 -> delta 콜백 (GL 컨텍스트 불요).
 */
#include "input/mouse_input.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("MouseInput 드래그 — press 후 move 가 delta 콜백", "[mouse_input]")
{
    SJH::MouseInput mouse;
    double gotDx = 0.0, gotDy = 0.0;
    int calls = 0;
    mouse.BindLookHandler([&](double dx, double dy) { gotDx = dx; gotDy = dy; ++calls; });

    mouse.HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 100.0, 200.0);
    REQUIRE(mouse.IsDragging());
    mouse.HandleMove(110.0, 195.0);

    REQUIRE(calls == 1);
    REQUIRE_THAT(gotDx, WithinAbs(10.0, 1e-9));
    REQUIRE_THAT(gotDy, WithinAbs(-5.0, 1e-9));
}

TEST_CASE("MouseInput 비드래그 — press 없이 move 는 무호출", "[mouse_input]")
{
    SJH::MouseInput mouse;
    int calls = 0;
    mouse.BindLookHandler([&](double, double) { ++calls; });
    mouse.HandleMove(50.0, 50.0);
    REQUIRE(calls == 0);
    REQUIRE_FALSE(mouse.IsDragging());
}

TEST_CASE("MouseInput release — 드래그 종료 후 move 무호출", "[mouse_input]")
{
    SJH::MouseInput mouse;
    int calls = 0;
    mouse.BindLookHandler([&](double, double) { ++calls; });
    mouse.HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0.0, 0.0);
    mouse.HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE, 5.0, 5.0);
    REQUIRE_FALSE(mouse.IsDragging());
    mouse.HandleMove(20.0, 20.0);
    REQUIRE(calls == 0);
}

TEST_CASE("MouseInput CancelDrag — 강제 해제", "[mouse_input]")
{
    SJH::MouseInput mouse;
    int calls = 0;
    mouse.BindLookHandler([&](double, double) { ++calls; });
    mouse.HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0.0, 0.0);
    mouse.CancelDrag();
    REQUIRE_FALSE(mouse.IsDragging());
    mouse.HandleMove(30.0, 30.0);
    REQUIRE(calls == 0);
}

TEST_CASE("MouseInput 연속 move — delta 는 직전 위치 기준 (누적 아님)", "[mouse_input]")
{
    SJH::MouseInput mouse;
    double lastDx = 0.0;
    mouse.BindLookHandler([&](double dx, double) { lastDx = dx; });
    mouse.HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0.0, 0.0);
    mouse.HandleMove(10.0, 0.0);
    REQUIRE_THAT(lastDx, WithinAbs(10.0, 1e-9));
    mouse.HandleMove(13.0, 0.0);
    REQUIRE_THAT(lastDx, WithinAbs(3.0, 1e-9)); // 13-10 — 누적이면 13
}

TEST_CASE("MouseInput 드래그 버튼 아닌 버튼 — 드래그 시작 안 함", "[mouse_input]")
{
    SJH::MouseInput mouse;
    mouse.HandleButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0.0, 0.0);
    REQUIRE_FALSE(mouse.IsDragging());
}
