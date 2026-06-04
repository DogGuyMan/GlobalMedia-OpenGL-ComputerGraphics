#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_PLAYER_LIFE__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_PLAYER_LIFE__

#include "Entity/Components/LifeComponents.h"
#include "Entity/BaseEntity.h"   // GetComponent<BaseEntity> + Timers() 위탁 (완전형)
#include "timer/timer.h"         // SJH::Timer::Timer* 핸들

namespace TopdownShooter::Entity::Components
{
	/// @brief 플레이어 전용 Life — Enemy 와 공유하는 base Life 에서 분리(무적시간 공유 방지 + 회복 확장).
	/// @details
	///   - **무적시간(i-frame)**: 기본 0.8초. ctor 에서 mIFrameSeconds 주입 → base Life::OnEnter 가
	///     "life.iframe" 타이머를 등록하므로 DoDamaged 의 IsInvincible early-return 이 실효된다.
	///     (base Life 는 0 = 무적 없음. Enemy 는 base 라 무적 미공유 — 사용자 요구사항.)
	///   - **자동 회복**: 0.5초마다 +1 HP (= 초당 2). MaxHp 캡.
	///     **한 번이라도 사망(CurHp 가 음수 도달 → mDeathFxFired latch)하면 회복 영구 정지**
	///     — 부활/언데드 회복 금지.
	///   - **회복 지연(Halo 식 lockout)**: 피격(실데미지)하면 mRegenDelaySeconds(기본 3초) 동안 회복 보류.
	///     "life.regenDelay" 타이머 arm-inactive 패턴(평소 IsTimesUp=true=회복 허용 / DoDamaged 시 Reset →
	///     지연창 동안 IsTimesUp=false=보류). 무적에 막힌 피격은 지연을 발동시키지 않는다.
	///   - GetComponent<Life> 는 typeid 미스 시 slow-path dynamic_cast 로 본 파생을 그대로 찾으므로
	///     BaseEntity / Carrier 의 `GetComponent<Components::Life>()` 는 무수정으로 동작한다.
	class PlayerLifeComponent : public Life
	{
	  public:
		explicit PlayerLifeComponent(int max_hp, int cur_hp = -1)
		    : Life(max_hp, cur_hp)
		{
			mIFrameSeconds = 0.25f;   // 플레이어 특수화 무적시간 (base 기본 0 = 무적 없음)
		}

		/// @brief 자동 회복 튜닝 (기본 0.5초마다 +1 = 초당 2). interval<=0 또는 amount<=0 이면 회복 비활성.
		PlayerLifeComponent &SetRegen(float intervalSeconds, int hpPerTick)
		{
			mRegenInterval = intervalSeconds;
			mRegenAmount   = hpPerTick;
			return *this;
		}
		/// @brief 회복 지연(Halo lockout) 길이 재설정 (기본 3초). <=0 이면 지연 없음(즉시 회복).
		PlayerLifeComponent &SetRegenDelay(float seconds)
		{
			mRegenDelaySeconds = seconds;
			return *this;
		}
		/// @brief i-frame 길이 재설정 (Life::SetIFrameSeconds 와 동일하나 PlayerLifeComponent& 반환 — fluent 체이닝).
		PlayerLifeComponent &SetIFrame(float seconds)
		{
			mIFrameSeconds = seconds;
			return *this;
		}

		void OnEnter() override
		{
			Life::OnEnter();   // iframe(>0)/die 타이머 등록 + sink 캐시
			auto *owner = GetOwner();
			if (owner == nullptr) return;
			auto *be = owner->GetComponent<BaseEntity>();
			if (be == nullptr) return;
			if (mRegenInterval > 0.0f && mRegenAmount > 0)
				// 0 에서 count-up (arm 없음) — 첫 회복 = +interval 후. base iframe/die 와 키 충돌 없음.
				mRegenTimer = be->Timers().Register("life.regen", mRegenInterval);
			if (mRegenDelaySeconds > 0.0f)
			{
				// arm-inactive: Tick(base)로 즉시 만료 = 평소 회복 허용. DoDamaged 시 Reset → 지연창 발동.
				mRegenDelayTimer = be->Timers().Register("life.regenDelay", mRegenDelaySeconds);
				mRegenDelayTimer->Tick(mRegenDelayTimer->GetBaseTime());
			}
		}

		void OnExit() override
		{
			if (auto *owner = GetOwner())
				if (auto *be = owner->GetComponent<BaseEntity>())
				{
					be->Timers().Unregister("life.regen");
					be->Timers().Unregister("life.regenDelay");
				}
			mRegenTimer      = nullptr;
			mRegenDelayTimer = nullptr;
			Life::OnExit();    // iframe/die Unregister + 핸들 정리
		}

		void DoInvincible() { mInvincibleTimer->Reset();}

		void DoDamaged(int damage) override
		{
			const bool wasInvincible = IsInvincible();   // base 가 무적이면 early-return → 실데미지 0
			Life::DoDamaged(damage);                     // HP 차감 + iframe Reset + 사망 처리
			if (!wasInvincible && mRegenDelayTimer)      // 실제로 맞았을 때만 회복 지연(Halo lockout) 발동
				mRegenDelayTimer->Reset();
		}

		void Update(float dt) override
		{
			Life::Update(dt);              // 사망 연출 진행 / 안전망(IsAlive 0 시 DoDie)
			if (mDeathFxFired) return;     // ★ 한 번이라도 사망하면 회복 영구 금지 (latch)
			// 피격 후 회복 지연창(Halo lockout) — 아직 안 지났으면 회복 틱을 fresh 로 눌러두고 보류.
			if (mRegenDelayTimer && !mRegenDelayTimer->IsTimesUp())
			{
				if (mRegenTimer) mRegenTimer->Reset();
				return;
			}
			if (mRegenTimer == nullptr) return;
			if (mRegenTimer->IsTimesUp())  // interval 경과 (중앙 tick 은 BaseEntity::Update 가 구동)
			{
				mRegenTimer->Reset();      // 반복 틱
				const int maxHp = GetMaxHp();
				if (mCurHp < maxHp)
					mCurHp = (mCurHp + mRegenAmount > maxHp) ? maxHp : mCurHp + mRegenAmount;
			}
		}

	  private:
		SJH::Timer::Timer *mRegenTimer        = nullptr;   // 회복 틱 타이머 (BaseEntity MultipleTimer 위탁, 비소유)
		SJH::Timer::Timer *mRegenDelayTimer   = nullptr;   // 피격 후 회복 지연(Halo lockout) (위탁, 비소유)
		float              mRegenInterval     = 0.5f;      // 회복 틱 간격(초)
		int                mRegenAmount       = 1;         // 틱당 +HP (0.5s 간격 * +1 = 초당 2)
		float              mRegenDelaySeconds = 3.0f;      // 피격 후 회복 보류 시간(초, Halo 식)
	};
}; // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_PLAYER_LIFE__
