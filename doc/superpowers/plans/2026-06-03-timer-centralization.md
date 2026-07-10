# Timer 통일 (중앙화 + Timer 객체화) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 엔티티 timer를 `BaseEntity`의 `MultipleTimer`로 중앙화하고, `WaveController`의 raw-float timer를 `SJH::Timer::Timer` 객체로 통일한다.

**Architecture:** 패턴 E(중앙화) — `BaseEntity`가 `MultipleTimer` 값 멤버를 단일 보유하고 `Update`에서 일괄 tick; `Life`/`Impulse`/`PlayerController`는 `OnEnter`에서 `Timers().Register`로 `Timer*` 핸들만 캐시하고 `OnExit`에서 `Unregister`. 패턴 S(객체화) — `WaveController`는 엔티티가 아니므로 `Timer`를 직접 멤버로 보유·tick.

**Tech Stack:** C++17, `SJH::Timer::{Timer, MultipleTimer}` ([src/timer/](../../../src/timer/)), `SJH::Scene::{Actor, Component}`, CMake/Ninja.

**검증 방식:** 본 프로젝트는 `no_auto_tests` 컨벤션 — 단위 테스트 신규 작성 없음. 각 Task 검증 = **컴파일 GREEN** + 동작 동등성 추론(spec §6). 빌드: `cmake --build --preset ninja --target _MyApp_`.

**커밋 규칙:** partial commit `git commit <경로> -m "..."` (사용자 병렬 staging 보호). 커밋 메시지 Co-Authored-By **미사용**. 접두사 `[refactor]`.

**Spec:** [doc/superpowers/specs/2026-06-03-timer-centralization-design.md](../specs/2026-06-03-timer-centralization-design.md)

**구현 메모(spec 대비 정제):** 멤버 핸들은 churn 최소화를 위해 **기존 이름을 그대로** 쓰되 타입만 `Timer*`로 바꾼다 (`mInvincibleTimer`/`mDieTimer`/`mActiveTimer`/`mCooldownTimer`/`mAttackTimer`). `optional`과 `Timer*`는 `if (ptr)` / `ptr->` 문법이 동일해 `DoDamaged`/`DoDie`/`IsInvincible` 등은 무변경.

---

### Task 0: 사전 상태 확인 (configure + baseline build)

**Files:** (없음 — 읽기/빌드만)

- [ ] **Step 1: 현재 빌드 GREEN 확인 (baseline)**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -15`
Expected: 빌드 성공 (`[N/N]` 완료, error 0). 실패 시 먼저 `cmake --preset ninja`로 configure.

- [ ] **Step 2: working tree 상태 확인 (사용자 병렬 staging 파악)**

Run: `git status --short`
Expected: 변경 목록 확인. 본 작업 7개 파일 외 변경은 건드리지 않는다.

---

### Task 1: BaseEntity — MultipleTimer 멤버 + Update tick + accessor

**Files:**
- Modify: `apps/_MyApp_/src/Entity/BaseEntity.h`

- [ ] **Step 1: include 추가**

`#include "scene/actor.h"` 줄 아래에 추가:

```cpp
#include "scene/actor.h"
#include "timer/multiple_timer.h"   // MultipleTimer (값 멤버 — 완전형 필요)
#include <string>
```

- [ ] **Step 2: `Update` 본문에 tick 구동 + `Timers()` accessor 추가**

기존:
```cpp
		void OnEnter() override;   // 4캐시 (.cpp — GetComponent/FindPhysics 완전형 필요)
		void OnExit()  override;
		void Update(float /*dt*/) override {}   // 로직 없음 — 형제가 자기 Update 보유
```
변경:
```cpp
		void OnEnter() override;   // 4캐시 (.cpp — GetComponent/FindPhysics 완전형 필요)
		void OnExit()  override;
		void Update(float dt) override { mTimers.Update(dt); }   // 중앙 tick 구동 (형제 timer 일괄)

		// ── 중앙 Timer 컨테이너 (엔티티 timer 단일 보유처) ──
		SJH::Timer::MultipleTimer&       Timers()       { return mTimers; }
		const SJH::Timer::MultipleTimer& Timers() const { return mTimers; }
```

