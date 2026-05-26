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

		virtual void DoForward(vmath::vec2 dir, float dt) override
		{
			auto *owner = GetOwner();
			if (!owner)
				return;
			// zero-vec 가드 — normalize(0) 는 NaN 발생.
			if (dir[0] == 0.0f && dir[1] == 0.0f)
				return;
			auto &tr = owner->GetTransform();
			// mMoveSpeed = units/sec  dt(초) 곱해 *프레임 변위* 산출. fps-independent.
			auto displacement = vmath::normalize(dir) * (mMoveSpeed.GetValue() * dt);
			tr.Translate[0] += displacement[0];
			tr.Translate[2] += displacement[1];
		}
	};
}; // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_MOVEMENT__