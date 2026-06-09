/**
 * @file mouse_input.cpp
 * @brief MouseInput 구현 - 드래그 상태 관리 + delta 산출 + 버튼 이산 디스패치.
 *
 * @details
 *  ### 책임
 *  - `HandleButton` : 드래그 버튼 press/release 로 `mIsDragging` 토글 + 기준점 저장.
 *                     비-드래그 버튼의 press 핸들러도 독립적으로 디스패치.
 *  - `HandleMove`   : 드래그 활성 시 `(dx, dy)` delta 산출 -> look 핸들러 호출.
 *  - `CancelDrag`   : 예외 상황(창 포커스 loss 등)에서 드래그 상태 강제 초기화.
 *
 *  ### 비-책임
 *  - [X] GLFW 창/콜백 등록 - 호출자(`App` / `main.cpp`) 책임.
 *  - [X] 카메라 회전 로직 - look 핸들러 콜백이 소비자 코드에서 처리.
 */
#include "input/mouse_input.h"
#include <utility>

namespace SJH
{
    void MouseInput::BindLookHandler(std::function<void(double, double)> handler)
    {
        mLookHandler = std::move(handler);
    }

    void MouseInput::UnbindLook() {
	mLookHandler = nullptr;
    }

    void MouseInput::BindButtonPressHandler(int button, std::function<void()> handler)
    {
        mButtonPressHandlers[button] = std::move(handler);
    }

    void MouseInput::UnbindButtonPress(int button)
    {
        mButtonPressHandlers.erase(button);
    }

    void MouseInput::HandleButton(int button, int action, double x, double y)
    {
        // 드래그 버튼(기본 우클릭) - press~release 동안 시점 조작 상태 토글.
        if (button == mDragButton)
        {
            if (action == GLFW_PRESS)
            {
                mIsDragging = true;
                mLastX = x;
                mLastY = y;
            }
            else if (action == GLFW_RELEASE)
            {
                mIsDragging = false;
            }
        }

        // 버튼 press 핸들러 디스패치 (이산) - 드래그 처리와 독립. KeyboardInput::Dispatch 대칭.
        if (action == GLFW_PRESS)
        {
            auto it = mButtonPressHandlers.find(button);
            if (it != mButtonPressHandlers.end() && it->second)
                it->second();
        }
    }

    void MouseInput::HandleMove(double x, double y)
    {
        if (!mIsDragging)
            return;
        const double dx = x - mLastX;
        const double dy = y - mLastY;
        mLastX = x;
        mLastY = y;
        if (mLookHandler)
            mLookHandler(dx, dy);
    }

    void MouseInput::CancelDrag()
    {
        mIsDragging = false;
    }
}
