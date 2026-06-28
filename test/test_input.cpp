// SJH::KeyboardInput<TAction> / SJH::MouseInput characterization - 창 인자 없는 디스패처 계약 잠금.
// PollHeld(window) 는 GLFW 창이 필요하므로 제외. Dispatch / HandleButton / HandleMove 만 검증.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "input/keyboard_input.h"
#include "input/mouse_input.h"

using Catch::Matchers::WithinAbs;
using SJH::KeyboardInput;
using SJH::MouseInput;

namespace
{
    // 테스트용 논리 액션 어휘 (enum class - GCC/Clang/MSVC 기본 해시 사용).
    enum class TestAction
    {
        MoveUp,
        Fire,
    };
}

TEST_CASE("Input(Keyboard): Press 핸들러는 BindKey+PRESS 시 호출", "[input]")
{
    KeyboardInput<TestAction> kb;
    int pressCount = 0;
    kb.BindKey(TestAction::MoveUp, GLFW_KEY_W);
    kb.BindPressHandler(TestAction::MoveUp, [&]() { pressCount++; });

    kb.Dispatch(GLFW_KEY_W, GLFW_PRESS); // PRESS=1
    REQUIRE(pressCount == 1);
    kb.Dispatch(GLFW_KEY_W, GLFW_PRESS);
    REQUIRE(pressCount == 2);
}

TEST_CASE("Input(Keyboard): 미바인딩 키는 no-op", "[input]")
{
    KeyboardInput<TestAction> kb;
    int pressCount = 0;
    kb.BindKey(TestAction::MoveUp, GLFW_KEY_W);
    kb.BindPressHandler(TestAction::MoveUp, [&]() { pressCount++; });

    kb.Dispatch(GLFW_KEY_S, GLFW_PRESS); // 미바인딩 - 조기 반환
    REQUIRE(pressCount == 0);
}

TEST_CASE("Input(Keyboard): Press/Release 핸들러는 액션별로 구분", "[input]")
{
    KeyboardInput<TestAction> kb;
    int pressCount = 0;
    int releaseCount = 0;
    kb.BindKey(TestAction::Fire, GLFW_KEY_SPACE);
    kb.BindPressHandler(TestAction::Fire, [&]() { pressCount++; });
    kb.BindReleaseHandler(TestAction::Fire, [&]() { releaseCount++; });

    kb.Dispatch(GLFW_KEY_SPACE, GLFW_PRESS); // PRESS -> press 핸들러만
    REQUIRE(pressCount == 1);
    REQUIRE(releaseCount == 0);

    kb.Dispatch(GLFW_KEY_SPACE, GLFW_RELEASE); // RELEASE -> release 핸들러만
    REQUIRE(pressCount == 1);
    REQUIRE(releaseCount == 1);
}

TEST_CASE("Input(Keyboard): REPEAT 등 그 외 action 은 무시", "[input]")
{
    KeyboardInput<TestAction> kb;
    int pressCount = 0;
    int releaseCount = 0;
    kb.BindKey(TestAction::Fire, GLFW_KEY_SPACE);
    kb.BindPressHandler(TestAction::Fire, [&]() { pressCount++; });
    kb.BindReleaseHandler(TestAction::Fire, [&]() { releaseCount++; });

    kb.Dispatch(GLFW_KEY_SPACE, GLFW_REPEAT); // REPEAT=2 -> press/release 모두 무시
    REQUIRE(pressCount == 0);
    REQUIRE(releaseCount == 0);
}

TEST_CASE("Input(Keyboard): BindKey 재바인딩은 덮어쓰기", "[input]")
{
    KeyboardInput<TestAction> kb;
    int upCount = 0;
    int fireCount = 0;
    kb.BindKey(TestAction::MoveUp, GLFW_KEY_W);
    kb.BindKey(TestAction::Fire, GLFW_KEY_W); // 같은 키 재바인딩 -> Fire 로 덮어씀
    kb.BindPressHandler(TestAction::MoveUp, [&]() { upCount++; });
    kb.BindPressHandler(TestAction::Fire, [&]() { fireCount++; });

    kb.Dispatch(GLFW_KEY_W, GLFW_PRESS);
    REQUIRE(upCount == 0);
    REQUIRE(fireCount == 1);
}

TEST_CASE("Input(Keyboard): UnbindKey 후 no-op", "[input]")
{
    KeyboardInput<TestAction> kb;
    int pressCount = 0;
    kb.BindKey(TestAction::MoveUp, GLFW_KEY_W);
    kb.BindPressHandler(TestAction::MoveUp, [&]() { pressCount++; });
    kb.UnbindKey(GLFW_KEY_W);
    kb.Dispatch(GLFW_KEY_W, GLFW_PRESS);
    REQUIRE(pressCount == 0);
}

TEST_CASE("Input(Mouse): 드래그 버튼 press/release 가 IsDragging 토글", "[input]")
{
    MouseInput mouse;
    REQUIRE_FALSE(mouse.IsDragging());

    mouse.HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 100.0, 200.0);
    REQUIRE(mouse.IsDragging());

    mouse.HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE, 100.0, 200.0);
    REQUIRE_FALSE(mouse.IsDragging());
}

TEST_CASE("Input(Mouse): CancelDrag 는 드래그 강제 해제", "[input]")
{
    MouseInput mouse;
    mouse.HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0.0, 0.0);
    REQUIRE(mouse.IsDragging());
    mouse.CancelDrag();
    REQUIRE_FALSE(mouse.IsDragging());
}

TEST_CASE("Input(Mouse): HandleMove - 드래그 중에만 delta 콜백", "[input]")
{
    MouseInput mouse;
    double lastDx = 0.0;
    double lastDy = 0.0;
    int lookCount = 0;
    mouse.BindLookHandler([&](double dx, double dy) { lastDx = dx; lastDy = dy; lookCount++; });

    // 드래그 비활성 - 콜백 없음
    mouse.HandleMove(50.0, 50.0);
    REQUIRE(lookCount == 0);

    // press 로 기준점 (100, 200) 저장 후 이동
    mouse.HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 100.0, 200.0);
    mouse.HandleMove(110.0, 195.0); // delta = (110-100, 195-200) = (10, -5)
    REQUIRE(lookCount == 1);
    REQUIRE_THAT(lastDx, WithinAbs(10.0, 1e-9));
    REQUIRE_THAT(lastDy, WithinAbs(-5.0, 1e-9));

    // 연속 이동 - 기준점이 직전 위치(110,195)로 갱신됨
    mouse.HandleMove(120.0, 195.0); // delta = (120-110, 195-195) = (10, 0)
    REQUIRE(lookCount == 2);
    REQUIRE_THAT(lastDx, WithinAbs(10.0, 1e-9));
    REQUIRE_THAT(lastDy, WithinAbs(0.0, 1e-9));
}

TEST_CASE("Input(Mouse): 버튼 press 핸들러는 드래그와 독립 디스패치", "[input]")
{
    MouseInput mouse;
    int leftCount = 0;
    mouse.BindButtonPressHandler(GLFW_MOUSE_BUTTON_LEFT, [&]() { leftCount++; });

    mouse.HandleButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0.0, 0.0); // press -> 핸들러
    REQUIRE(leftCount == 1);
    REQUIRE_FALSE(mouse.IsDragging()); // 좌클릭은 드래그 버튼 아님

    mouse.HandleButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0.0, 0.0); // release -> 무시
    REQUIRE(leftCount == 1);
}