- [ ] **Step 3: 멤버 추가 (protected 블록 끝)**

기존 protected 마지막 멤버:
```cpp
		Physics::Impulse*                    mImpulse  = nullptr;   // Player+Enemy 부착. null-guard=비엔티티 바디 대비
```
바로 아래에 추가:
```cpp
		Physics::Impulse*                    mImpulse  = nullptr;   // Player+Enemy 부착. null-guard=비엔티티 바디 대비
		SJH::Timer::MultipleTimer            mTimers;               // ★ 엔티티 timer 유일 보유 (형제가 Register 위탁)
```

- [ ] **Step 4: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -15`
Expected: GREEN (이 시점엔 Life/Impulse가 여전히 자기 timer 보유 — 무변경 동작, mTimers는 빈 컨테이너라 tick no-op).

- [ ] **Step 5: 커밋**

```bash
git commit apps/_MyApp_/src/Entity/BaseEntity.h -m "[refactor] : BaseEntity 에 MultipleTimer 단일 보유 + Update 일괄 tick + Timers() accessor (timer 중앙화 foundation)"
```

---

### Task 2: Impulse — Timer 멤버 → 중앙 등록 핸들

**Files:**
- Modify: `apps/_MyApp_/src/Physics/PhysicsImpulse.h`

- [ ] **Step 1: include 추가**

`#include "timer/timer.h"` 줄을 BaseEntity include로 보강 (timer.h는 BaseEntity.h 경유 전파되지만 명시 유지 가능):

```cpp
#include "apps/_MyApp_/src/Entity/BaseEntity.h"   // Entity::BaseEntity::Timers() (MultipleTimer 등록 위탁)
#include "scene/actor.h"
#include "timer/timer.h"   // SJH::Timer::Timer (header-only)
```

- [ ] **Step 2: ctor 에서 Timer 멤버 초기화/arm 제거**

기존:
```cpp
		Impulse()
		    : mImpulseForce(IMPULSE_FORCE, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
		      mCooldown(IMPULSE_COOLDOWN, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed),
		      mActiveTimer(IMPULSE_DURATION),
		      mCooldownTimer(mCooldown.GetValue())
		{
			// arm-inactive — 생성 직후 finished(비활성). 평소 IsActive=false / 쿨다운 해제. (Reset 으로 발동)
			mActiveTimer.Tick(mActiveTimer.GetBaseTime());
			mCooldownTimer.Tick(mCooldownTimer.GetBaseTime());
		}
```
변경:
```cpp
		Impulse()
		    : mImpulseForce(IMPULSE_FORCE, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
		      mCooldown(IMPULSE_COOLDOWN, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed)
		{
			// timer 는 OnEnter 에서 중앙 컨테이너에 Register + arm-inactive.
		}
```

- [ ] **Step 3: OnEnter / OnExit 에 등록·해제 추가**

기존:
```cpp
		void OnEnter() override { mBody = Components::FindPhysics(GetOwner()); }
		void OnExit() override { mBody = nullptr; }
```
변경:
```cpp
		void OnEnter() override
		{
			mBody = Components::FindPhysics(GetOwner());
			auto* be = GetOwner() ? GetOwner()->GetComponent<Entity::BaseEntity>() : nullptr;
			if (be == nullptr) return;
			mActiveTimer = be->Timers().Register("impulse.active", IMPULSE_DURATION);
			mActiveTimer->Tick(mActiveTimer->GetBaseTime());          // arm-inactive
			mCooldownTimer = be->Timers().Register("impulse.cooldown", mCooldown.GetValue());
			mCooldownTimer->Tick(mCooldownTimer->GetBaseTime());      // arm-inactive
		}
		void OnExit() override
		{
			if (auto* be = GetOwner() ? GetOwner()->GetComponent<Entity::BaseEntity>() : nullptr)
			{
				be->Timers().Unregister("impulse.active");
				be->Timers().Unregister("impulse.cooldown");
			}
			mBody = nullptr;
			mActiveTimer = nullptr;
			mCooldownTimer = nullptr;
		}
```

