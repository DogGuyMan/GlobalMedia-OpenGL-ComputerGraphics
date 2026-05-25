#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENT__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENT__
#include "Physics/Components.Interfaces.h"
#include "input/keyboard_input.h"
#include "input/mouse_input.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include <box2d/b2_body.h>
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::Physics::Components
{
	class Physics : public SJH::Scene::Component,
	                          public IContactable
	{
	  private:
		b2Body *mBody = nullptr;
		bool mIsSensor = false;

	  public:
		Physics() = default;
		Physics &SetBody(b2Body *body)
		{
			mBody = body;
			return *this;
		};
		Physics &SetSensor(bool is_sensor)
		{
			mIsSensor = is_sensor;
			return *this;
		};

		virtual void OnEnter() = 0;
		virtual void OnExit() = 0;
		virtual void Update(float dt) = 0;

		virtual bool GetIsTrigger() final { return mIsSensor; }   // 단일 source-of-truth
		virtual void OnTriggerEnter(SJH::Scene::Actor *other) = 0;
		virtual void OnTriggerExit(SJH::Scene::Actor *other) = 0;
		virtual void OnCollisionEnter(SJH::Scene::Actor *other) = 0;
		virtual void OnCollisionExit(SJH::Scene::Actor *other) = 0;
	};
}; // namespace TopdownShooter::Physics::Components

#endif //_TOPDOWNSHOOTER_PHYSICS_COMPONENT__