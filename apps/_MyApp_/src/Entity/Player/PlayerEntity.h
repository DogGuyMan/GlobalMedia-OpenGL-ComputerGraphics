#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER__

#include "BaseEntity.h"
#include "Components/Components.Interfaces.h"   // IMovable
#include "Components/WeaponComponents.h"        // Components::Weapon (UseWeapon 호출 — 완전형)
#include <vmath.h>

namespace TopdownShooter::Entity
{
	/// @brief 플레이어 Accessor-facade — BaseEntity(공유) + Movement/Weapon 접근 + Dash/Attack verb.
	class PlayerEntity : public BaseEntity, public IMovable
	{
	  protected:
		IMovable*           mMovement = nullptr;  // PhysicsMovement (인터페이스 캐시 — 물리/비물리 양분기)
		Components::Weapon* mWeapon   = nullptr;

	  public:
		void OnEnter() override;   // BaseEntity::OnEnter() + mMovement/mWeapon 캐시

		// accessor
		IMovable*           GetMovement() const { return mMovement; }
		Components::Weapon* GetWeapon()   const { return mWeapon; }

		// verb
		void DoForward(vmath::vec2 dir, float dt) override { if (mMovement) mMovement->DoForward(dir, dt); } // IMovable
		void Dash(vmath::vec2 dir)   { DoImpulse(dir); }                       // BaseEntity 기반(대시)
		void Attack(vmath::vec2 aim) { if (mWeapon) mWeapon->UseWeapon(aim); } // ranged bullet
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER__