- [ ] **Step 4: Update 를 빈 override 로 (tick은 BaseEntity가)**

기존:
```cpp
		void Update(float dt) override
		{
			mActiveTimer.Tick(dt);
			mCooldownTimer.Tick(dt);
		}
```
변경:
```cpp
		void Update(float /*dt*/) override {}   // tick은 BaseEntity::Update 가 일괄 구동
```

- [ ] **Step 5: DoImpulse / IsActive 핸들 경유로**

기존:
```cpp
		void DoImpulse(vmath::vec2 dir) override
		{
			if (!mCooldownTimer.IsTimesUp() || IsActive()) return;   // 쿨다운 중 or 이미 active -> 게이트
			if (!mBody || !mBody->GetBody()) return;
```
변경 (첫 게이트만):
```cpp
		void DoImpulse(vmath::vec2 dir) override
		{
			if (!mCooldownTimer || !mCooldownTimer->IsTimesUp() || IsActive()) return;   // 미등록/쿨다운 중/active -> 게이트
			if (!mBody || !mBody->GetBody()) return;
```

기존 (DoImpulse 끝):
```cpp
			mActiveTimer.Reset();     // 버스트 창 발동 (0.3s)
			mCooldownTimer.Reset();   // 쿨다운 발동 (0.8s)
		}
```
변경:
```cpp
			mActiveTimer->Reset();     // 버스트 창 발동 (0.3s)
			mCooldownTimer->Reset();   // 쿨다운 발동 (0.8s)
		}
```

기존:
```cpp
		bool IsActive() const { return !mActiveTimer.IsTimesUp(); }    // 버스트 창 진행 중
```
변경:
```cpp
		bool IsActive() const { return mActiveTimer && !mActiveTimer->IsTimesUp(); }   // 버스트 창 진행 중
```

- [ ] **Step 6: 멤버 선언 타입 변경 (`Timer` → `Timer*`)**

기존:
```cpp
		SJH::Timer::Timer        mActiveTimer;        // 0.3s 버스트 창
		SJH::Timer::Timer        mCooldownTimer;      // 0.8s 재발동 게이트
```
변경:
```cpp
		SJH::Timer::Timer*       mActiveTimer   = nullptr;   // 0.3s 버스트 창 (중앙 컨테이너 핸들, 비소유)
		SJH::Timer::Timer*       mCooldownTimer = nullptr;   // 0.8s 재발동 게이트 (중앙 컨테이너 핸들, 비소유)
```

- [ ] **Step 7: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -15`
Expected: GREEN.

- [ ] **Step 8: 커밋**

```bash
git commit apps/_MyApp_/src/Physics/PhysicsImpulse.h -m "[refactor] : Impulse Timer 멤버 -> BaseEntity 중앙 컨테이너 등록 핸들 (impulse.active/cooldown)"
```

---

### Task 3: Life — optional<Timer> → 중앙 등록 핸들 + float 설정

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Components/LifeComponents.h`

- [ ] **Step 1: include 교체**

기존:
```cpp
#include "apps/_MyApp_/src/Algebraic/Stat.h"
#include "Components.Interfaces.h"
#include "scene/actor.h"
#include "timer/timer.h"
#include <functional>
#include <optional>
```
변경:
```cpp
#include "apps/_MyApp_/src/Algebraic/Stat.h"
#include "Components.Interfaces.h"
#include "apps/_MyApp_/src/Entity/BaseEntity.h"   // BaseEntity::Timers() (MultipleTimer 등록 위탁)
#include "scene/actor.h"
#include "timer/timer.h"
#include <functional>
```

