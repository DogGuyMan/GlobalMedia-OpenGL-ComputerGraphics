#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__

#include "Algebraic/Stat.h"
#include "Components.Interfaces.h"
#include "scene/actor.h"
#include <functional>

namespace TopdownShooter::Entity::Components
{
	class Life : public SJH::Scene::Component,
	             public ILivable,
	             public IDieable,
	             public IDamageable
	{
	  protected:
		Algebraic::Numeric::Stat mMaxHp;
		int   mCurHp;
		float mIFrameSeconds   = 0.0f;   // PB mHitInvincibility 미러 (기본 0 = 무적 없음)
		float mInvincibleTimer = 0.0f;
		bool  mDeathFxFired    = false;  // one-shot death guard (mDead 대체)
		std::function<void(const vmath::vec3 &)> mOnDeathFx;   // spawn-at-point seam
		IActorPresentation *mSink = nullptr;                   // OnEnter 1회 캐시

		float mDeathDelaySeconds = 0.0f;   // 0 = 즉시(현행). >0 = 사망 연출(디졸브) 동안 SetActive 지연
		bool  mDying             = false;
		float mDeathTimer        = 0.0f;

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

		Life(int max_hp, int cur_hp, float iframe)
		    : mMaxHp(static_cast<float>(max_hp), Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MaxHp),
		      mCurHp(cur_hp == -1 ? max_hp : cur_hp), mIFrameSeconds(iframe)
		{
		}

		Life &SetIFrameSeconds(float s) { mIFrameSeconds = s; return *this; }
		Life &SetOnDeathFx(std::function<void(const vmath::vec3 &)> fx) { mOnDeathFx = std::move(fx); return *this; }
		Life &SetDeathDelaySeconds(float s) { mDeathDelaySeconds = s; return *this; }
		bool  IsInvincible() const { return mInvincibleTimer > 0.0f; }

		void OnEnter() override
		{
			// 핫패스 sink 1회 해소 (ctor 금지 — GetOwner null + dynamic type=base). 없으면 silent no-op.
			if (GetOwner())
				mSink = GetOwner()->GetComponent<IActorPresentation>();
		}
		void OnExit() override {}

		void Update(float dt) override
		{
			if (mInvincibleTimer > 0.0f) mInvincibleTimer -= dt;
			if (mDying)   // 사망 연출 진행 중 — 타이머 만료 시 SetActive(false)
			{
				mDeathTimer -= dt;
				if (mDeathTimer <= 0.0f && GetOwner()) GetOwner()->SetActive(false);
				return;
			}
			// 안전망 — DoDamaged 외 경로(직접 mCurHp 조작 등)로 죽었어도 death 1회 발화.
			if (!mDeathFxFired && !IsAlive()) DoDie();
		}

		bool IsAlive() const override { return 0 < mCurHp; }
		int  GetHp() const override { return mCurHp; }
		int  GetMaxHp() const override { return static_cast<int>(mMaxHp.GetValue()); }

		void DoDamaged(int damage) override
		{
			if (IsInvincible()) return;              // i-frame early-return (총알+접촉 모두 보호)
			mCurHp -= damage;
			if (mSink) mSink->ReactDamaged(damage);  // Template-Method forward
			mInvincibleTimer = mIFrameSeconds;       // arm i-frame
			if (!IsAlive())
			{
				mCurHp = 0;
				DoDie();
			}
		}

		void DoDie() override
		{
			if (mDeathFxFired) return;               // one-shot
			mDeathFxFired = true;
			const vmath::vec3 pos = GetOwner() ? GetOwner()->GetTransform().Translate : vmath::vec3(0.0f);
			if (mSink) mSink->ReactDied(pos);
			if (mOnDeathFx) mOnDeathFx(pos);         // spawn-at-point seam
			if (mDeathDelaySeconds <= 0.0f)
			{
				if (GetOwner()) GetOwner()->SetActive(false);   // 즉시 (기본/현행)
			}
			else
			{
				mDying      = true;                  // 지연 — Update 가 만료 시 비활성
				mDeathTimer = mDeathDelaySeconds;
			}
		}
	};
}; // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__
