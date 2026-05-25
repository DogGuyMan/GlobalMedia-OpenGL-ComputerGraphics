#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
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

namespace TopdownShooter::Physics::Components
{
	class CircleBody : public Physics
	{
	  public:
		virtual void OnEnter() override
		{
		}

		virtual void OnExit() override
		{
		}

		virtual void Update(float dt) override
		{
		}


		virtual void OnTriggerEnter(SJH::Scene::Actor *other) override
		{
			return;
		}
		virtual void OnTriggerExit(SJH::Scene::Actor *other) override
		{
			return;
		}
		virtual void OnCollisionEnter(SJH::Scene::Actor *other) override
		{
		}
		virtual void OnCollisionExit(SJH::Scene::Actor *other) override
		{
		}
	};

	class BoxBody : public Physics
	{
	  public:
		virtual void OnEnter() override
		{
		}
		virtual void OnExit() override
		{
		}
		virtual void Update(float dt) override
		{
		}

		virtual void OnTriggerEnter(SJH::Scene::Actor *other) override
		{
			return;
		}
		virtual void OnTriggerExit(SJH::Scene::Actor *other) override
		{
			return;
		}
		virtual void OnCollisionEnter(SJH::Scene::Actor *other) override
		{
		}
		virtual void OnCollisionExit(SJH::Scene::Actor *other) override
		{
		}
	};
}; // namespace TopdownShooter::Physics::Components

#endif //_TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__