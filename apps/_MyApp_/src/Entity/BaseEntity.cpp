/**
 * @file BaseEntity.cpp
 * @brief BaseEntity 의 형제 Component 캐시(OnEnter/OnExit)와 인터페이스 위임 구현.
 * @details 헤더가 피하던 Playable/Physics 완전형을 여기서만 포함 - GetComponent/FindPhysics/Play 호출에 필요.
 */
#include "Entity/BaseEntity.h"

#include "Entity/Components/LifeComponents.h"
#include "Physics/PhysicsComponent.h"        // Components::Physics + FindPhysics
#include "Physics/PhysicsImpulse.h"          // Impulse (IsActive)
#include "Playable/PlayableDirector.h"       // 완전형 — GetComponent/Play

namespace TopdownShooter::Entity
{
	// 형제 Component 4종을 owner Actor 에서 조회해 비소유 캐시. owner 없으면 캐시 생략.
	void BaseEntity::OnEnter()
	{
		auto* owner = GetOwner();
		if (owner == nullptr) return;
		mLife     = owner->GetComponent<Components::Life>();
		mPhysics  = Physics::Components::FindPhysics(owner);   // Physics 추상의 concrete 바디 탐색
		mDirector = owner->GetComponent<Playable::PlayableDirector>();
		mImpulse  = owner->GetComponent<Physics::Impulse>();
	}

	// 씬 이탈 시 캐시 포인터 무효화 - dangling 참조 방지.
	void BaseEntity::OnExit()
	{
		mLife = nullptr; mPhysics = nullptr; mDirector = nullptr; mImpulse = nullptr;
	}

	// 이하 ILivable/IDieable/IDamageable 위임 — 형제 미캐시(nullptr) 시 안전한 기본값 반환.
	bool BaseEntity::IsAlive()  const { return mLife && mLife->IsAlive(); }
	int  BaseEntity::GetHp()    const { return mLife ? mLife->GetHp()    : 0; }
	int  BaseEntity::GetMaxHp() const { return mLife ? mLife->GetMaxHp() : 0; }
	void BaseEntity::DoDamaged(int damage) { if (mLife) mLife->DoDamaged(damage); }
	void BaseEntity::DoDie()               { if (mLife) mLife->DoDie(); }

	// IImpulsable 위임 — 넉백/대시 버스트.
	void BaseEntity::DoImpulse(vmath::vec2 dir) { if (mImpulse) mImpulse->DoImpulse(dir); }
	bool BaseEntity::IsImpulseActive() const   { return mImpulse && mImpulse->IsActive(); }

	// named playable 재생 위임 — Director 미캐시 시 무시.
	void BaseEntity::Play(const std::string& key) { if (mDirector) mDirector->Play(key); }
} // namespace TopdownShooter::Entity
