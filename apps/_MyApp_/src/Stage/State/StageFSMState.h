/**
 * @file StageFSMState.h
 * @brief Stage FSM 구상 State 들이 공유하는 공통 베이스 클래스.
 *
 * @details
 *  ### 책임
 *  - @c SJH::FSM::IFsmState<SJH::Scene::Actor> 를 상속해 @c EStageStatus 기반
 *    StateFlag / TransitFlag 를 제공한다.
 *  - 생성자에서 @c StageStateMachine 포인터를 주입받아 State 내부에서 전이
 *    (@c mFsm->TryTransit) 를 직접 요청할 수 있도록 역참조를 보유한다 (설계 결정 D8).
 *
 *  ### 비-책임
 *  - [X] OnEnter / OnUpdate / OnExit 구현 - 구상 State (TitleState / CombatPlayState /
 *    PauseState / GameOverState) 가 각자 담당 (@c StageState.Impl.h 참조).
 *  - [X] 상태 등록 / 전이 조건 판정 - @c StageStateMachine / @c SJH::FSM::StateMachine 담당.
 *
 * @note @c IFsmState 인터페이스에 @c GetFsm() 이 없으므로 각 State 생성자에
 *       @c StageStateMachine* 을 명시적으로 주입해야 한다 (D8 계약).
 */
#ifndef __TOPDOWNSHOOTER_STAGE_FSM_H__
#define __TOPDOWNSHOOTER_STAGE_FSM_H__

#include "fsm/fsm_state.h"
#include "../Stage.h"
#include "scene/actor.h"
#include <cstdint>

namespace TopdownShooter::Stage
{
	class StageStateMachine; // 전방 선언 - State 가 역참조(mFsm) 보유 (D8)

	/**
	 * @brief Stage FSM State 공통 베이스 - TOwner = Root Actor (D1).
	 * @details
	 *  @c SJH::FSM::IFsmState<SJH::Scene::Actor> 를 상속하며, @c EStageStatus 를
	 *  StateFlag / TransitFlag 로 보유한다. 단일 Transit 상태는 enum 값을 직접 저장하고,
	 *  복수 Transit (CombatPlay -> Pause | GameOver) 은 비트 OR 합성값으로 전달한다.
	 *
	 *  @c mFsm 포인터를 통해 OnUpdate 내부에서 @c TryTransit 을 직접 호출해 전이를 요청한다.
	 *  OnEnter / OnUpdate / OnExit 는 @c IFsmState 의 순수가상 - 구상 State 가 구현한다 (S6).
	 */
	class BaseStageFsmState : public SJH::FSM::IFsmState<SJH::Scene::Actor>
	{
	  protected:
		EStageStatus       mStateFlag;    ///< 이 State 의 식별자 (자기 자신 EStageStatus).
		EStageStatus       mTransitFlag;  ///< 전이 가능 대상 - 복수일 경우 비트 OR 합성.
		StageStateMachine *mFsm = nullptr; ///< FSM 역참조 - TryTransit 호출용 (D8 주입).

	  public:
		/// @brief 공통 베이스 생성자.
		/// @param fsm         부모 StateMachine 포인터 (역참조, non-owning).
		/// @param stateFlag   이 State 의 EStageStatus 식별자.
		/// @param transitFlag 전이 가능 대상 EStageStatus (복수 = 비트 OR).
		BaseStageFsmState(StageStateMachine *fsm, EStageStatus stateFlag, EStageStatus transitFlag)
		    : mStateFlag(stateFlag), mTransitFlag(transitFlag), mFsm(fsm)
		{
		}

		/// @brief IFsmState 계약 - @c mStateFlag 를 uint64_t 로 반환.
		/// @return 이 State 의 식별 플래그.
		uint64_t GetStateFlag() const override { return static_cast<uint64_t>(mStateFlag); }

		/// @brief IFsmState 계약 - @c mTransitFlag 를 uint64_t 로 반환.
		/// @return 전이 가능 대상 플래그 (비트 OR 합성 포함).
		uint64_t GetTransitFlag() const override { return static_cast<uint64_t>(mTransitFlag); }
		// OnEnter / OnUpdate / OnExit 는 IFsmState 순수가상 - 구상 State(S6)가 구현
	};
} // namespace TopdownShooter::Stage

#endif // __TOPDOWNSHOOTER_STAGE_FSM_H__