- [ ] **Step 2: 멤버 교체 — optional → Timer* + float 설정값 + ArmInactive 제거**

기존:
```cpp
		// === SJH::Timer 자가 보유 (패턴 A) — "없음(미설정)"은 nullopt 로 표현 (VO 정통) ===
		// Timer 는 항상 유효한 시간값(VO). i-frame/사망지연이 "없는" 엔티티는 Timer 자체가 부재(nullopt).
		// 장전 시 Tick(base)로 finished 상태로 시작 -> 평소 비활성, 피격/사망 시 Reset 으로 발동
		// (생성 직후 passed=0 이면 "스폰 즉시 무적"(§5.1)이 되므로 finished 로 막는다).
		std::optional<SJH::Timer::Timer> mInvincibleTimer;   // i-frame    (nullopt = 무적 없음)
		std::optional<SJH::Timer::Timer> mDieTimer;          // 사망 연출 지연 (nullopt = 즉시)

		/// @brief s>0 이면 Timer(s) 를 finished(비활성) 상태로 장전, s<=0 이면 부재(nullopt).
		static void ArmInactive(std::optional<SJH::Timer::Timer> &slot, float s)
		{
			if (s > 0.0f)
			{
				slot.emplace(s);
				slot->Tick(s);   // passed=base -> IsTimesUp (평소 비활성; Reset 시 발동)
			}
			else
				slot.reset();
		}
```
변경:
```cpp
		// === Timer 중앙화 — BaseEntity 의 MultipleTimer 에 위탁, 핸들만 보유 (비소유) ===
		// "없음(미설정)"은 등록 안 함 = nullptr. 설정값(초)은 OnEnter 전(빌더)에 들어오므로 float 보관 후
		// OnEnter 에서 Register + arm-inactive(Tick(base)로 finished 시작 — 평소 비활성, Reset 시 발동).
		SJH::Timer::Timer* mInvincibleTimer = nullptr;   // i-frame    (nullptr = 무적 없음)
		SJH::Timer::Timer* mDieTimer        = nullptr;   // 사망 연출 지연 (nullptr = 즉시)
		float              mIFrameSeconds   = 0.0f;       // 등록 대기 설정값 (>0 시 OnEnter 등록)
		float              mDieDelaySeconds = 0.0f;       // 등록 대기 설정값 (>0 시 OnEnter 등록)
```

- [ ] **Step 3: 3-arg ctor — ArmInactive → float 저장**

기존:
```cpp
		Life(int max_hp, int cur_hp, float iframe)
		    : mMaxHp(static_cast<float>(max_hp), Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MaxHp),
		      mCurHp(cur_hp == -1 ? max_hp : cur_hp)
		{
			ArmInactive(mInvincibleTimer, iframe);   // iframe>0 일 때만 i-frame Timer 장전
		}
```
변경:
```cpp
		Life(int max_hp, int cur_hp, float iframe)
		    : mMaxHp(static_cast<float>(max_hp), Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MaxHp),
		      mCurHp(cur_hp == -1 ? max_hp : cur_hp)
		{
			mIFrameSeconds = iframe;   // OnEnter 에서 >0 일 때만 i-frame Timer 등록
		}
```

- [ ] **Step 4: Setter — ArmInactive → float 저장**

기존:
```cpp
		Life &SetIFrameSeconds(float s) { ArmInactive(mInvincibleTimer, s); return *this; }
		Life &SetOnDeathFx(std::function<void(const vmath::vec3 &)> fx) { mOnDeathFx = std::move(fx); return *this; }
		Life &SetDeathDelaySeconds(float s) { ArmInactive(mDieTimer, s); return *this; }   // Task 6 가 0.5 주입 예정
```
변경:
```cpp
		Life &SetIFrameSeconds(float s) { mIFrameSeconds = s; return *this; }
		Life &SetOnDeathFx(std::function<void(const vmath::vec3 &)> fx) { mOnDeathFx = std::move(fx); return *this; }
		Life &SetDeathDelaySeconds(float s) { mDieDelaySeconds = s; return *this; }
```

