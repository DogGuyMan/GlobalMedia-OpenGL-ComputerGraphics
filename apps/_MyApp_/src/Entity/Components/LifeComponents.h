#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__

#include "Algebraic/Stat.h"
#include "Components.Interfaces.h"
#include "scene/actor.h"
#include "timer/timer.h"
#include <functional>
#include <optional>

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
		bool  mDeathFxFired = false;  // one-shot death guard (mDead 대체)
		std::function<void(const vmath::vec3 &)> mOnDeathFx;   // spawn-at-point seam
		IActorPresentation *mSink = nullptr;                   // OnEnter 1회 캐시

		// === SJH::Timer 자가 보유 (패턴 A) — "없음(미설정)"은 nullopt 로 표현 (VO 정통) ===
		// Timer 는 항상 유효한 시간값(VO). i-frame/사망지연이 "없는" 엔티티는 Timer 자체가 부재(nullopt).
		// 장전 시 Tick(base)로 finished 상태로 시작 → 평소 비활성, 피격/사망 시 Reset 으로 발동
		// (생성 직후 passed=0 이면 "스폰 즉시 무적"(§5.1)이 되므로 finished 로 막는다).
		std::optional<SJH::Timer::Timer> mInvincibleTimer;   // i-frame    (nullopt = 무적 없음)
		std::optional<SJH::Timer::Timer> mDieTimer;          // 사망 연출 지연 (nullopt = 즉시)

		/// @brief s>0 이면 Timer(s) 를 finished(비활성) 상태로 장전, s<=0 이면 부재(nullopt).
		static void ArmInactive(std::optional<SJH::Timer::Timer> &slot, float s)
		{
			if (s > 0.0f)
			{
				slot.emplace(s);
				slot->Tick(s);   // passed=base → IsTimesUp (평소 비활성; Reset 시 발동)
			}
			else
				slot.reset();
		}

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
		      mCurHp(cur_hp == -1 ? max_hp : cur_hp)
		{
			ArmInactive(mInvincibleTimer, iframe);   // iframe>0 일 때만 i-frame Timer 장전
		}

		Life &SetIFrameSeconds(float s) { ArmInactive(mInvincibleTimer, s); return *this; }
		Life &SetOnDeathFx(std::function<void(const vmath::vec3 &)> fx) { mOnDeathFx = std::move(fx); return *this; }
		Life &SetDeathDelaySeconds(float s) { ArmInactive(mDieTimer, s); return *this; }   // Task 6 가 0.5 주입 예정
		bool  IsInvincible() const { return mInvincibleTimer && !mInvincibleTimer->IsTimesUp(); }

		void OnEnter() override
		{
			// 핫패스 sink 1회 해소 (ctor 금지 — GetOwner null + dynamic type=base). 없으면 silent no-op.
			if (GetOwner())
				mSink = GetOwner()->GetComponent<IActorPresentation>();
		}
		void OnExit() override {}

		void Update(float dt) override
		{
			if (mInvincibleTimer)
				mInvincibleTimer->Tick(dt);   // 무적 진행 (IsTimesUp 도달 후엔 [0,base] clamp 로 무해)
			if (mDeathFxFired && mDieTimer)   // 사망 연출 진행 중 (지연 장전 + 죽음 발화됨)
			{
				mDieTimer->Tick(dt);
				if (mDieTimer->IsTimesUp() && GetOwner()) GetOwner()->SetActive(false);
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
			if (mInvincibleTimer) mInvincibleTimer->Reset();   // passed=0 → 무적 발동 (없으면 no-op = 무적 없음)
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
			if (mSink) mSink->ReactDied(pos);        // 디졸브 시작 (sink 가 구동 — 분해 Task 6)
			if (mOnDeathFx) mOnDeathFx(pos);         // spawn-at-point seam
			if (mDieTimer)
				mDieTimer->Reset();                  // 지연 발동 — Update 가 만료 시 비활성 (mDeathFxFired 가 게이트)
			else if (GetOwner())
				GetOwner()->SetActive(false);        // 즉시 (기본/현행)
		}
	};
}; // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__
