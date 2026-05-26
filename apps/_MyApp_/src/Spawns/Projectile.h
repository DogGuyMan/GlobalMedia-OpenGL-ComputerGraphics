#ifndef _TOPDOWNSHOOTER_SPAWN_CARRIER_COMPONENTS__
#define _TOPDOWNSHOOTER_SPAWN_CARRIER_COMPONENTS__
#include "Entity/Components/Components.Interfaces.h"
#include "Physics/Components.Interfaces.h"
#include "Physics/PhysicsComponent.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include <box2d/b2_body.h>
#include <cstddef>
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::Spawn::Carrier
{
	class Projectile : public SJH::Scene::Component,
	                   public Physics::IContactable,
	                   public Entity::IDieable
	{
	  private:
		SJH::Scene::Actor *mOwnerEntity = nullptr;
		float mDamageInfo = 0.0f;
		bool isInitialized = false;

	  public:
		Projectile() = default;

		Projectile &SetOwnerEntity(SJH::Scene::Actor *owner)
		{
			mOwnerEntity = owner;
			return *this;
		}

		Projectile &SetDamage(float damage)
		{
			mDamageInfo = damage;
			return *this;
		}

		Projectile &SetUp()
		{
			isInitialized = true;
			return *this;
		}

		virtual void OnEnter() override
		{
		}
		virtual void OnExit() override
		{
		}
		virtual void Update(float dt) override
		{
		}

		virtual void DoDie() override
		{
		}
		
		virtual void OnTriggerEnter(SJH::Scene::Actor *other) override
		{
			if(!other->IsActive()) return;
		}

		virtual void OnTriggerExit(SJH::Scene::Actor *other) override
		{

		}

		virtual void OnCollisionEnter(SJH::Scene::Actor *other) override
		{
			return;
		}

		virtual void OnCollisionExit(SJH::Scene::Actor *other) override
		{
			return;
		}
	};
}; // namespace TopdownShooter::Spawner
#endif //_TOPDOWNSHOOTER_SPAWN_CARRIER_COMPONENTS__