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
        // 드래그 버튼(기본 우클릭) — press~release 동안 시점 조작 상태 토글.
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

        // 버튼 press 핸들러 디스패치 (이산) — 드래그 처리와 독립. KeyboardInput::Dispatch 대칭.
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
