#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_MOVEMENT__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_MOVEMENT__

#include "Algebraic/Stat.h"
#include "Components.Interfaces.h"
#include "scene/actor.h"

namespace TopdownShooter::Entity::Components
{
	class Movement : public SJH::Scene::Component,
	                 public IMovable
	{
	  protected:
		Algebraic::Numeric::Stat mMoveSpeed;

	  public:
		Movement() 
			: mMoveSpeed(0.0f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
		{
		}

		Movement(float movespeed) 
			: mMoveSpeed(movespeed, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
		{
		}

		virtual void OnEnter() override {};
		virtual void Update(float dt) override {};
		virtual void OnExit() override {};

		virtual void DoForward(vmath::vec2 dir) override 
		{
			
		}
	};
} // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_MOVEMENT__