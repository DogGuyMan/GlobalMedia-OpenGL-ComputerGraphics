#ifndef __TOPDOWNSHOOTER_STAGE_FSM_H__
#define __TOPDOWNSHOOTER_STAGE_FSM_H__

#include "fsm/fsm_state.h"
#include "../Stage.h"
#include "scene/actor.h"
#include <cstdint>

namespace TopdownShooter::Stage
{
	class StageStateMachine; // 전방 선언 — State 가 역참조(mFsm) 보유 (D8)

	/// @brief Stage FSM State 공통 베이스 — TOwner = Root Actor (D1).
	/// @details IFsmState 에 GetFsm() 이 없어 각 State 가 생성자에 StageStateMachine* 주입(D8).
	class BaseStageFsmState : public SJH::FSM::IFsmState<SJH::Scene::Actor>
	{
	  protected:
		EStageStatus       mStateFlag;
		EStageStatus       mTransitFlag;
		StageStateMachine *mFsm = nullptr;

	  public:
		BaseStageFsmState(StageStateMachine *fsm, EStageStatus stateFlag, EStageStatus transitFlag)
		    : mStateFlag(stateFlag), mTransitFlag(transitFlag), mFsm(fsm)
		{
		}

		uint64_t GetStateFlag() const override { return static_cast<uint64_t>(mStateFlag); }
		uint64_t GetTransitFlag() const override { return static_cast<uint64_t>(mTransitFlag); }
		// OnEnter / OnUpdate / OnExit 는 IFsmState 순수가상 — 구상 State(S6)가 구현
	};
} // namespace TopdownShooter::Stage

#endif // __TOPDOWNSHOOTER_STAGE_FSM_H__
