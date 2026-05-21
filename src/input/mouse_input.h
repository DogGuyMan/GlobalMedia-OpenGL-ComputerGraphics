/**
 * @file mouse_input.h
 * @brief 마우스 드래그 -> (dx, dy) delta 콜백. SSU MouseInput 의 GLFW 적응.
 */
#ifndef __SJH_MOUSE_INPUT_H__
#define __SJH_MOUSE_INPUT_H__

#include <GLFW/glfw3.h>
#include <functional>

namespace SJH
{
    /**
     * @brief 마우스 드래그 입력 — 드래그 버튼(기본 우클릭) press~release 동안
     *        cursor 이동마다 look 핸들러를 (dx, dy) 로 호출.
     * @details `mIsDragging` 이 "시점 조작 중" 상태의 단일 진실 — 소비자는 별도 플래그 불요.
     */
    class MouseInput
    {
    public:
        /// @brief 드래그 중 이동마다 (dx, dy) 로 호출할 핸들러 (논리적으로 LookAround 액션).
        void BindLookHandler(std::function<void(double dx, double dy)> handler);

        /// @brief GLFW mouse-button 콜백 위임 — 드래그 버튼 press 시 시작, release 시 종료.
        void HandleButton(int button, int action, double x, double y);
        /// @brief GLFW cursor-pos 콜백 위임 — 드래그 중이면 delta 산출 후 look 핸들러 호출.
        void HandleMove(double x, double y);

        /// @brief 드래그 강제 해제.
        void CancelDrag();
        /// @brief 현재 드래그(=시점 조작) 중인지.
        bool IsDragging() const { return mIsDragging; }

    private:
        std::function<void(double, double)> mLookHandler;
        bool   mIsDragging = false;
        double mLastX = 0.0;
        double mLastY = 0.0;
        int    mDragButton = GLFW_MOUSE_BUTTON_RIGHT;
    };
}
#endif // __SJH_MOUSE_INPUT_H__
