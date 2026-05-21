#include "input/mouse_input.h"
#include <utility>

namespace SJH
{
    void MouseInput::BindLookHandler(std::function<void(double, double)> handler)
    {
        mLookHandler = std::move(handler);
    }

    void MouseInput::HandleButton(int button, int action, double x, double y)
    {
        if (button != mDragButton)
            return;
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
