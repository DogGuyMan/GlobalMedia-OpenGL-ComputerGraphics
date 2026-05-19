/**
 * @file keyboard_input.h
 * @brief 키 -> 논리 액션 -> 핸들러 2단 디스패처 (액션 타입에 제네릭).
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
     * @brief 키보드 입력 — 물리 키를 논리 액션으로 매핑하고 액션에 핸들러를 바인딩.
     * @tparam TAction 소비자가 정의하는 액션 enum. 입력 모듈은 이 어휘를 알지 못한다.
     * @details
     *  - 1단: @ref BindKey — 물리 GLFW 키 ↔ 논리 액션 (InputMap 계층).
     *  - 2단: @ref BindHeldHandler / @ref BindPressHandler / @ref BindReleaseHandler — 액션 ↔ 핸들러.
     *  - 연속(held)은 @ref PollHeld, 이산(press/release)은 @ref Dispatch.
     */
    template <typename TAction>
    class KeyboardInput
    {
    public:
        /// @brief 물리 키 -> 논리 액션 바인딩 (같은 키 재바인딩 시 덮어씀).
        void BindKey(int glfwKey, TAction action) { mKeyBindings[glfwKey] = action; }
        /// @brief @p glfwKey 의 키->액션 바인딩 제거.
        void UnbindKey(int glfwKey) { mKeyBindings.erase(glfwKey); }

        /// @brief 액션이 *눌려 있는 동안* 매 프레임 실행할 핸들러 (연속).
        void BindHeldHandler(TAction action, std::function<void()> handler)
        {
            mHeldHandlers[action] = std::move(handler);
        }
        /// @brief 액션 키가 *눌리는 순간* 1회 실행할 핸들러 (이산, key down).
        void BindPressHandler(TAction action, std::function<void()> handler)
        {
            mPressHandlers[action] = std::move(handler);
        }
        /// @brief 액션 키가 *떼지는 순간* 1회 실행할 핸들러 (이산, key up).
        void BindReleaseHandler(TAction action, std::function<void()> handler)
        {
            mReleaseHandlers[action] = std::move(handler);
        }

        /// @brief 매 프레임 — 바인딩된 키 중 현재 눌린 키의 액션을 held 핸들러로 디스패치.
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

        /// @brief GLFW key 콜백 위임 — GLFW_PRESS 면 press, GLFW_RELEASE 면 release 핸들러.
        ///        GLFW_REPEAT 등 그 외 action 은 무시 (연속은 @ref PollHeld 담당).
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
        std::unordered_map<int, TAction> mKeyBindings;
        std::unordered_map<TAction, std::function<void()>> mHeldHandlers;
        std::unordered_map<TAction, std::function<void()>> mPressHandlers;
        std::unordered_map<TAction, std::function<void()>> mReleaseHandlers;
    };
}
#endif // __SJH_KEYBOARD_INPUT_H__
