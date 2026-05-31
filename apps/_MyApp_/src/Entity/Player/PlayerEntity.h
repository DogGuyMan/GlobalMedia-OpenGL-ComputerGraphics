#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER__
#include "BaseEntity.h"
#include "Components/Components.Interfaces.h"
#include "Components/MovementComponents.h"
#include "Components/WeaponComponents.h"
#include "InputHandler/PlayerController.h"
#include "input/mouse_input.h"

namespace TopdownShooter::Entity
{
	class PlayerEntity : public BaseEntity,
	                     public IMovable,
	                     public IAttackable
	{
	  protected:
		Components::Movement *mMovementComponentPtr;
		Components::Weapon *mWaeponComponentPtr;
		Controller::PlayerController *mPlayerControllerPtr;

		SJH::KeyboardInput<Controller::PlayerController::Action> mKeyboard;
		SJH::MouseInput mMouse;

	  public:
		virtual void DoForward(vmath::vec2 dir, float dt) {mMovementComponentPtr->DoForward(dir, dt);}
		virtual void DoAttack(IDamageable &target) {target.DoDamaged(GetNormalAtk());}
		virtual int GetNormalAtk() const { return (int)mWaeponComponentPtr->Damage.GetValue();}
		Components::Movement& GetMovemenet() const {return *mMovementComponentPtr;}
		Components::Weapon& GetWeapon() const {return *mWaeponComponentPtr;}
	};
}; // namespace TopdownShooter::Entity
#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER__