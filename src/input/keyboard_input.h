/**
 * @file keyboard_input.h
 * @brief 물리 키 -> 논리 액션 -> 핸들러, 2단 디스패처 템플릿 - 액션 타입에 제네릭.
 *
 * @details
 *  ### 책임
 *  - **1단(InputMap)**: `BindKey(action, glfwKey)` - 물리 GLFW 키코드를 소비자 정의 논리 액션에 매핑.
 *  - **2단(Handler)**: `BindHeldHandler` / `BindPressHandler` / `BindReleaseHandler` - 논리 액션에 콜백 등록.
 *  - **연속 폴링**: `PollHeld(window)` - 매 프레임 `glfwGetKey` 로 누름 상태를 검사해 held 핸들러 호출.
 *  - **이산 이벤트**: `Dispatch(glfwKey, glfwAction)` - GLFW key 콜백에서 위임받아 press/release 핸들러 호출.
 *
 *  ### 비-책임
 *  - [X] 마우스 입력 - `MouseInput` 담당.
 *  - [X] 액션 어휘 정의 - 소비자가 `TAction` 열거형으로 정의 (예: `GameAction`).
 *  - [X] GLFW 창/콜백 등록 - 호출자(`main.cpp` / `App`)가 직접 `glfwSetKeyCallback` 후 `Dispatch` 위임.
 *
 * @note `TAction` 은 `std::unordered_map` 키로 사용되므로 `std::hash<TAction>` 특수화가 필요하다.
 *       `enum class` 는 C++14 이후 기본 특수화가 없어 직접 해시 특수화나
 *       `static_cast<int>` 래퍼가 필요할 수 있다 - 현재 코드베이스는 MSVC/GCC/Clang 모두
 *       `enum class` 해시를 기본 제공하므로 문제 없음.
 */
#ifndef __SJH_KEYBOARD_INPUT_H__
#define __SJH_KEYBOARD_INPUT_H__

#include <GLFW/glfw3.h>
#include <functional>
#include <unordered_map>
#include <utility>

namespace SJH
{
    /**
     * @brief 물리 키를 논리 액션으로 매핑하고 액션에 콜백 핸들러를 바인딩하는 2단 디스패처.
     * @tparam TAction 소비자가 정의하는 액션 enum (예: `SJH::GameAction`).
     *                 입력 모듈은 이 어휘를 *알지 못한다* - 완전한 역전.
     * @details
     *  ### 디스패치 경로
     *  ```
     *  물리 키(GLFW_KEY_*)  ->  BindKey  ->  TAction  ->  BindXxxHandler  ->  std::function<void()>
     *  ```
     *  - **연속(held)**: 매 프레임 `PollHeld(window)` 호출 - `glfwGetKey` 기반.
     *  - **이산(press/release)**: GLFW key 콜백 -> `Dispatch(key, action)` 위임.
     *
     *  ### 멀티-바인딩 규칙
     *  - 같은 `glfwKey` 를 다시 `BindKey` 하면 덮어쓴다.
     *  - 같은 `TAction` 에 핸들러를 다시 `BindXxxHandler` 하면 덮어쓴다.
     *  - 한 액션에 held/press/release 핸들러를 각각 독립으로 등록할 수 있다.
     */
    template <typename TAction>
    class KeyboardInput
    {
    public:
        /// @brief 물리 키 -> 논리 액션 바인딩. 같은 @p glfwKey 재바인딩 시 덮어씀.
        /// @param action 매핑할 논리 액션.
        /// @param glfwKey GLFW 키코드 (예: @c GLFW_KEY_W).
        void BindKey(TAction action, int glfwKey) { mKeyBindings[glfwKey] = action; }

        /// @brief @p glfwKey 의 키->액션 바인딩 제거.
        /// @param glfwKey 제거할 물리 키코드.
        void UnbindKey(int glfwKey) { mKeyBindings.erase(glfwKey); }

