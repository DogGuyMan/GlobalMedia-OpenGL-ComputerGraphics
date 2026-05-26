#ifndef __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__
#define __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__

#include <cstdint>

#include "../Stage.h"
#include "fsm/state_machine.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage 
{
	class StageStateMachine : public SJH::FSM::StateMachine<EStageStatus, Stage> 
	{
		
	};
};

#endif // __TOPDOWNSHOOTER_STAGE_STATEMACHINE_
//H__