- [ ] **Step 5: OnEnter — sink 캐시 + 중앙 등록**

기존:
```cpp
		void OnEnter() override
		{
			// 핫패스 sink 1회 해소 (ctor 금지 — GetOwner null + dynamic type=base). 없으면 silent no-op.
			if (GetOwner())
				mSink = GetOwner()->GetComponent<IActorPresentation>();
		}
		void OnExit() override {}
```
변경:
```cpp
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
```

- [ ] **Step 6: Update — timer Tick 제거 (read만)**

기존:
```cpp
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
```
변경:
```cpp
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
```

> `IsInvincible()`, `DoDamaged`(`mInvincibleTimer->Reset()`), `DoDie`(`mDieTimer->Reset()`)는 `if(ptr)` / `ptr->` 문법이 optional 과 동일하므로 **무변경**.

- [ ] **Step 7: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -15`
Expected: GREEN.

- [ ] **Step 8: 커밋**

```bash
git commit apps/_MyApp_/src/Entity/Components/LifeComponents.h -m "[refactor] : Life optional<Timer> -> BaseEntity 중앙 등록 핸들 + 설정값 float 보관 (life.iframe/die)"
```

---

### Task 4: PlayerController — float mAttackTimer → 중앙 등록 핸들

**Files:**
- Modify: `apps/_MyApp_/src/InputHandler/PlayerController.h`
- Modify: `apps/_MyApp_/src/InputHandler/PlayerController.cpp`

- [ ] **Step 1: 헤더 — Timer 전방선언 + 멤버 타입 변경**

`namespace TopdownShooter::Entity { class BaseEntity; }` 위(또는 근처)에 Timer 전방선언 추가:
```cpp
namespace SJH::Timer { class Timer; }
namespace TopdownShooter::Entity { class BaseEntity; }
```

기존 멤버:
```cpp
		float                                       mAttackWindowSec = 0.15f; // 발사 후 "조준 응시" 윈도
		float                                       mAttackTimer     = 0.0f;
```
변경:
```cpp
		float                                       mAttackWindowSec = 0.15f; // 발사 후 "조준 응시" 윈도 (Register base)
		SJH::Timer::Timer*                          mAttackTimer     = nullptr; // 중앙 컨테이너 핸들 (비소유)
```

- [ ] **Step 2: cpp — OnEnter 에 등록 (mIsInitialized 통과 시)**

기존:
```cpp
	void PlayerController::OnEnter()
	{
		if (!mIsInitialized)
			return;
	}
```
변경:
```cpp
	void PlayerController::OnEnter()
	{
		if (!mIsInitialized)
			return;
		// attack 윈도 timer 를 BaseEntity 중앙 컨테이너에 등록 + arm-inactive (발사 시 Reset 으로 발동).
		if (auto* owner = GetOwner())
		{
			mEntity = owner->GetComponent<TopdownShooter::Entity::BaseEntity>();
			if (mEntity != nullptr)
			{
				mAttackTimer = mEntity->Timers().Register("player.attack", mAttackWindowSec);
				mAttackTimer->Tick(mAttackTimer->GetBaseTime());
			}
		}
	}
```

- [ ] **Step 3: cpp — OnExit 에 해제**

기존:
```cpp
	void PlayerController::OnExit()
	{
		if (!mIsInitialized)
			return;
		UnregisterBindings();
		mKeyboardInput = nullptr;
		mIsInitialized = false;
	}
```
변경:
```cpp
	void PlayerController::OnExit()
	{
		if (!mIsInitialized)
			return;
		if (mEntity != nullptr)
			mEntity->Timers().Unregister("player.attack");
		mAttackTimer = nullptr;
		UnregisterBindings();
		mKeyboardInput = nullptr;
		mIsInitialized = false;
	}
