#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_PLAYER_MOVEMENT__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_PLAYER_MOVEMENT__

#include "Algebraic/Stat.h"
#include "Entity/Components/MovementComponents.h"
#include "scene/actor.h"
#include <vmath.h>

namespace TopdownShooter::Entity::Components
{
	class PlayerMovement : public Movement
	{

	  public:
		PlayerMovement()
		    : Movement::Movement()
		{
		}

		PlayerMovement(float movespeed)
		    : Movement::Movement(movespeed)
		{
		}

		virtual void OnEnter() override {};
		virtual void Update(float dt) override {

		};
		virtual void OnExit() override {};

		virtual void DoForward(vmath::vec2 dir) override
		{
			auto *owner = GetOwner();
			if (!owner)
				return;
			auto &tr = owner->GetTransform();
			auto normalizedDir = vmath::normalize(dir * mMoveSpeed.GetValue());
			tr.Translate[0] += normalizedDir[0];
			tr.Translate[2] += normalizedDir[1];
		}
	};
}; // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_PLAYER_MOVEMENT__