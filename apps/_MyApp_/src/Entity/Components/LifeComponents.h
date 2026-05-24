#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__

#include "Algebraic/Stat.h"
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
		// MaxHp 는 Stat 으로 통합 — modifier 시스템 적용 가능 (NumericType::MaxHp, UseType::Natural).
		Algebraic::Numeric::Stat mMaxHp;
		int mCurHp;

	  public:
		Life()
		    : mMaxHp(0.0f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MaxHp), mCurHp(0)
		{
		}

		Life(int max_hp, int cur_hp = -1)
		    : mMaxHp(static_cast<float>(max_hp), Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MaxHp),
		      mCurHp(cur_hp == -1 ? max_hp : cur_hp)
		{
		}

		virtual void OnEnter() override {};
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
			return static_cast<int>(mMaxHp.GetValue());
		}
	};
}; // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__
