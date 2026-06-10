/**
 * @file StageStateMachine.h
 * @brief 탑다운 슈터 스테이지 FSM 진입점 - EStageStatus x Root Actor 유한 상태 기계.
 *
 * @details
 *  ### 책임
 *  - @c SJH::FSM::StateMachine<EStageStatus, SJH::Scene::Actor> 를 상속해 스테이지 전용
 *    FSM 을 구성한다.
 *  - startup 상태를 @c EStageStatus::Title 로 고정 - 별도 Boot State 불필요.
 *  - @c game_application 이 멤버로 소유하며, 씬 루트 Actor 를 Owner 로 받는다.
 *
 *  ### 비-책임
 *  - [X] 개별 State 구현 - @c StageState.Impl.h 의 TitleState / CombatPlayState / PauseState /
 *    GameOverState 가 담당.
 *  - [X] State 등록 (AddState 호출) - game_application 의 startup 코드가 담당.
 *
 *  ### 정통 매핑
 *  - Unreal @c UStateMachineComponent - 스테이지 진행 FSM.
 *  - Cocos2D @c Scene 전환 ( @c Director::runScene / @c replaceScene ).
 *
 * @note Root Actor 는 @c StageStateMachine 보다 수명이 길어야 한다 (dangling 방지).
 */
#ifndef __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__
#define __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__

#include "../Stage.h"
#include "fsm/state_machine.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage
{
	/**
	 * @brief Stage FSM - Owner = Root Actor (D1). @c game_application 멤버로 보유.
	 * @details
	 *  startup 상태를 @c EStageStatus::Title 로 고정하여 생성자에서 즉시 TitleState 에
	 *  진입한다. Boot State 는 불필요 (Title 이 초기 상태).
	 *
	 *  상태 전이 그래프:
	 *  - Title  -> CombatPlay : 화면 빈 곳 마우스 클릭
	 *  - CombatPlay -> Pause  : PauseButton 클릭 (ESC 는 sb7 강제 종료 - 사용 불가)
	 *  - CombatPlay -> GameOver : Player 사망 감지 (WaveController observer)
	 *  - Pause  -> CombatPlay : 화면 빈 곳 마우스 클릭 (볼륨 슬라이더 제외)
	 *  - GameOver             : terminal - 전이 없음 (transit=NONE)
	 */
	class StageStateMachine : public SJH::FSM::StateMachine<EStageStatus, SJH::Scene::Actor>
	{
	  public:
		/// @brief Root Actor 를 Owner 로 연결하고 startup 상태를 @c EStageStatus::Title 로 초기화.
		/// @param root 씬 루트 Actor - 상태 기계 Owner (수명이 본 객체보다 길어야 함).
		explicit StageStateMachine(SJH::Scene::Actor &root)
		    : SJH::FSM::StateMachine<EStageStatus, SJH::Scene::Actor>(root, EStageStatus::Title)
		{
		}
	};
} // namespace TopdownShooter::Stage

#endif // __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__
