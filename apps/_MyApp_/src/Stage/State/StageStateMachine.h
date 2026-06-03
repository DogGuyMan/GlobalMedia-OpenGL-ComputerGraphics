#ifndef __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__
#define __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__

#include "../Stage.h"
#include "fsm/state_machine.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage
{
	/// @brief Stage FSM — Owner = Root Actor (D1). game_application 멤버로 보유.
	/// @details startup=Title 로 생성 → OnEnter() 호출 시 TitleState 즉시 진입(Boot State 불요).
	class StageStateMachine : public SJH::FSM::StateMachine<EStageStatus, SJH::Scene::Actor>
	{
	  public:
		explicit StageStateMachine(SJH::Scene::Actor &root)
		    : SJH::FSM::StateMachine<EStageStatus, SJH::Scene::Actor>(root, EStageStatus::Title)
		{
		}
	};
} // namespace TopdownShooter::Stage

#endif // __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__
