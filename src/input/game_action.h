/**
 * @file game_action.h
 * @brief 게임 전용 논리 입력 액션 어휘 - `KeyboardInput<GameAction>` 의 TAction 인스턴스.
 *
 * @details
 *  ### 책임
 *  - 물리 키와 무관한 *논리 입력 어휘* 정의 - `KeyboardInput::BindKey(action, glfwKey)` 의 좌항 타입.
 *  - 새 액션이 필요하면 `Count` 앞에 열거자를 추가하고 `BindKey` / 핸들러를 재배선.
 *
 *  ### 비-책임
 *  - [X] 키->액션 바인딩 - `KeyboardInput::BindKey` 책임.
 *  - [X] 핸들러 실행 - `KeyboardInput::PollHeld` / `Dispatch` 책임.
 *  - [X] 마우스 입력 액션 - `MouseInput` 에서 독립 처리 (드래그/버튼 콜백 직접 등록).
 *
 * @note 이 파일은 입력 모듈(`SJH::input` 라이브러리) 이 아닌 *소비자(게임 코드)* 가 소유하는
 *       어휘집. 탑다운 슈터 `_MyApp_` 전용이 아니라 `KeyboardInput<GameAction>` 를 쓰는
 *       모든 데모가 공유한다.
 */
#ifndef __SJH_GAME_ACTION_H__
#define __SJH_GAME_ACTION_H__

namespace SJH
{
    /**
     * @brief `KeyboardInput<TAction>` 의 기본 논리 입력 액션 열거형.
     * @details
     *  각 열거자는 물리 키코드(`GLFW_KEY_*`)와 분리된 *의도* 를 나타낸다.
     *  `KeyboardInput::BindKey(action, glfwKey)` 로 런타임에 재매핑 가능하므로
     *  키 레이아웃(QWERTY/AZERTY 등) 과 무관하게 게임 로직을 작성할 수 있다.
     */
    enum class GameAction
    {
        MoveForward,  ///< @brief 전진 이동 (기본: @c GLFW_KEY_W).
        MoveBackward, ///< @brief 후진 이동 (기본: @c GLFW_KEY_S).
        MoveLeft,     ///< @brief 왼쪽 이동 (기본: @c GLFW_KEY_A).
        MoveRight,    ///< @brief 오른쪽 이동 (기본: @c GLFW_KEY_D).
        MoveUp,       ///< @brief 상승 이동 (기본: @c GLFW_KEY_E).
        MoveDown,     ///< @brief 하강 이동 (기본: @c GLFW_KEY_Q).
        LookAround,   ///< @brief 시점 회전 (마우스 드래그와 연동되는 경우 키 바인딩 없이 사용).
        Count         ///< @brief 열거자 수 - 배열 크기 산출용. 실제 액션 아님.
    };
}
#endif // __SJH_GAME_ACTION_H__
