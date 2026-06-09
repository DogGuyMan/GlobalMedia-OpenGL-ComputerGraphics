/**
 * @file mouse_input.h
 * @brief 마우스 드래그 (dx, dy) delta 콜백 + 버튼 press 이산 디스패처.
 *
 * @details
 *  ### 책임
 *  - **드래그(시점 조작)**: 지정 버튼(기본 `GLFW_MOUSE_BUTTON_RIGHT`) press~release 동안
 *    cursor 이동마다 look 핸들러를 `(dx, dy)` delta 로 호출.
 *  - **버튼 이산 이벤트**: `BindButtonPressHandler` 로 등록된 버튼의 press 시 1회 콜백.
 *  - **상태 노출**: `IsDragging()` - 소비자가 별도 플래그를 들고 다닐 필요 없음.
 *
 *  ### 비-책임
 *  - [X] 키보드 입력 - `KeyboardInput<TAction>` 담당.
 *  - [X] GLFW 콜백 등록 - 호출자가 `glfwSetMouseButtonCallback` / `glfwSetCursorPosCallback` 후
 *       `HandleButton` / `HandleMove` 를 직접 위임.
 *  - [X] 드래그 버튼 변경 - `mDragButton` 은 헤더 상수(`GLFW_MOUSE_BUTTON_RIGHT`) 로 고정.
 *       변경이 필요하면 생성자 파라미터 도입 예정.
 *
 * @note `mIsDragging` 이 "시점 조작 중" 상태의 *단일 진실* - 소비자는 별도 플래그 불요.
 */
#ifndef __SJH_MOUSE_INPUT_H__
#define __SJH_MOUSE_INPUT_H__

#include <GLFW/glfw3.h>
#include <functional>
#include <unordered_map>

namespace SJH
{
    /**
     * @brief 마우스 드래그 입력 + 버튼 이산 이벤트 디스패처.
     * @details
     *  ### 드래그 흐름
     *  1. `HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, x, y)` -> `mIsDragging = true`, 기준점 저장.
     *  2. `HandleMove(x, y)` 매 cursor-pos 콜백 -> delta `(dx, dy)` 산출 -> look 핸들러 호출.
     *  3. `HandleButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE, ...)` -> `mIsDragging = false`.
     *
     *  ### 버튼 이산 이벤트 흐름
     *  `BindButtonPressHandler(button, fn)` 등록 후 `HandleButton` 위임 -> `GLFW_PRESS` 시 `fn()` 호출.
     *  드래그 처리와 독립 - 드래그 버튼에도 별도 press 핸들러 등록 가능.
     */
    class MouseInput
    {
    public:
        /// @brief 드래그 중 이동마다 `(dx, dy)` 로 호출할 look 핸들러 등록.
        /// @param handler delta `(dx, dy)` 를 받아 카메라 회전 등을 처리하는 콜백.
        ///                논리적으로 `GameAction::LookAround` 에 해당.
        void BindLookHandler(std::function<void(double dx, double dy)> handler);

        /// @brief look 핸들러 해제 (`mLookHandler = nullptr`).
        void UnbindLook();

        /// @brief 버튼이 *눌리는 순간* 1회 실행할 핸들러 등록 (이산, button down).
        /// @param button GLFW 버튼 코드 (예: @c GLFW_MOUSE_BUTTON_LEFT).
        /// @param handler `GLFW_PRESS` 수신 시 1회 호출되는 콜백. `KeyboardInput::BindPressHandler` 대칭.
        void BindButtonPressHandler(int button, std::function<void()> handler);

        /// @brief @p button 의 press 핸들러 제거.
        /// @param button 제거할 버튼 코드.
        void UnbindButtonPress(int button);

        /// @brief GLFW mouse-button 콜백 위임.
        /// @param button GLFW 버튼 코드.
        /// @param action GLFW 액션 (`GLFW_PRESS` / `GLFW_RELEASE`).
        /// @param x cursor X (화면 픽셀). 드래그 시작점 기록에 사용.
        /// @param y cursor Y (화면 픽셀). 드래그 시작점 기록에 사용.
        /// @details 드래그 버튼 press -> `mIsDragging = true` + 기준점 저장.
        ///          드래그 버튼 release -> `mIsDragging = false`.
        ///          등록된 버튼의 press 핸들러 디스패치는 드래그 처리와 *독립적으로* 수행.
        void HandleButton(int button, int action, double x, double y);

        /// @brief GLFW cursor-pos 콜백 위임 - 드래그 중이면 delta 산출 후 look 핸들러 호출.
        /// @param x cursor 현재 X (화면 픽셀).
        /// @param y cursor 현재 Y (화면 픽셀).
        /// @details 드래그 비활성이면 조기 반환. 활성이면 `(x - mLastX, y - mLastY)` delta 산출.
        void HandleMove(double x, double y);

        /// @brief 드래그 강제 해제 (`mIsDragging = false`).
        /// @details 창 포커스 loss / ESC 처리 등 예외 상황에서 드래그 상태를 안전하게 초기화.
        void CancelDrag();

        /// @brief 현재 드래그(=시점 조작) 중인지.
        /// @return `true` 이면 드래그 버튼이 눌린 채 이동 중.
        bool IsDragging() const { return mIsDragging; }

    private:
        std::function<void(double, double)> mLookHandler;                    ///< @brief look(시점 회전) 콜백.
        std::unordered_map<int, std::function<void()>> mButtonPressHandlers; ///< @brief 버튼 코드 -> 이산 press 콜백.
        bool   mIsDragging = false;                                          ///< @brief 드래그 활성 상태 플래그.
        double mLastX = 0.0;                                                 ///< @brief 이전 프레임 cursor X (delta 산출 기준).
        double mLastY = 0.0;                                                 ///< @brief 이전 프레임 cursor Y (delta 산출 기준).
        int    mDragButton = GLFW_MOUSE_BUTTON_RIGHT;                        ///< @brief 드래그를 활성화하는 버튼 (기본: 우클릭).
    };
}
#endif // __SJH_MOUSE_INPUT_H__
