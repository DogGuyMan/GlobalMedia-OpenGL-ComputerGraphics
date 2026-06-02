#include "Entity/BaseEntity.h"

#include "Entity/Components/LifeComponents.h"
#include "Physics/PhysicsComponent.h"        // Components::Physics + FindPhysics
#include "Physics/PhysicsImpulse.h"          // Impulse (IsActive)
#include "Playable/PlayableDirector.h"       // 완전형 — GetComponent/Play

namespace TopdownShooter::Entity
{
	void BaseEntity::OnEnter()
	{
		auto* owner = GetOwner();
		if (owner == nullptr) return;
		mLife     = owner->GetComponent<Components::Life>();
		mPhysics  = Physics::Components::FindPhysics(owner);
		mDirector = owner->GetComponent<Playable::PlayableDirector>();
		mImpulse  = owner->GetComponent<Physics::Impulse>();
	}

	void BaseEntity::OnExit()
	{
		mLife = nullptr; mPhysics = nullptr; mDirector = nullptr; mImpulse = nullptr;
	}

	bool BaseEntity::IsAlive()  const { return mLife && mLife->IsAlive(); }
	int  BaseEntity::GetHp()    const { return mLife ? mLife->GetHp()    : 0; }
	int  BaseEntity::GetMaxHp() const { return mLife ? mLife->GetMaxHp() : 0; }
	void BaseEntity::DoDamaged(int damage) { if (mLife) mLife->DoDamaged(damage); }
	void BaseEntity::DoDie()               { if (mLife) mLife->DoDie(); }

	void BaseEntity::DoImpulse(vmath::vec2 dir) { if (mImpulse) mImpulse->DoImpulse(dir); }
	bool BaseEntity::IsImpulseActive() const   { return mImpulse && mImpulse->IsActive(); }

	void BaseEntity::Play(const std::string& key) { if (mDirector) mDirector->Play(key); }
} // namespace TopdownShooter::Entity
