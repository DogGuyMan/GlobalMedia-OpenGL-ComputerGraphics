#ifndef _TOPDOWNSHOOTER_ENTITY_BASE__
#define _TOPDOWNSHOOTER_ENTITY_BASE__

#include "Components/Components.Interfaces.h"
#include "Components/LifeComponents.h"
#include "Components/MovementComponents.h"
#include "scene/actor.h"
#include <string.h>

namespace TopdownShooter::Entity
{
	class BaseEntity : public SJH::Scene::Component,
	                   public ILivable,
	                   public IDieable,
	                   public IDamageable
	{
	  protected:

		Components::Life* mLifeComponentPtr;
		BaseEntity(int hp, const char *name);

	  public:
		virtual ~BaseEntity();
		virtual std::string GetName() const
		{
			return GetOwner()->GetName();
		}
		// virtual ostream &GetHpState(ostream &) const;

		virtual bool IsAlive() const override {return mLifeComponentPtr->IsAlive();}
		virtual void DoDie() override { mLifeComponentPtr->DoDie(); }
		virtual void DoDamaged(int damage) override { mLifeComponentPtr->DoDamaged(damage); }
		virtual int GetHp() const override {return mLifeComponentPtr->GetHp();}
		virtual int GetMaxHp() const override {return mLifeComponentPtr->GetMaxHp();}
	};
} // namespace TopdownShooter::Entity

#endif//_TOPDOWNSHOOTER_ENTITY_BASE__
