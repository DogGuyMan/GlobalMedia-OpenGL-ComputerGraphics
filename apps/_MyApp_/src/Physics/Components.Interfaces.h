#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENTS_INTERFACES__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENTS_INTERFACES__

#include "scene/actor.h"
#include <vmath.h>

namespace TopdownShooter::Physics
{
	class IContactable
	{
	  protected:
		IContactable() = default;

	  public:
		virtual ~IContactable() = default;
		IContactable(const IContactable &) = delete;
		IContactable operator=(const IContactable &) = delete;
		IContactable(IContactable &&) = delete;
		IContactable operator=(IContactable &) = delete;

		virtual bool GetIsTrigger() { return false; }   // 단일 source-of-truth — Physics 베이스가 final override
		virtual void OnTriggerEnter(SJH::Scene::Actor *other)   = 0;
		virtual void OnTriggerExit(SJH::Scene::Actor *other)    = 0;
		virtual void OnCollisionEnter(SJH::Scene::Actor *other) = 0;
		virtual void OnCollisionExit(SJH::Scene::Actor *other)  = 0;
	};
}; // namespace TopdownShooter::Physics

#endif //_TOPDOWNSHOOTER_PHYSICS_COMPONENTS_LIFE__