        /// @brief 액션이 *눌려 있는 동안* 매 프레임 실행할 핸들러 등록 (연속).
        /// @param action 핸들러를 연결할 논리 액션.
        /// @param handler `PollHeld` 호출 시 눌려 있으면 매 프레임 호출되는 콜백.
        void BindHeldHandler(TAction action, std::function<void()> handler)
        {
            mHeldHandlers[action] = std::move(handler);
        }

        /// @brief 액션 키가 *눌리는 순간* 1회 실행할 핸들러 등록 (이산, key down).
        /// @param action 핸들러를 연결할 논리 액션.
        /// @param handler `Dispatch` 에서 `GLFW_PRESS` 수신 시 1회 호출되는 콜백.
        void BindPressHandler(TAction action, std::function<void()> handler)
        {
            mPressHandlers[action] = std::move(handler);
        }

        /// @brief 액션 키가 *떼지는 순간* 1회 실행할 핸들러 등록 (이산, key up).
        /// @param action 핸들러를 연결할 논리 액션.
        /// @param handler `Dispatch` 에서 `GLFW_RELEASE` 수신 시 1회 호출되는 콜백.
        void BindReleaseHandler(TAction action, std::function<void()> handler)
        {
            mReleaseHandlers[action] = std::move(handler);
        }

        /// @brief 매 프레임 호출 - 바인딩된 키 중 현재 눌린 키의 액션을 held 핸들러로 디스패치.
        /// @param window `glfwGetKey` 에 전달할 GLFW 창 핸들.
        /// @details `glfwGetKey(window, key) == GLFW_PRESS` 인 모든 바인딩 키를 순회해
        ///          해당 액션의 held 핸들러를 호출한다. 핸들러가 미등록이면 무시.
        void PollHeld(GLFWwindow *window)
        {
            for (const auto &[glfwKey, action] : mKeyBindings)
            {
                if (glfwGetKey(window, glfwKey) != GLFW_PRESS)
                    continue;
                auto it = mHeldHandlers.find(action);
                if (it != mHeldHandlers.end() && it->second)
                    it->second();
            }
        }

        /// @brief GLFW key 콜백 위임 - `GLFW_PRESS` 면 press, `GLFW_RELEASE` 면 release 핸들러.
        /// @param glfwKey GLFW 키코드 (콜백 파라미터 `key` 그대로 전달).
        /// @param glfwAction GLFW 액션 (`GLFW_PRESS` / `GLFW_RELEASE` / `GLFW_REPEAT`).
        /// @details `GLFW_REPEAT` 등 그 외 @p glfwAction 은 무시 - 연속 입력은 @ref PollHeld 담당.
        ///          키 바인딩이 없으면 조기 반환.
        void Dispatch(int glfwKey, int glfwAction)
        {
            auto keyIt = mKeyBindings.find(glfwKey);
            if (keyIt == mKeyBindings.end())
                return;
            const TAction action = keyIt->second;

            std::unordered_map<TAction, std::function<void()>> *table = nullptr;
            if (glfwAction == GLFW_PRESS)
                table = &mPressHandlers;
            else if (glfwAction == GLFW_RELEASE)
                table = &mReleaseHandlers;
            else
                return;

            auto it = table->find(action);
            if (it != table->end() && it->second)
                it->second();
        }

    private:
        std::unordered_map<int, TAction> mKeyBindings;                       ///< @brief 물리 키코드 -> 논리 액션.
        std::unordered_map<TAction, std::function<void()>> mHeldHandlers;    ///< @brief 논리 액션 -> 연속(held) 콜백.
        std::unordered_map<TAction, std::function<void()>> mPressHandlers;   ///< @brief 논리 액션 -> 이산 press 콜백.
        std::unordered_map<TAction, std::function<void()>> mReleaseHandlers; ///< @brief 논리 액션 -> 이산 release 콜백.
    };
}
#endif // __SJH_KEYBOARD_INPUT_H__
