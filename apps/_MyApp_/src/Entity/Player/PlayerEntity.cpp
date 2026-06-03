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
		//    구체 PhysicsMovement(typeid 매칭, fast-path)로 캐시 -> 자기 자신 배제.
		mMovement = owner->GetComponent<Physics::PhysicsMovement>();  // PhysicsMovement->IMovable 업캐스트
		mWeapon   = owner->GetComponent<Components::Weapon>();
		// dust interval timer 등록 — BaseEntity 중앙 컨테이너 위탁. passed=0 시작(첫 dust 는 interval 후).
		mDustTimer = Timers().Register("player.dust", mDustInterval);
	}

	void PlayerEntity::OnExit()
	{
		Timers().Unregister("player.dust"); // 컨테이너 정리 전 먼저 해제
		mDustTimer = nullptr;
		BaseEntity::OnExit();
	}
} // namespace TopdownShooter::Entity
