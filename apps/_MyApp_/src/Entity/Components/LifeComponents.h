#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__

#include "Components.Interfaces.h"
#include "scene/actor.h"

namespace TopdownShooter::Entity::Components
{
	class Life : public SJH::Scene::Component,
	             public ILivable,
	             public IDieable,
	             public IDamageable
	{
	  protected:
		int mMaxHp;
		int mCurHp;

	  public:
		Life() = default;

		Life(int max_hp, int cur_hp = -1)
		    : mMaxHp(max_hp), mCurHp(cur_hp == -1 ? max_hp : cur_hp)
		{
		}

		virtual void OnEnter() override
		{
			mMaxHp = mCurHp;
		};
		virtual void Update(float dt) override {};
		virtual void OnExit() override {};

		virtual bool IsAlive() const override
		{
			return 0 < mCurHp;
		}

		virtual void DoDie() override
		{
		}

		virtual void DoDamaged(int damage) override
		{
			mCurHp -= damage;
			if (!IsAlive())
			{
				mCurHp = 0;
				DoDie();
				return;
			}
		}
		virtual int GetHp() const override
		{
			return mCurHp;
		}
		virtual int GetMaxHp() const override
		{
			return mMaxHp;
		}
	};
}; // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__