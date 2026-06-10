/**
 * @file LifeComponents.h
 * @brief HP/무적/사망 연출을 담당하는 @c Life Component 선언.
 *
 * @details
 *  ### 책임
 *  - HP 보유(@c Algebraic::Numeric::Stat) + 피격(@c DoDamaged) + 사망(@c DoDie) 처리.
 *  - i-frame(무적 시간) + 사망 연출 지연 타이머를 @c BaseEntity::Timers() 에 위탁 관리.
 *  - 연출 seam: @c mOnHitFx / @c mOnDeathFx / @c mOnDamageNumber / @c mOnDeath 콜백.
 *  - @c IActorPresentation 싱크(@c mSink)를 통해 스프라이트/FX 연출 레이어로 forward.
 *
 *  ### 비-책임
 *  - [X] 타이머 tick 구동 - @c BaseEntity::Update 가 @c MultipleTimer 를 일괄 tick.
 *  - [X] 회복(regen) - @c PlayerLifeComponent 에서 확장(LifeComponents.h 의 base 는 무회복).
 *  - [X] Actor 씬 제거 - WaveController 가 @c mOnDeath 옵저버 콜백을 받아 deferred sweep 처리.
 *
 *  ### 정통 매핑
 *  - Cocos2D `HealthComponent` / Unity `IDamageable` 패턴. Actor 비상속, Component 위임.
 *
 * @note @c mInvincibleTimer / @c mDieTimer 는 비소유 포인터 - @c OnEnter 에서 등록,
 *       @c OnExit 에서 Unregister 후 nullptr 로 초기화. Owner @c BaseEntity 가 lifetime 보유.
 */
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
    /**
     * @brief HP/i-frame/사망을 담당하는 기본 생명력 컴포넌트.
     * @details
     *  @c ILivable / @c IDieable / @c IDamageable 세 인터페이스를 다중 구현.
     *  피격 -> i-frame 활성 -> 사망 -> (선택적) 지연 후 비활성 흐름을 내부에서 처리한다.
     *
     *  타이머 위탁 패턴: @c mInvincibleTimer / @c mDieTimer 는 @c BaseEntity::Timers() 에 등록,
     *  핸들(포인터)만 보유(비소유). tick 은 @c BaseEntity::Update 가 일괄 구동.
     *  @c OnEnter 에서 Register + arm-inactive(@c Tick(base) 로 즉시 만료 상태로 시작 = 평소 비활성,
     *  @c Reset 호출 시 발동). @c OnExit 에서 Unregister + nullptr 초기화.
     *
     *  연출 seam 주입은 Builder fluent 체인(@c SetOnHitFx / @c SetOnDeathFx 등)으로 수행.
     *  스프라이트/FX 타입은 0개 노출 - 게임플레이 레이어가 연출 구현을 알 필요 없다.
     *
     * @note Enemy 는 base @c Life 를 사용(무적 시간 없음). 플레이어는 @c PlayerLifeComponent 로 확장.
     */
	class Life : public SJH::Scene::Component,
	             public ILivable,
	             public IDieable,
	             public IDamageable
	{
	  protected:
		Algebraic::Numeric::Stat mMaxHp;          ///< 최대 HP Stat (modifier 확장 가능).
		int   mCurHp;                              ///< 현재 HP (0 이하 = 사망).
		bool  mDeathFxFired = false;               ///< one-shot 사망 guard (@c mDead 대체). DoDie 중복 발동 방지.
		std::function<void(const vmath::vec3 &)> mOnDeathFx;          ///< 사망 위치에 spawn-at-point FX seam (빌더 주입).
		std::function<void(const vmath::vec3 &)> mOnHitFx;            ///< 피격 위치에 hit FX seam - 적/플레이어 공통 (빌더 주입).
		std::function<void(int, const vmath::vec3 &)> mOnDamageNumber; ///< 피격 위치에 데미지 숫자 seam (damage+pos, 빌더가 WorldText::SpawnDamage 주입).
		std::function<void(SJH::Scene::Actor *)> mOnDeath;             ///< 사망(HP0) observer 통지 seam - WaveController 가 count-down + deferred sweep 수행.
		IActorPresentation *mSink = nullptr;                           ///< @c OnEnter 에서 1회 캐시하는 연출 sink (비소유).

		// === Timer 중앙화 -- BaseEntity 의 MultipleTimer 에 위탁, 핸들만 보유 (비소유) ===
		// "없음(미설정)"은 등록 안 함 = nullptr. 설정값(초)은 OnEnter 전(빌더)에 들어오므로 float 보관 후
		// OnEnter 에서 Register + arm-inactive(Tick(base)로 finished 시작 -- 평소 비활성, Reset 시 발동).
		SJH::Timer::Timer* mInvincibleTimer = nullptr; ///< i-frame 타이머 핸들 (nullptr = 무적 없음, 비소유).
		SJH::Timer::Timer* mDieTimer        = nullptr; ///< 사망 연출 지연 타이머 핸들 (nullptr = 즉시 비활성, 비소유).
		float              mIFrameSeconds   = 0.0f;    ///< i-frame 길이 설정값(초). >0 일 때 OnEnter 에서 등록.
		float              mDieDelaySeconds = 0.0f;    ///< 사망 연출 지연 설정값(초). >0 일 때 OnEnter 에서 등록.

	  public:
		/// @brief 기본 생성 (HP=0, 무적 없음). 이후 빌더 fluent 로 값 설정 필요.
		Life()
		    : mMaxHp(0.0f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MaxHp), mCurHp(0)
		{
		}

		/// @brief 최대/현재 HP 설정 생성.
		/// @param max_hp 최대 HP.
		/// @param cur_hp 초기 현재 HP. -1 이면 @p max_hp 와 동일하게 초기화.
		Life(int max_hp, int cur_hp = -1)
		    : mMaxHp(static_cast<float>(max_hp), Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MaxHp),
		      mCurHp(cur_hp == -1 ? max_hp : cur_hp)
		{
		}

		/// @brief 최대/현재 HP + i-frame 길이를 한 번에 설정하는 생성자.
		/// @param max_hp  최대 HP.
		/// @param cur_hp  초기 현재 HP. -1 이면 @p max_hp 와 동일.
		/// @param iframe  무적 시간(초). >0 이면 @c OnEnter 에서 타이머 등록.
		Life(int max_hp, int cur_hp, float iframe)
		    : mMaxHp(static_cast<float>(max_hp), Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MaxHp),
		      mCurHp(cur_hp == -1 ? max_hp : cur_hp)
		{
			mIFrameSeconds = iframe;   // OnEnter 에서 >0 일 때만 i-frame Timer 등록
		}

		/// @brief i-frame 길이 설정 fluent. @param s 무적 시간(초, >0). @return *this.
		Life &SetIFrameSeconds(float s) { mIFrameSeconds = s; return *this; }
		/// @brief 사망 위치 FX seam 주입 fluent. @param fx 사망 spawn-at-point 콜백. @return *this.
		Life &SetOnDeathFx(std::function<void(const vmath::vec3 &)> fx) { mOnDeathFx = std::move(fx); return *this; }
		/// @brief 피격 위치 FX seam 주입 fluent. @param fx hit FX 콜백. @return *this.
		Life &SetOnHitFx(std::function<void(const vmath::vec3 &)> fx) { mOnHitFx = std::move(fx); return *this; }
		/// @brief 데미지 숫자 seam 주입 fluent. @param fn (damage, pos) 콜백. @return *this.
		Life &SetOnDamageNumber(std::function<void(int, const vmath::vec3 &)> fn) { mOnDamageNumber = std::move(fn); return *this; }
		/// @brief 사망 observer 콜백 주입 fluent (WaveController count-down 등). @param fn Actor* 콜백. @return *this.
		Life &SetOnDeath(std::function<void(SJH::Scene::Actor *)> fn) { mOnDeath = std::move(fn); return *this; }
		/// @brief 사망 연출 지연 설정 fluent. @param s 지연 시간(초, >0). @return *this.
		Life &SetDeathDelaySeconds(float s) { mDieDelaySeconds = s; return *this; }
		/// @brief 현재 무적 상태인지 반환. i-frame 타이머가 유효하고 아직 만료되지 않으면 true.
		bool  IsInvincible() const { return mInvincibleTimer && !mInvincibleTimer->IsTimesUp(); }
		/// @brief 디졸브(사망 연출) 종료 = despawn 가능 시점. WaveController deferred sweep 의 RemoveChild 게이트.
		bool  IsDespawnReady() const { return mDeathFxFired && (mDieTimer == nullptr || mDieTimer->IsTimesUp()); }

		/// @brief 씬 진입 시 초기화 - sink 캐시 + i-frame/die 타이머를 BaseEntity 에 등록.
		/// @details @c mIFrameSeconds > 0 이면 "life.iframe", @c mDieDelaySeconds > 0 이면 "life.die"
		///          타이머를 @c BaseEntity::Timers() 에 등록하고 arm-inactive 상태로 설정.
		void OnEnter() override
		{
			auto* owner = GetOwner();
			if (owner == nullptr) return;
			// 핫패스 sink 1회 해소 (ctor 금지 - GetOwner null + dynamic type=base). 없으면 silent no-op.
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
		/// @brief 씬 이탈 시 정리 - i-frame/die 타이머 Unregister + 핸들 nullptr 초기화.
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

		/// @brief 프레임 갱신 - 사망 연출 진행 감시 + 안전망(외부 HP 조작으로 0 이 됐을 때 DoDie 발화).
		/// @details i-frame/die tick 은 @c BaseEntity::Update 가 일괄 구동하므로 여기서는 만료 read 만.
		/// @param dt 직전 프레임 경과 시간(초, 내부 미사용 - 타이머 tick 은 BaseEntity 담당).
		void Update(float /*dt*/) override
		{
			// i-frame/die tick 은 BaseEntity::Update 가 일괄 구동 - 여기선 만료 read 만.
			if (mDeathFxFired && mDieTimer)   // 사망 연출 진행 중 (지연 등록 + 죽음 발화됨)
			{
				if (mDieTimer->IsTimesUp() && GetOwner()) GetOwner()->SetActive(false);
				return;
			}
			// 안전망 - DoDamaged 외 경로(직접 mCurHp 조작 등)로 죽었어도 death 1회 발화.
			if (!mDeathFxFired && !IsAlive()) DoDie();
		}

		/// @brief HP > 0 이면 살아 있음. @return 현재 HP > 0 이면 true.
		bool IsAlive() const override { return 0 < mCurHp; }
		/// @brief 현재 HP 반환. @return 현재 체력.
		int  GetHp() const override { return mCurHp; }
		/// @brief 최대 HP 반환. @return Stat 기반 최대 체력.
		int  GetMaxHp() const override { return static_cast<int>(mMaxHp.GetValue()); }

		/// @brief 데미지 수신 - HP 차감 + i-frame 활성 + 사망 판정.
		/// @details 무적(@c IsInvincible) 상태이면 전체 skip. 실데미지가 가해지면
		///          @c mSink->ReactDamaged, @c mOnHitFx, @c mOnDamageNumber seam 발동.
		///          HP 0 이하 시 @c DoDie 연결.
		/// @param damage 적용할 데미지 양.
		void DoDamaged(int damage) override
		{
			if (IsInvincible()) return;              // i-frame early-return (총알+접촉 모두 보호)
			mCurHp -= damage;
			if (mSink) mSink->ReactDamaged(damage);  // Template-Method forward
			if (mOnHitFx)                            // 피격 위치에 hit FX (mOnDeathFx 대칭 seam - 빌더가 VFX::Spawn 주입)
				mOnHitFx(GetOwner() ? GetOwner()->GetTransform().Translate : vmath::vec3(0.0f));
			if (mOnDamageNumber)                     // 피격 위치에 데미지 숫자 (빌더가 WorldText::SpawnDamage 주입)
				mOnDamageNumber(damage, GetOwner() ? GetOwner()->GetTransform().Translate : vmath::vec3(0.0f));
			if (mInvincibleTimer) mInvincibleTimer->Reset();   // passed=0 -> 무적 발동 (없으면 no-op = 무적 없음)
			if (!IsAlive())
			{
				mCurHp = 0;
				DoDie();
			}
		}

		/// @brief 사망 처리 발동 - one-shot 가드 후 연출 seam + observer 통지 + Actor 비활성.
		/// @details @c mDeathFxFired latch 로 중복 호출 방지. @c mDieTimer 설정 시 지연 비활성,
		///          미설정 시 즉시 @c Actor::SetActive(false).
		void DoDie() override
		{
			if (mDeathFxFired) return;               // one-shot
			mDeathFxFired = true;
			const vmath::vec3 pos = GetOwner() ? GetOwner()->GetTransform().Translate : vmath::vec3(0.0f);
			if (mSink) mSink->ReactDied(pos);        // 디졸브 시작 (sink 가 구동 - 분해 Task 6)
			if (mOnDeathFx) mOnDeathFx(pos);         // spawn-at-point seam
			if (mOnDeath) mOnDeath(GetOwner());      // 사망 통지(observer) - countv + 제거 큐 등록은 owner(WaveController)
			if (mDieTimer)
				mDieTimer->Reset();                  // 지연 발동 - Update 가 만료 시 비활성 (mDeathFxFired 가 게이트)
			else if (GetOwner())
				GetOwner()->SetActive(false);        // 즉시 (기본/현행)
		}
	};
}; // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__
