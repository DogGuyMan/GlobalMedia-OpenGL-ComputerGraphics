/**
 * @file game_action.h
 * @brief 게임의 논리 입력 액션 어휘. 입력 모듈이 아닌 *소비자* 가 소유 — 자유롭게 확장.
 */
#ifndef __SJH_GAME_ACTION_H__
#define __SJH_GAME_ACTION_H__

namespace SJH
{
    /// @brief 논리 입력 액션. KeyboardInput<GameAction> 의 액션 타입. 새 액션은 여기 추가.
    enum class GameAction
    {
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        LookAround,
        Count
    };
}
#endif // __SJH_GAME_ACTION_H__