```

- [ ] **Step 4: cpp — Update 의 카운트다운 → 핸들 read**

기존:
```cpp
			namespace E = TopdownShooter::Entity;
			if (mAttackTimer > 0.0f) mAttackTimer -= dt;
			const bool        attacking = (mAttackTimer > 0.0f);
```
변경:
```cpp
			namespace E = TopdownShooter::Entity;
			const bool        attacking = (mAttackTimer != nullptr && !mAttackTimer->IsTimesUp()); // tick은 BaseEntity가
```

- [ ] **Step 5: cpp — OnFirePressed 의 set → Reset**

기존:
```cpp
		mAttackTimer = mAttackWindowSec; // 발사 후 0.15s 동안 facing=조준 (하이브리드)
```
변경:
```cpp
		if (mAttackTimer) mAttackTimer->Reset(); // 발사 후 0.15s 동안 facing=조준 (하이브리드)
```

- [ ] **Step 6: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -15`
Expected: GREEN.

- [ ] **Step 7: 커밋**

```bash
git commit apps/_MyApp_/src/InputHandler/PlayerController.h apps/_MyApp_/src/InputHandler/PlayerController.cpp -m "[refactor] : PlayerController attack 윈도 float -> BaseEntity 중앙 등록 핸들 (player.attack)"
```

---

### Task 5: WaveController — float mSpawnTimer → Timer 객체 (패턴 S)

**Files:**
- Modify: `apps/_MyApp_/src/Stage/WaveController.h`
- Modify: `apps/_MyApp_/src/Stage/WaveController.cpp`

- [ ] **Step 1: 헤더 — include + 멤버 타입 변경**

기존:
```cpp
#include "scene/actor.h"
#include <vector>
#include <vmath.h>
```
변경:
```cpp
#include "scene/actor.h"
#include "timer/timer.h"   // SJH::Timer::Timer (spawn 간격 — Timer 객체화)
#include <vector>
#include <vmath.h>
```

기존:
```cpp
        int   mWave       = 0;
        float mSpawnTimer = 0.0f;
        int   mSpawnCount = 0; // 누적 스폰 카운터 — ENEMY_FRONT variant 순환용
```
변경:
```cpp
        int   mWave       = 0;
        SJH::Timer::Timer mSpawnTimer;   // 스폰 간격 누적기 (WAVE_SPAWN_INTERVAL, ctor 초기화)
        int   mSpawnCount = 0; // 누적 스폰 카운터 — ENEMY_FRONT variant 순환용
```

- [ ] **Step 2: cpp — ctor 초기화 리스트에 mSpawnTimer 추가**

기존:
```cpp
    WaveController::WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                                    SJH::Scene::Actor* playerActor, float arenaHalfExtent)
        : mWorld(world), mSpawnParent(spawnParent),
          mPlayerActor(playerActor), mArenaHalfExtent(arenaHalfExtent)
    {}
```
변경 (선언 순서상 mArenaHalfExtent 뒤 → mSpawnTimer 순서 유지, -Wreorder 안전):
```cpp
    WaveController::WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                                    SJH::Scene::Actor* playerActor, float arenaHalfExtent)
        : mWorld(world), mSpawnParent(spawnParent),
          mPlayerActor(playerActor), mArenaHalfExtent(arenaHalfExtent),
          mSpawnTimer(WAVE_SPAWN_INTERVAL)
    {}
```

- [ ] **Step 3: cpp — Update 의 float 산술 → Timer API**

