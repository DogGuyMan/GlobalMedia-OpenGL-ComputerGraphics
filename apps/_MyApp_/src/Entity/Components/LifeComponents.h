#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__

#include "Algebraic/Stat.h"
#include "Components.Interfaces.h"
#include "Entity/BaseEntity.h"   // BaseEntity::Timers() (MultipleTimer 등록 위탁)
#include "scene/actor.h"
#include "timer/timer.h"
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
		bool  mDeathFxFired = false;  // one-shot death guard (mDead 대체)
		std::function<void(const vmath::vec3 &)> mOnDeathFx;   // 사망 spawn-at-point seam
		std::function<void(const vmath::vec3 &)> mOnHitFx;     // 피격 spawn-at-point seam (hit FX — 적/플레이어 공통)
		IActorPresentation *mSink = nullptr;                   // OnEnter 1회 캐시

		// === Timer 중앙화 — BaseEntity 의 MultipleTimer 에 위탁, 핸들만 보유 (비소유) ===
		// "없음(미설정)"은 등록 안 함 = nullptr. 설정값(초)은 OnEnter 전(빌더)에 들어오므로 float 보관 후
		// OnEnter 에서 Register + arm-inactive(Tick(base)로 finished 시작 — 평소 비활성, Reset 시 발동).
		SJH::Timer::Timer* mInvincibleTimer = nullptr;   // i-frame    (nullptr = 무적 없음)
		SJH::Timer::Timer* mDieTimer        = nullptr;   // 사망 연출 지연 (nullptr = 즉시)
		float              mIFrameSeconds   = 0.0f;       // 등록 대기 설정값 (>0 시 OnEnter 등록)
		float              mDieDelaySeconds = 0.0f;       // 등록 대기 설정값 (>0 시 OnEnter 등록)

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
			mIFrameSeconds = iframe;   // OnEnter 에서 >0 일 때만 i-frame Timer 등록
		}

		Life &SetIFrameSeconds(float s) { mIFrameSeconds = s; return *this; }
		Life &SetOnDeathFx(std::function<void(const vmath::vec3 &)> fx) { mOnDeathFx = std::move(fx); return *this; }
		Life &SetOnHitFx(std::function<void(const vmath::vec3 &)> fx) { mOnHitFx = std::move(fx); return *this; }
		Life &SetDeathDelaySeconds(float s) { mDieDelaySeconds = s; return *this; }
		bool  IsInvincible() const { return mInvincibleTimer && !mInvincibleTimer->IsTimesUp(); }

		void OnEnter() override
		{
			auto* owner = GetOwner();
			if (owner == nullptr) return;
			// 핫패스 sink 1회 해소 (ctor 금지 — GetOwner null + dynamic type=base). 없으면 silent no-op.
			mSink = owner->GetComponent<IActorPresentation>();
			// i-frame/die timer 를 BaseEntity 중앙 컨테이너에 등록 (>0 일 때만) + arm-inactive.
			auto* be = owner->GetComponent<BaseEntity>();
			if (be == nullptr) return;
			if (mIFrameSeconds > 0.0f)
			{
				mInvincibleTimer = be->Timers().Register("life.iframe", mIFrameSeconds);
				mInvincibleTimer->Tick(mInvincibleTimer->GetBaseTime());
			}
			if (mDieDelaySeconds > 0.0f)
			{
				mDieTimer = be->Timers().Register("life.die", mDieDelaySeconds);
				mDieTimer->Tick(mDieTimer->GetBaseTime());
			}
		}
		void OnExit() override
		{
			if (auto* owner = GetOwner())
				if (auto* be = owner->GetComponent<BaseEntity>())
				{
					be->Timers().Unregister("life.iframe");
					be->Timers().Unregister("life.die");
				}
			mInvincibleTimer = nullptr;
			mDieTimer        = nullptr;
		}

		void Update(float /*dt*/) override
		{
			// i-frame/die tick 은 BaseEntity::Update 가 일괄 구동 — 여기선 만료 read 만.
			if (mDeathFxFired && mDieTimer)   // 사망 연출 진행 중 (지연 등록 + 죽음 발화됨)
			{
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
			if (mOnHitFx)                            // 피격 위치에 hit FX (mOnDeathFx 대칭 seam — 빌더가 VFX::Spawn 주입)
				mOnHitFx(GetOwner() ? GetOwner()->GetTransform().Translate : vmath::vec3(0.0f));
			if (mInvincibleTimer) mInvincibleTimer->Reset();   // passed=0 -> 무적 발동 (없으면 no-op = 무적 없음)
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
