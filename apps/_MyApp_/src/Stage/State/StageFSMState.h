#ifndef __TOPDOWNSHOOTER_STAGE_FSM_H__
#define __TOPDOWNSHOOTER_STAGE_FSM_H__

#include "fsm/fsm_state.h"
#include "../Stage.h"
#include <cstdint>
namespace TopdownShooter::Stage
{
	class BaseStageFsmState : SJH::FSM::IFsmState<Stage> {
		protected:
			EStageStatus mStateFlag;
			EStageStatus mTransitFlag;
		public :
			uint64_t GetStateFlag() const {return static_cast<uint64_t>(mStateFlag);}
			uint64_t GetTransitFlag() const {return static_cast<uint64_t>(mTransitFlag);}
	};

} // namespace SJH::FSM

#endif // __TOPDOWNSHOOTER_FSM_FSM_STATE_H__