기존:
```cpp
        // 첫 웨이브 개시
        if (mWave == 0)
        {
            mWave       = 1;
            mSpawnTimer = 0.0f;
        }

        // 전멸 감지 ->다음 웨이브
        if (mWave > 0 && !mEnemies.empty() && LiveCount() == 0)
        {
            ++mWave;
            mEnemies.clear();
            mSpawnTimer = 0.0f;
            spdlog::info("[Wave] All cleared ->Wave {}", mWave);
        }

        mSpawnTimer += dt;
        if (mSpawnTimer >= WAVE_SPAWN_INTERVAL && LiveCount() < WAVE_MAX_ENEMIES)
        {
            mSpawnTimer = 0.0f;
            SpawnEnemy();
        }
```
변경:
```cpp
        // 첫 웨이브 개시
        if (mWave == 0)
        {
            mWave = 1;
            mSpawnTimer.Reset();
        }

        // 전멸 감지 ->다음 웨이브
        if (mWave > 0 && !mEnemies.empty() && LiveCount() == 0)
        {
            ++mWave;
            mEnemies.clear();
            mSpawnTimer.Reset();
            spdlog::info("[Wave] All cleared ->Wave {}", mWave);
        }

        mSpawnTimer.Tick(dt);
        if (mSpawnTimer.IsTimesUp() && LiveCount() < WAVE_MAX_ENEMIES)
        {
            mSpawnTimer.Reset();
            SpawnEnemy();
        }
```

- [ ] **Step 4: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -15`
Expected: GREEN.

- [ ] **Step 5: 커밋**

```bash
git commit apps/_MyApp_/src/Stage/WaveController.h apps/_MyApp_/src/Stage/WaveController.cpp -m "[refactor] : WaveController spawn 간격 raw-float -> SJH::Timer::Timer 객체화 (분산 산술 제거)"
```

---

### Task 6: 최종 통합 검증

**Files:** (없음 — 빌드/실행 확인)

- [ ] **Step 1: 클린-ish 전체 빌드**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -20`
Expected: GREEN, warning 0 (-Wall -Werror 통과 — 특히 -Wreorder/미사용 멤버 없음).

- [ ] **Step 2: 변경 파일 확인 (범위 누수 없음)**

Run: `git status --short`
Expected: 본 작업 7개 파일만 커밋됨. 미커밋 잔여가 본 작업 파일이면 누락 점검.

- [ ] **Step 3: (선택) GUI 육안 검증 — 사용자**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
확인 항목:
- 적 스폰 간격 3초 유지 (WaveController) — 웨이브/전멸 리셋 정상.
- 좌클릭 발사 후 facing=조준 0.15s 유지 (PlayerController attack 윈도).
- 피격 시 i-frame 무적/사망 처리 정상 (Life).
- (대시 입력 미배선이라 Impulse 는 dormant — 동작 변화 없음이 정상.)

---

## Self-Review

**1. Spec coverage:**
- §5.1 BaseEntity → Task 1 ✓
- §5.2 Life → Task 3 ✓
- §5.3 Impulse → Task 2 ✓
- §5.4 PlayerController → Task 4 ✓
- §5.5 WaveController → Task 5 ✓
- §6 동작 동등성 → Task 6 Step 3 육안 검증 + 각 Task 코드가 arm-inactive/clamp 불변 유지 ✓
- §7 빌드/의존성 → 각 Task 빌드 Step + Task 1 foundation 우선 ✓
- §8 가드레일 → partial commit / 코어 미수정 / 7파일 한정 명시 ✓

**2. Placeholder scan:** TBD/TODO 없음. 모든 코드 step 에 완전한 코드 블록 포함. ✓

**3. Type consistency:** 핸들 이름 일관 — `mInvincibleTimer`/`mDieTimer`/`mActiveTimer`/`mCooldownTimer`/`mAttackTimer` 모두 `SJH::Timer::Timer*`; `mSpawnTimer` 는 값 `SJH::Timer::Timer`. `Timers()` 반환 `MultipleTimer&`. 키 문자열 spec §4 와 동일(`life.iframe`/`life.die`/`impulse.active`/`impulse.cooldown`/`player.attack`). ✓
