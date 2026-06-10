/**
 * @file PlayerEntity.cpp
 * @brief PlayerEntity lifecycle 구현 — OnEnter/OnExit 에서 형제 Component 캐시 + dust timer 관리.
 *
 * @details
 *  ### 책임
 *  - @c OnEnter: BaseEntity 공통 4캐시(super) 수행 후 @c PhysicsMovement / @c Weapon 추가 캐시.
 *    dust interval timer 를 "player.dust" 키로 @c BaseEntity::Timers() 에 등록.
 *  - @c OnExit: "player.dust" timer 를 먼저 Unregister 해 dangling 핸들 방지, 이후 super OnExit.
 *
 *  ### 비-책임
 *  - [X] 이동/발사/대시 로직 — 헤더 인라인 verb 가 담당. 이 파일에는 lifecycle 만 존재.
 *
 * @note PhysicsMovement 를 IMovable 인터페이스가 아닌 구체 타입으로 캐시하는 이유:
 *       PlayerEntity 자신이 IMovable 을 구현하므로, GetComponent<IMovable>() 호출 시
 *       자기 자신이 매칭될 수 있다. typeid 기반 구체 타입 조회로 자기 매칭을 배제한다.
 */
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
