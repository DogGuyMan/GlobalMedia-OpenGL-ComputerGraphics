#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER__
#include "BaseEntity.h"
#include "Components/Components.Interfaces.h"
#include "Components/MovementComponents.h"
#include "Components/WeaponComponents.h"
#include "InputHandler/PlayerController.h"
#include "Player/PlayerMovementComponents.h"
#include "input/mouse_input.h"

namespace TopdownShooter::Entity
{
	class PlayerEntity : public BaseEntity,
	                     public IMovable,
	                     public IAttackable
	{
	  protected:
		Components::PlayerMovement *mPlaterMovementComponentPtr;
		Components::Weapon *mWaeponComponentPtr;
		Controller::PlayerController *mPlayerControllerPtr;

		SJH::KeyboardInput<Controller::PlayerController::Action> mKeyboard;
		SJH::MouseInput mMouse;

	  public:
		virtual void DoForward(vmath::vec2 dir) = 0;
		virtual void DoAttack(IAttackable &target) = 0;
		virtual int GetNormalAtk() const = 0;
	};
}; // namespace TopdownShooter::Entity
#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER__