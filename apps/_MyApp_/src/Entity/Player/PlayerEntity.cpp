#include "Entity/Player/PlayerEntity.h"

#include "Physics/PhysicsMovement.h"   // PhysicsMovement (IMovable 구현체 — 캐시 대상)
#include "scene/actor.h"

namespace TopdownShooter::Entity
{
	void PlayerEntity::OnEnter()
	{
		BaseEntity::OnEnter();   // Life/Physics/Director/Impulse 캐시
		auto* owner = GetOwner();
		if (owner == nullptr) return;
		// ⚠ GetComponent<IMovable>(인터페이스) 금지 — PlayerEntity 자신도 IMovable 라 자기매칭 위험.
		//    구체 PhysicsMovement(typeid 매칭, fast-path)로 캐시 → 자기 자신 배제.
		mMovement = owner->GetComponent<Physics::PhysicsMovement>();  // PhysicsMovement→IMovable 업캐스트
		mWeapon   = owner->GetComponent<Components::Weapon>();
	}
} // namespace TopdownShooter::Entity
