# PlayerBehavior 분해 Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: `superpowers:subagent-driven-development`(권장) 또는 `superpowers:executing-plans` 로 task-by-task 구현. step 은 체크박스(`- [ ]`) 추적.
> **⚠ 검증 정책 (프로젝트 컨벤션 override)**: `no_auto_tests` 메모리 + 본 작업 지시에 따라 **단위 테스트 step 없음**. 각 Task 검증 = ① 빌드 exit 0 (`cmake --build --preset ninja --target _MyApp_`) + ② (해당 시) 실행 시각 검증 + ③ **사용자 승인 대기 후 commit** (commit 은 사용자 blocking step).
> **커밋 메시지**: 프로젝트 컨벤션 `[<type>] : <한 줄 요약>` (한국어, `Co-Authored-By` 트레일러 **미사용** — git history 일관).

**Goal:** 죽은 god-component `PlayerBehavior` 를 `Entity/Components` taxonomy 로 분해 — Dash→`Impulse`, i-frame+Die→`Life` 확장, 방향 스프라이트→`PlayerSpriteDirector`(8그룹), 데미지 배달→`Carrier`. 엔진 `GetComponent<T>` 게이트를 인터페이스 조회 가능하게 완화.

**Architecture:** 공유 베이스 컴포넌트 + Stat config + `IActorPresentation` sink(Template-Method 연출). gameplay→push→연출 sink(LEAF). controller→`IMovable`/`IImpulsable` port. Entity→Spawns std::function delegate(의존 역전). 전부 inward·acyclic.

**Tech Stack:** C++17, CMake(Ninja+MSVC), Box2D v2.4.1, `SJH::engine`(Actor/Component), `SJH::sprite`(SpriteRenderer.Visible 토글), `MyApp::*` Client STATIC 모듈.

**Spec:** [`doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md`](../specs/2026-06-01-playerbehavior-decomposition-design.md) (§13/§4/§14 직렬화).

---

## Summary

| # | Task | 변경 파일 | 핵심 결과 | 검증 |
|---|---|---|---|---|
| **0** | 엔진 게이트 완화 | `src/scene/actor.h` (+ `apps/_MyApp_/src/Physics/PhysicsComponent.h` 주석) | `GetComponent<Interface>` 가능 (`is_polymorphic_v`) | 빌드 |
| **1** | 인터페이스 추가 | `apps/_MyApp_/src/Physics/Components.Interfaces.h` | `IActorPresentation`+`IImpulsable`+`EFacing`/`EPose`/`quantize4` | 빌드 |
| **2** | Life 확장 | `apps/_MyApp_/src/Entity/Components/LifeComponents.h` | i-frame(plain float)+`DoDie`+`SetOnDeathFx`+sink forward | 빌드 |
| **3** | EnemyContactHandler repoint | `Entity/Enemy/EnemyContactHandler.{h,cpp}` | dead PB::Hit→`Life::DoDamaged` (load-bearing: 접촉 데미지 작동) | 빌드 + **실행** |
| **4** | Impulse 컴포넌트 | `<Physics>/physics_impulse.h` (new) + `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` | Dash/Knockback 속도버스트, 미배선 ship | 빌드 |
| **5** | Carrier base+subtypes | `apps/_MyApp_/src/Spawns/Carrier.h` (new) + `bullet_factory.h` + `enemy_factory.h` (+삭제 2종) | `GetComponent<IDamageable>` 배달, Bullet→Projectile, 적→ContactCarrier | 빌드 + **실행** |
| **6** | PlayerSpriteDirector + RD5 | `Player/PlayerSpriteDirector.{h,cpp}` (new) + `PlayerActor.{h,cpp}` + `PlayerBuilder.cpp` + `PlayerController.{h,cpp}` + Entity CMake | 8그룹 Visible 토글 + facing/pose 단일 작성자 + flipX 핵 삭제 | 빌드 + **실행** |
| **7** | 철거 | `PlayerBehavior.{h,cpp}` + `BulletSpawnPlayable.{h,cpp}` + `PlayerMovementComponents.h` 삭제 + Entity CMake | grep 0참조, link-clean | 빌드 + **실행** |

**의존 그래프 (순서 강제)**: 0 → 1 → 2 → 3 / 4 / (5 5는 0,1,2 후) / 6(0,1,2 후) → 7(전부 후). **3은 0+2 후 필수**(인터페이스 조회 + Life i-frame). **5는 0+1+2 후**(Deliver 가 IDamageable/IImpulsable 조회). **6은 1 후**(EFacing/EPose/sink). **7은 마지막**.

**Invariant**: 각 Task 종료 시 빌드 green. PB 는 Task 7까지 dead 로 잔존(삭제만 마지막). Task 3 전까지 적 접촉 데미지는 no-op(현 버그 유지) — Task 3에서 작동 시작.

---

## Task 0: 엔진 `GetComponent<T>` 게이트 완화

**Files:**
- Modify: `src/scene/actor.h:92-93` (SFINAE 게이트)
- Modify: `apps/_MyApp_/src/Physics/PhysicsComponent.h:72-73` (stale 주석 정정)

- [ ] **Step 1: actor.h 게이트 1줄 완화**

`src/scene/actor.h` 의 GetComponent 선언([actor.h:92-94](../../../src/scene/actor.h#L92))을 변경. **본문(`actor.h:179-193`)·`AddComponent`/`RemoveComponent` 의 `static_assert(is_base_of)` 는 무변경**.

```cpp
// AS-IS (line 92-94)
        template<typename T,
                 typename = std::enable_if_t<std::is_base_of_v<Component, T>>>
        T*   GetComponent() const;

// TO-BE
        template<typename T,
                 typename = std::enable_if_t<std::is_polymorphic_v<T>>>
        T*   GetComponent() const;
```

선언부 doxygen 주석(이미 인터페이스 조회를 기술)에 한 줄 보강:
```cpp
        /// @brief T 타입(인터페이스 포함) 컴포넌트 조회. 자식 Actor 는 순회하지 않음.
        /// @details
        ///   게이트 = is_polymorphic_v<T> — Component 파생뿐 아니라 순수 인터페이스
        ///   (IDamageable/IActorPresentation 등)도 조회 가능 (Unity GetComponent<IInterface> 정통).
        ///   1단계 — `typeid(T)` 정확 매칭 (O(1)). 구체 클래스 호출의 빠른 경로.
        ///   2단계 — miss 시 dynamic_cast<T*> 순회 (O(N)). 인터페이스/base 는 항상 2단계.
```

- [ ] **Step 2: PhysicsComponent.h:73 stale 주석 정정**

`apps/_MyApp_/src/Physics/PhysicsComponent.h:72-73`:
```cpp
// AS-IS
	/// @details Actor::GetComponent<T> 는 type_index 정확 매치 — Physics base 로 질의 불가.
	///          본 헬퍼는 ForEachComponent + dynamic_cast 로 BoxBody/CircleBody 모두 회수.

// TO-BE
	/// @details Actor::GetComponent<Physics> 도 이제 polymorphic 조회 가능(게이트 완화).
	///          단 첫 매칭만 반환하므로, 다중 Physics 가능성 + 명시적 의도를 위해 본 헬퍼 유지.
```

- [ ] **Step 3: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0, `Linking CXX executable apps/_MyApp_/_MyApp_`. (헤더 템플릿 변경이라 다수 TU 재컴파일.)

- [ ] **Step 4: 사용자 승인 후 commit**

```
[refactor] : 엔진 GetComponent 게이트 is_polymorphic 완화 (PB분해 Step0)

- actor.h GetComponent SFINAE 게이트 is_base_of<Component> → is_polymorphic_v<T>
  → 인터페이스(IDamageable/IActorPresentation) 조회 가능 (Unity GetComponent<IInterface> 정통)
- 본문/AddComponent·RemoveComponent static_assert 무변경 (생성·삭제는 concrete만)
- PhysicsComponent.h:73 stale 주석 정정 (base 질의 이제 가능)
```

---

## Task 1: `IActorPresentation` + `IImpulsable` + `EFacing`/`EPose`/`quantize4`

**Files:**
- Modify: `apps/_MyApp_/src/Physics/Components.Interfaces.h` (header-only — CMake 변경 없음)

- [ ] **Step 1: enum + quantize4 + 두 인터페이스 추가**

`Components.Interfaces.h` 의 `namespace TopdownShooter::Entity` 안, 기존 인터페이스 뒤에 추가. 파일 상단 include 에 `<cmath>` 추가:

```cpp
#include <cmath>   // quantize4 의 std::abs

// ... 기존 ILivable/IDieable/IDamageable/IAttackable/IMovable 뒤 ...

	enum class EFacing : int { Front = 0, Back, Left, Right };
	enum class EPose   : int { Idle  = 0, Move };

	/// @brief XZ 방향 벡터(aim/velocity)를 4방향으로 양자화. |x|>|z| 이면 좌우, 아니면 전후.
	///        부호 규약: x>0=Right, z>0=Front (탑다운 W=-Z 기준 — Task6 실행 검증).
	inline EFacing Quantize4(vmath::vec2 v)
	{
		if (std::abs(v[0]) > std::abs(v[1]))
			return v[0] > 0.0f ? EFacing::Right : EFacing::Left;
		return v[1] > 0.0f ? EFacing::Front : EFacing::Back;
	}

	/// @brief 게임플레이 베이스 → 연출 sink (Template-Method forward 대상). RD1 OPT-1.
	///        IContactable 式 defaulted no-op — director 가 쓰는 verb 만 override.
	///        스프라이트/FMOD/Effekseer 타입 0개 (deps inward).
	class IActorPresentation
	{
	  protected:
		IActorPresentation() = default;

	  public:
		virtual ~IActorPresentation() = default;
		IActorPresentation(const IActorPresentation &) = delete;
		IActorPresentation &operator=(const IActorPresentation &) = delete;
		IActorPresentation(IActorPresentation &&) = delete;
		IActorPresentation &operator=(IActorPresentation &&) = delete;

		virtual void ReactDamaged(int /*dmg*/) {}
		virtual void ReactDied(vmath::vec3 /*pos*/) {}
		virtual void ReactAttack(vmath::vec2 /*aimDir*/) {}
		virtual void FaceAim(vmath::vec2 /*aimDir*/) {}
		virtual void SetFacing(EFacing /*facing*/) {}
		virtual void SetPose(EPose /*pose*/) {}
	};

	/// @brief 일회성 타임드 속도 버스트 (Dash/Knockback). DoForward(지속)의 대칭 파트너.
	///        IMovable 오버로드 금지 (SetMovableTarget 계약) → 신규 인터페이스.
	class IImpulsable
	{
	  protected:
		IImpulsable() = default;

	  public:
		virtual ~IImpulsable() = default;
		IImpulsable(const IImpulsable &) = delete;
		IImpulsable &operator=(const IImpulsable &) = delete;
		IImpulsable(IImpulsable &&) = delete;
		IImpulsable &operator=(IImpulsable &&) = delete;

		virtual void DoImpulse(vmath::vec2 dir) = 0;
	};
```

- [ ] **Step 2: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0. (헤더만 추가 — 신규 심볼 미사용이라 link 변화 없음.)

- [ ] **Step 3: 사용자 승인 후 commit**

```
[feat] : IActorPresentation + IImpulsable + EFacing/EPose 인터페이스 (PB분해 Step1)

- IActorPresentation: IContactable式 defaulted no-op sink (ReactDamaged/Died/Attack/FaceAim/SetFacing/SetPose)
- IImpulsable: DoImpulse(vec2) 단일 — Dash/Knockback (IMovable 오버로드 금지)
- EFacing{Front,Back,Left,Right}/EPose{Idle,Move} + Quantize4 자유 함수
- header-only (Components.Interfaces.h) — 미사용이라 동작 변화 0
```

---

## Task 2: `Life` 확장 (i-frame + DoDie + sink forward)

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Components/LifeComponents.h` (header-only)

- [ ] **Step 1: Life 본문 교체**

`LifeComponents.h` 상단 include 에 `<functional>` 추가 (`Components.Interfaces.h` 는 이미 include). `Life` 클래스를 아래로 교체 (i-frame=plain float 확정 §6.1):

```cpp
#include "apps/_MyApp_/src/Algebraic/Stat.h"
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
			if (GetOwner()) GetOwner()->SetActive(false);
		}
	};
};
```

> 주의 — 기존 `Life()` 생성자가 i-frame 인자 없이 0.0f 기본이므로 적(enemy_factory `AddComponent<Life>(cfg.hp)`)·플레이어(PlayerActor `AddComponent<Life>(cfg.life.hp)`)는 i-frame 0 으로 동작 유지(회귀 없음). 플레이어 0.5s i-frame 은 Task 6 PlayerBuilder 에서 `SetIFrameSeconds(0.5f)` 주입.

- [ ] **Step 2: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0. (Life 는 헤더-only. `<functional>`/`<vmath.h>`(actor.h 전파) 의존 충족.)

- [ ] **Step 3: 사용자 승인 후 commit**

```
[feat] : Life i-frame + DoDie + IActorPresentation sink forward (PB분해 Step2)

- mIFrameSeconds/mInvincibleTimer (plain float) + IsInvincible() — DoDamaged 에 i-frame 게이트 fold
- DoDie() 구현: one-shot guard → ReactDied + onDeathFx(pos) → SetActive(false)
- OnEnter 에서 GetComponent<IActorPresentation> 1회 캐시 (sink 없으면 silent no-op)
- DoDamaged/DoDie 가 ReactDamaged/ReactDied 로 forward (Template-Method)
- Life(hp,cur,iframe) ctor + SetIFrameSeconds/SetOnDeathFx fluent. 기존 ctor i-frame 0 유지 (회귀 0)
```

---

## Task 3: `EnemyContactHandler` → `Life::DoDamaged` repoint (load-bearing)

**Files:**
- Modify: `<apps>/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp`
- Modify: `<apps>/_MyApp_/src/Entity/Enemy/EnemyContactHandler.h` (주석 정정)

- [ ] **Step 1: .cpp repoint**

`EnemyContactHandler.cpp` 전체 교체 — dead `PlayerBehavior::Hit` → `Life::DoDamaged` (IDamageable 인터페이스 조회):

```cpp
#include "<Entity>/Enemy/EnemyContactHandler.h"
#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"   // IDamageable
#include "scene/actor.h"

namespace TopdownShooter::Entity::Enemy
{
    EnemyContactHandler::EnemyContactHandler(int damage) : mDamage(damage) {}
    EnemyContactHandler::~EnemyContactHandler() = default;

    void EnemyContactHandler::OnCollisionEnter(SJH::Scene::Actor* other)
    {
        if (!other) return;
        // 게이트 완화(Step0)로 인터페이스 직접 조회. Life 가 IDamageable 다중상속 → slow-path dynamic_cast.
        // i-frame 게이트가 Life::DoDamaged 안에 있어 총알+접촉 모두 보호.
        if (auto* dmg = other->GetComponent<IDamageable>())
            dmg->DoDamaged(mDamage);
    }
}
```

- [ ] **Step 2: .h include/주석 정정**

`EnemyContactHandler.h` line 9 주석 + (필요시) 클래스 주석:
```cpp
    /// @brief 플레이어에 접촉 시 IDamageable::DoDamaged(damage) 호출 (i-frame 게이트는 Life 거주).
```
(헤더는 `<Player>/PlayerBehavior.h` 를 include 하지 않으므로 .h 변경은 주석만. `apps/_MyApp_/src/Physics/Components.Interfaces.h` 는 유지.)

- [ ] **Step 3: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0.

- [ ] **Step 4: 실행 검증 (load-bearing 버그 수정 확인)**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
Expected: 적이 플레이어에 접촉 시 **플레이어 HP 감소**(이전엔 no-op). 연속 접촉 시 0.5s i-frame 은 아직 미주입(Task6)이라 매 접촉 데미지 — Task6 후 i-frame 적용. 크래시 없음.

- [ ] **Step 5: 사용자 승인 후 commit**

```
[fix] : 적 접촉 데미지 repoint → Life::DoDamaged (PB분해 Step3, load-bearing)

- EnemyContactHandler 가 dead PlayerBehavior::Hit (nullptr no-op) 호출하던 버그 수정
- GetComponent<IDamageable>()->DoDamaged 로 repoint (게이트 완화 활용)
- 접촉 데미지가 비로소 작동 (유일한 동작 변화 = 양성)
- PlayerBehavior.h include 제거 (헤더는 원래 미include — 주석만 정정)
```

---

## Task 4: `Impulse` 컴포넌트 (Dash/Knockback, 미배선 ship)

**Files:**
- Create: `<apps>/_MyApp_/src/Physics/physics_impulse.h` (header-only — `physics_movement.h` 와 동일 패턴, Physics CMake 변경 없음)
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` (physics 분기에 AddComponent)

- [ ] **Step 1: physics_impulse.h 생성**

```cpp
#ifndef __MYAPP_PHYSICS_IMPULSE_H__
#define __MYAPP_PHYSICS_IMPULSE_H__

#include "apps/_MyApp_/src/Algebraic/Stat.h"
#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"
#include "apps/_MyApp_/src/Physics/PhysicsComponent.h"
#include "scene/actor.h"
#include <cmath>

namespace TopdownShooter::Physics
{
    /// @brief 범용 물리 속도 버스트 — Player Dash + Monster Knockback. PhysicsMovement 옆 거주(b2Body 동일 취급).
    /// @details
    ///   - body 는 OnEnter 에서 Components::FindPhysics(GetOwner()) 로 해소(비소유).
    ///   - DoImpulse(dir): cd/active 게이트 → SetLinearVelocity(normalize(dir)*force) XZ→XY(Z→-Y) → 타이머 arm.
    ///   - i-frame/FX 슬롯 부착 금지(미니 god 방지). i-frame=Life, FX=seam.
    ///   - 속도싸움(Step4): controller 가 IsActive() 중 DoForward suppress (dash 바인딩 시 활성, 현재 dormant).
    class Impulse : public SJH::Scene::Component, public Entity::IImpulsable
    {
    public:
        Impulse()
            : mImpulseForce(7.5f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
              mCooldown(0.8f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed)
        {
        }

        void OnEnter() override { mBody = Components::FindPhysics(GetOwner()); }
        void OnExit()  override { mBody = nullptr; }

        void Update(float dt) override
        {
            if (mActiveTimer   > 0.0f) mActiveTimer   -= dt;
            if (mCooldownTimer > 0.0f) mCooldownTimer -= dt;
        }

        void DoImpulse(vmath::vec2 dir) override
        {
            if (mCooldownTimer > 0.0f || IsActive()) return;
            if (!mBody || !mBody->GetBody()) return;
            const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
            if (len <= 0.001f) return;
            vmath::vec2 n(dir[0] / len, dir[1] / len);
            const float force = mImpulseForce.GetValue();
            // XZ → Box2D XY (Z → -Y, spec §4.4) — PB::Dash 미러
            mBody->GetBody()->SetLinearVelocity(b2Vec2(n[0] * force, -n[1] * force));
            mActiveTimer   = mDurationSec;
            mCooldownTimer = mCooldown.GetValue();
        }

        void ApplyKnockback(vmath::vec2 fromXZ) { DoImpulse(fromXZ); }  // convenience
        bool IsActive() const { return mActiveTimer > 0.0f; }

    private:
        Algebraic::Numeric::Stat mImpulseForce;   // Stat(DashForce, base 7.5)
        Algebraic::Numeric::Stat mCooldown;       // Stat(CoolDownSpeed, base 0.8 — sec 저장)
        float mDurationSec   = 0.3f;              // plain (맞는 enum 없음)
        float mActiveTimer   = 0.0f;
        float mCooldownTimer = 0.0f;
        Components::Physics* mBody = nullptr;      // FindPhysics — 비소유
    };
}

#endif // __MYAPP_PHYSICS_IMPULSE_H__
```

- [ ] **Step 2: CreatePlayerActor physics 분기에 Impulse 부착**

`apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` 상단 include 추가:
```cpp
#include "<Physics>/physics_impulse.h"
```
physics 분기([PlayerActor.cpp:47](../../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L47) `auto *pm = ... PhysicsMovement` 직후)에 추가:
```cpp
			auto *pm = actor->AddComponent<Physics::PhysicsMovement>(cfg.movement.speed);

			// Dash/Knockback 속도버스트 — dash 입력 미배선이라 현재 dormant (Action::Dash 바인딩 시 활성).
			actor->AddComponent<Physics::Impulse>();
```

- [ ] **Step 3: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0. (physics_impulse.h header-only — Physics CMake 무변경. PlayerActor.cpp 는 Entity 모듈, game_deps PRIVATE 로 box2d 충족.)

- [ ] **Step 4: 사용자 승인 후 commit**

```
[feat] : Impulse 컴포넌트 (Dash/Knockback 속도버스트) (PB분해 Step4)

- <Physics>/physics_impulse.h — IImpulsable, DashForce(7.5)/CoolDownSpeed(0.8) Stat + plain duration(0.3)/timer
- DoImpulse: cd/active 게이트 → SetLinearVelocity XZ→XY → 타이머 arm (PB::Dash 미러)
- CreatePlayerActor physics 분기에 AddComponent (미배선 ship — dash 입력 없음 = correct-by-construction)
- 속도싸움 suppress 는 dash 바인딩 시 controller 에 (현재 dormant)
```

---

## Task 5: `Carrier` base+subtypes (Projectile/ContactCarrier — Bullet/적 흡수)

> **⚠ 본 Task 가 가장 무거움** (live 전투 코드 리팩토링). 2 sub-commit (5a Bullet→Projectile, 5b 적→ContactCarrier)로 분리. 각 sub 후 빌드+실행 검증.
> **범위 주의**: spec §5.6 의 "적 자식 센서(hurtbox) Carrier" 의 *별도 센서 fixture + 자식 body follow* 토폴로지는 물리 fixture 설계가 brainstorm 에서 미상세 → 본 Task 는 **동작 보존**(적 body 의 기존 OnCollisionEnter 재사용)으로 ContactCarrier 를 적 actor 에 직접 부착. 별도 hurtbox 센서 child 변형은 후속(물리 fixture 설계 필요 시) — 구현자 확인 게이트.

**Files:**
- Create: `apps/_MyApp_/src/Spawns/Carrier.h` (CarrierBase + Projectile + ContactCarrier — header-only)
- Delete: `<apps>/_MyApp_/src/Spawns/Projectile.h` (스켈레톤 superseded)
- Modify: `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h` (BulletContactHandler → Carrier::Projectile)
- Modify: `<apps>/_MyApp_/src/Entity/Enemy/enemy_factory.h` (EnemyContactHandler → Carrier::ContactCarrier)
- Delete: `apps/_MyApp_/src/Entity/Bullet/BulletContactHandler.{h,cpp}` + `apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.{h,cpp}`
- Modify: `apps/_MyApp_/src/Entity/CMakeLists.txt` (BulletContactHandler.cpp / EnemyContactHandler.cpp 제거)

### 5a — CarrierBase + Projectile (Bullet)

- [ ] **Step 1: apps/_MyApp_/src/Spawns/Carrier.h 생성**

```cpp
#ifndef __TOPDOWNSHOOTER_SPAWN_CARRIER__
#define __TOPDOWNSHOOTER_SPAWN_CARRIER__

#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"   // IDamageable / IImpulsable
#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"             // IContactable
#include "scene/actor.h"
#include <functional>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Spawn::Carrier
{
    /// @brief 데미지 배달 공통 베이스 (얇음 — 확정 §6.4). 수명/센서/이동은 subtype 책임.
    class CarrierBase : public SJH::Scene::Component,
                        public Physics::IContactable
    {
      public:
        using HitFx = std::function<void(const vmath::vec3 &)>;

        CarrierBase &SetOwnerEntity(SJH::Scene::Actor *o) { mOwnerEntity = o; return *this; }
        CarrierBase &SetDamage(int d) { mDamage = d; return *this; }
        CarrierBase &SetOnHitFx(HitFx fx) { mOnHitFx = std::move(fx); return *this; }  // C4 — SetOnHitFx 흡수

      protected:
        /// @brief C1 배달: target 의 IDamageable→DoDamaged + (있으면) IImpulsable→DoImpulse 넉백 + onHitFx.
        void Deliver(SJH::Scene::Actor *target, vmath::vec2 knockbackDir)
        {
            if (!target || target == mOwnerEntity || !target->IsActive()) return;
            if (auto *dmg = target->GetComponent<Entity::IDamageable>())
                dmg->DoDamaged(mDamage);
            if (auto *imp = target->GetComponent<Entity::IImpulsable>())
                imp->DoImpulse(knockbackDir);
            if (mOnHitFx)
                mOnHitFx(target->GetTransform().Translate);   // spawn-at-point seam 유지
        }

        SJH::Scene::Actor *mOwnerEntity = nullptr;   // 발사자 (자가 피해 방지)
        int    mDamage = 0;
        HitFx  mOnHitFx;
    };

    /// @brief flying + lifetime + self-despawn. BulletContactHandler 흡수 (mAlive 가드 + 지연 despawn).
    class Projectile : public CarrierBase, public Entity::IDieable
    {
      public:
        explicit Projectile(int damage) { mDamage = damage; }

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override
        {
            // 콜백 중 b2Body 수정 금지 → despawn 은 Update 에서 지연 (BulletContactHandler 패턴).
            if (mPendingDisable && GetOwner()) GetOwner()->SetActive(false);
        }

        void DoDie() override { mPendingDisable = true; }

        void OnCollisionEnter(SJH::Scene::Actor *other) override { HandleHit(other); }
        void OnTriggerEnter  (SJH::Scene::Actor *other) override { HandleHit(other); }

      private:
        void HandleHit(SJH::Scene::Actor *other)
        {
            if (!mAlive || !other) return;
            mAlive = false;
            // 넉백 방향 = 발사 진행 방향(현 위치→타겟). 간단히 타겟-소유자 XZ 평면 차분.
            const vmath::vec3 d = other->GetTransform().Translate - GetTransform().Translate;
            Deliver(other, vmath::vec2(d[0], -d[2]));
            DoDie();
        }

        bool mAlive = true;
        bool mPendingDisable = false;
    };

    /// @brief sensor + persistent (적 접촉 데미지). EnemyContactHandler 흡수 — 적 body 재사용(동작 보존).
    class ContactCarrier : public CarrierBase
    {
      public:
        explicit ContactCarrier(int damage) { mDamage = damage; }

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override {}

        void OnCollisionEnter(SJH::Scene::Actor *other) override { Deliver(other, vmath::vec2(0.0f)); }
        void OnTriggerEnter  (SJH::Scene::Actor *other) override { Deliver(other, vmath::vec2(0.0f)); }
    };
}; // namespace TopdownShooter::Spawn::Carrier
#endif // __TOPDOWNSHOOTER_SPAWN_CARRIER__
```

> 넉백: i-frame 으로 보호된 타겟은 `DoDamaged` early-return 하지만 `DoImpulse` 는 별도 호출됨. 현재 플레이어에 Impulse 부착되어 있으니(Task4) 총알/접촉 넉백이 적용된다. 과한 넉백이면 추후 게이트 조정 — 동작 검증 후 판단.

- [ ] **Step 2: 기존 <Spawns>/Projectile.h 삭제 + 참조 확인**

```bash
rm <apps>/_MyApp_/src/Spawns/Projectile.h
grep -rn "<Spawns>/Projectile.h\|Spawn::Carrier::Projectile" apps/_MyApp_/src   # Carrier.h 외 0 이어야 함
```
Expected: `Carrier.h` 의 정의 외 다른 include 없음 (스켈레톤은 미사용이었음).

- [ ] **Step 3: bullet_factory.h — Projectile 로 교체**

`apps/_MyApp_/src/Entity/Bullet/bullet_factory.h`:
```cpp
// include 교체
// AS-IS: #include "<Entity>/Bullet/BulletContactHandler.h"
// TO-BE:
#include "apps/_MyApp_/src/Spawns/Carrier.h"
// (BulletLifetime.h 유지)

// AddComponent 교체 (line 50 부근)
// AS-IS: actor->AddComponent<BulletContactHandler>(cfg.damage);
// TO-BE:
        auto* proj = actor->AddComponent<Spawn::Carrier::Projectile>(cfg.damage);
        proj->SetOwnerEntity(nullptr);   // 발사자 자가피해 방지 site (현재 필터로 충분 — 후속 owner 주입 가능)
        actor->AddComponent<BulletLifetime>(cfg.lifetime);
```

> `SetOnHitFx` 임팩트 FX seam: 현재 `BulletContactHandler::SetOnHitFx` 발화처가 main/M6 배선에 있으면 `proj->SetOnHitFx(...)` 로 동일 이전. 현 코드에 발화처가 없으면(미배선) 생략 — grep `SetOnHitFx` 로 확인 후 발화처 있으면 Projectile 로 repoint.

- [ ] **Step 4: 빌드 검증 (5a)**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 빌드 실패 가능 — `BulletContactHandler.cpp` 가 아직 CMake 에 남아 `BulletContactHandler.h` 삭제 전이면 OK. **본 step 에서는 BulletContactHandler 아직 미삭제** → 빌드 green 이어야 함 (bullet_factory 만 Projectile 사용). exit 0.

- [ ] **Step 5: 실행 검증 (5a)**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
Expected: 좌클릭 발사 → 총알이 적 명중 시 적 HP 감소 + 총알 소멸(self-despawn). 벽 명중 시 소멸. 크래시 없음.

- [ ] **Step 6: 5a commit (사용자 승인 후)**

```
[refactor] : Bullet → Carrier::Projectile (PB분해 Step5a)

- apps/_MyApp_/src/Spawns/Carrier.h: CarrierBase(얇은 배달 베이스) + Projectile(IDieable, self-despawn) + ContactCarrier
- Deliver(): GetComponent<IDamageable>->DoDamaged + IImpulsable 넉백 + onHitFx seam
- bullet_factory: BulletContactHandler → Carrier::Projectile. 기존 <Spawns>/Projectile.h 스켈레톤 삭제
```

### 5b — ContactCarrier (적) + BulletContactHandler/EnemyContactHandler 철거

- [ ] **Step 7: enemy_factory.h — ContactCarrier 로 교체**

`<apps>/_MyApp_/src/Entity/Enemy/enemy_factory.h`:
```cpp
// include 교체
// AS-IS: #include "<Entity>/Enemy/EnemyContactHandler.h"
// TO-BE:
#include "apps/_MyApp_/src/Spawns/Carrier.h"

// AddComponent 교체 (line 54)
// AS-IS: actor->AddComponent<EnemyContactHandler>(cfg.damage);
// TO-BE:
        actor->AddComponent<Spawn::Carrier::ContactCarrier>(cfg.damage);
```

- [ ] **Step 8: BulletContactHandler / EnemyContactHandler 삭제 + CMake 정리**

```bash
rm <apps>/_MyApp_/src/Entity/Bullet/BulletContactHandler.h <apps>/_MyApp_/src/Entity/Bullet/BulletContactHandler.cpp
rm <apps>/_MyApp_/src/Entity/Enemy/EnemyContactHandler.h <apps>/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp
grep -rn "BulletContactHandler\|EnemyContactHandler" apps/_MyApp_/src   # 0 이어야 함
```
`apps/_MyApp_/src/Entity/CMakeLists.txt` source 목록에서 2줄 제거:
```cmake
# 제거:
#   <Bullet>/BulletContactHandler.cpp
#   <Enemy>/EnemyContactHandler.cpp
```
또한 Entity CMake 에 `MyApp::Spawns` 또는 Carrier.h include 경로가 필요한지 확인: bullet_factory.h/enemy_factory.h 가 `apps/_MyApp_/src/Spawns/Carrier.h` 를 include → Entity 모듈이 Spawns 헤더 경로를 봐야 함. `apps/_MyApp_/src/Spawns/Carrier.h` 는 header-only지만 include 경로(`src/`)는 Entity 의 PUBLIC include(`../`)로 해소됨(같은 `src/` 루트). **단 순환 의존 주의**: Spawns CMake 가 이미 `MyApp::Entity` PUBLIC link → Entity 가 Spawns 를 link 하면 순환. Carrier.h 는 header-only(심볼 없음)이므로 **link 불필요, include 경로만** 필요 → Entity 의 기존 PUBLIC `../` include 로 충분(CMake link 추가 금지). 빌드로 확인.

- [ ] **Step 9: 빌드 검증 (5b)**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0. (CMakeLists.txt 변경 → 자동 reconfigure. 순환 link 에러 없으면 성공.)

- [ ] **Step 10: 실행 검증 (5b)**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
Expected: 적 접촉 시 플레이어 HP 감소(Task3 동작 유지, 이제 ContactCarrier 경유). 총알 명중 정상. 크래시 없음.

- [ ] **Step 11: 5b commit (사용자 승인 후)**

```
[refactor] : 적 접촉 → Carrier::ContactCarrier + BulletContactHandler/EnemyContactHandler 철거 (PB분해 Step5b)

- enemy_factory: EnemyContactHandler → Carrier::ContactCarrier (적 body 재사용, 동작 보존)
- BulletContactHandler.{h,cpp} + EnemyContactHandler.{h,cpp} 삭제 + Entity CMake 2줄 제거
- 적 자식 센서(hurtbox) 별도 fixture 토폴로지는 후속 (물리 설계 필요 시)
```

---

## Task 6: 스프라이트 트랙 — PlayableDirector(directional/HitBlink/Dissolve) + PlayerController RD5 + flipX 삭제

> **🔴 재편 (2026-06-02, PlayableDirector foundation 기준)**: 사용자 철학 *"게임로직 외 모든 연출은 PlayableDirector 경유"* 확정 → **별도 `PlayerSpriteDirector` 를 만들지 않는다.** 대신:
> 1. **전제 = 공유 foundation 먼저**(별도 에이전트): `PlayableDirector`(Component+IActorPresentation sink, named Playable + Play(key) + 중앙 tick) + `PostFXRegistry` + 플레이어 sink 부착 + onFire/onDamage 마이그레이션. 정본 spec [`2026-06-02-playable-director-foundation-design.md`](../specs/2026-06-02-playable-director-foundation-design.md). **본 Task 시작 전 foundation 랜딩 확인.**
> 2. **Task 6(스프라이트 트랙) = foundation 의 `PlayableDirector` 위에**: (a) `PlayableDirector` 에 **directional 지원 확장**(DirGroup + RegisterGroup + `SetFacing/SetPose`→active 그룹 Visible 토글 + B-part Play). (b) 신규 Playable 타입 **`HitBlinkPlayable`**(enableHit true 0.25s→false) + **`DissolvePlayable`**(dissolveThreshold 0→1, dissolve.png) — 플레이어 SpriteRenderer 핸들 주입. (c) PlayerBuilder 가 `"hit"`/`"death"` key Composite 에 이 스프라이트 Playable 을 합성(hit-FX 트랙의 Vignette/사운드와 ParallelPlayable 로 묶임 — 공유 편집점). (d) PlayerController RD5 → `sink->SetFacing/SetPose` + flipX 핵 삭제. (e) PlayerBuilder `life->SetIFrameSeconds(0.5f).SetDeathDelaySeconds(0.5f)` 주입.
> 3. **아래 Step 1~2(PlayerSpriteDirector.{h,cpp} 직접 생성 + 직접 enableHit/Visible poke)는 🔴 폐기** — foundation 의 PlayableDirector 확장 + HitBlink/Dissolve Playable 로 대체. **Step 4~7 은 유효하되 `PlayerSpriteDirector` → `PlayableDirector` 로 치환, RegisterGroup 은 PlayableDirector 에 등록.** RD1~6 보존(sink 추상 seam·액터당 1·gameplay→presentation 비읽기). FX 구동은 직접-poke 가 아니라 Playable-through-director.

**Files (재편 후):**
- Modify: `apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp}` (foundation 생성분 — directional DirGroup + RegisterGroup + SetFacing/SetPose 구현 추가)
- Create: `apps/_MyApp_/src/Playable/HitBlinkPlayable.{h,cpp}` + `DissolvePlayable.{h,cpp}` (PlayableBase 파생, SpriteRenderer 핸들 구동) + CMake 등록
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerActor.h` (`SpriteCfg` 8그룹 config + `DirGroupCfg`)
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` (8그룹 빌드 루프 + PlayableDirector 에 RegisterGroup)
- Modify: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` (8그룹 테이블 + `"hit"`/`"death"` Composite 에 HitBlink/Dissolve 합성 + `SetIFrameSeconds(0.5).SetDeathDelaySeconds(0.5)` 주입 + child-scan 은퇴) — **hit-FX 트랙과 공유**
- Modify: `apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}` (RD5 계산→`sink->SetFacing/SetPose`, flipX 핵 삭제)

> 아래 Step 1~10 은 *원(2026-06-01) PlayerSpriteDirector 설계* 기록 — Step 1~3 은 폐기(위 재편으로 대체), Step 4~7 은 PlayerSpriteDirector→PlayableDirector 치환해 읽을 것. Step 8~10(빌드/실행/커밋 검증)은 그대로 유효.

- [ ] **Step 1: PlayerSpriteDirector.h 생성** 🔴 **폐기 (PlayableDirector 재편)** — 아래는 원설계 기록. 실제로는 foundation 의 `PlayableDirector` 에 directional 지원(DirGroup/RegisterGroup/SetFacing·SetPose Visible 토글)을 *확장*하고, 깜빡임/디졸브는 `HitBlinkPlayable`/`DissolvePlayable` 로 분리한다(헤더 재편 노트 참조). 아래 DirGroup/Apply 구조는 PlayableDirector 의 directional 부에 흡수.

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_PLAYER_SPRITE_DIRECTOR_H__
#define __TOPDOWNSHOOTER_ENTITY_PLAYER_SPRITE_DIRECTOR_H__

#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"   // IActorPresentation / EFacing / EPose
#include "playable/iplayable.h"
#include "scene/actor.h"
#include <array>

namespace SJH::Sprite { class SpriteRenderer; }
namespace SJH::SpriteSequence { class SpriteSequencePlayable; }

namespace TopdownShooter::Entity::Player
{
    /// @brief 방향 스프라이트 sink (presentation-only LEAF). 4-레이어 (EFacing×EPose) 그룹 소유.
    /// @details siblings(PlayerController)가 SetFacing/SetPose push, 자신은 never call back.
    ///          physics/Stat/movement/input 없음. velocity 안 읽음 (RD5 — controller 단일 작성자).
    class PlayerSpriteDirector : public SJH::Scene::Component, public IActorPresentation
    {
      public:
        struct DirGroup
        {
            std::array<SJH::Sprite::SpriteRenderer *, 4> layers{};         // E·H·B·F
            SJH::SpriteSequence::SpriteSequencePlayable *bPart = nullptr;  // ColCount>1 애니(B) 레이어
        };

        void RegisterGroup(EFacing f, EPose p, const DirGroup &g) { mGroups[idx(f)][idx(p)] = g; }
        void SetMoveEffect(SJH::Playable::IPlayable *p) { mMoveEffect = p; }
        void PlayOneShot(SJH::Playable::IPlayable *p);   // 얇은 generic 주입 훅 (자산 의존 0)

        // === IActorPresentation (sink) ===
        void SetFacing(EFacing f) override;
        void SetPose(EPose p) override;
        // ReactDamaged/ReactDied/ReactAttack/FaceAim 은 자산 있을 때만 override (현재 defaulted no-op)

        EFacing CurrentFacing() const { return mFacing; }
        EPose   CurrentPose() const { return mPose; }

        void OnEnter() override { Apply(); }   // 초기 가시성 (Front/Idle 만 보이게)
        void OnExit() override {}
        void Update(float /*dt*/) override {}  // velocity 안 읽음 (RD5)

      private:
        static int idx(EFacing f) { return static_cast<int>(f); }
        static int idx(EPose p) { return static_cast<int>(p); }
        void Apply();   // (mFacing,mPose) 그룹만 Visible=true, 나머지 false; 활성 B-part Play

        DirGroup mGroups[4][2]{};
        EFacing  mFacing = EFacing::Front;
        EPose    mPose   = EPose::Idle;
        SJH::Playable::IPlayable *mMoveEffect = nullptr;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_PLAYER_SPRITE_DIRECTOR_H__
```

- [ ] **Step 2: PlayerSpriteDirector.cpp 생성** 🔴 **폐기 (PlayableDirector 재편)** — `Apply()`(Visible 토글)는 PlayableDirector 의 SetFacing/SetPose 로, `ReactDamaged`(enableHit 직접 poke)/`ReactDied`(dissolve 직접 ramp)는 `HitBlinkPlayable`/`DissolvePlayable` 을 `"hit"`/`"death"` Composite 에 등록하는 방식으로 대체. 아래는 원설계 기록.

```cpp
#include "<Entity>/Player/PlayerSpriteDirector.h"

#include "playable/iplayable.h"
#include "sprite/sprite_component.h"           // SpriteRenderer.Visible
#include "sprite/sprite_sequence_playable.h"   // SpriteSequencePlayable

namespace TopdownShooter::Entity::Player
{
    void PlayerSpriteDirector::SetFacing(EFacing f)
    {
        if (f == mFacing) return;
        mFacing = f;
        Apply();
    }

    void PlayerSpriteDirector::SetPose(EPose p)
    {
        if (p == mPose) return;
        const EPose prev = mPose;
        mPose = p;
        // Move 진입/이탈 시 발 파티클(mMoveEffect) Play/Stop (PB mMoveEffect 미러)
        if (mMoveEffect)
        {
            if (p == EPose::Move && prev != EPose::Move) mMoveEffect->Play();
            else if (p != EPose::Move && prev == EPose::Move) mMoveEffect->Stop();
        }
        Apply();
    }

    void PlayerSpriteDirector::PlayOneShot(SJH::Playable::IPlayable *p)
    {
        if (p) { p->Stop(); p->Play(); }
    }

    void PlayerSpriteDirector::Apply()
    {
        for (int f = 0; f < 4; ++f)
            for (int p = 0; p < 2; ++p)
            {
                const bool active = (f == idx(mFacing) && p == idx(mPose));
                for (auto *layer : mGroups[f][p].layers)
                    if (layer) layer->Visible = active;
            }
        // 활성 그룹 B-part 애니 재생 (루프는 빌드 시 SetIsLoop(true).Play() 로 이미 켜짐 — 여기선 보장만)
        if (auto *b = mGroups[idx(mFacing)][idx(mPose)].bPart)
            b->Play();
    }
}
```

- [ ] **Step 3: Entity CMakeLists.txt 에 .cpp 추가**

`apps/_MyApp_/src/Entity/CMakeLists.txt` source 목록에 추가:
```cmake
    <Player>/PlayerSpriteDirector.cpp
```

- [ ] **Step 4: PlayerActor.h — SpriteCfg 8그룹 config**

`apps/_MyApp_/src/Entity/Player/PlayerActor.h` 상단 include 추가:
```cpp
#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"   // EFacing / EPose
```
`SpriteCfg` 를 교체 (단일 `direction` → 8그룹 테이블):
```cpp
		/// @brief 방향 그룹 1개 = (facing,pose) + 4-레이어 텍스처 세트.
		struct DirGroupCfg
		{
			Entity::EFacing facing;
			Entity::EPose   pose;
			const std::vector<TopdownShooter::Playable::EntityTextureConfig> *layers;
		};

		struct SpriteCfg
		{
			/// @brief 8그룹(4 facing × 2 pose) 테이블. nullptr → 스프라이트 없음(게임플레이-only).
			const std::vector<DirGroupCfg> *groups = nullptr;
			float fps = 8.0f;
		};
```

- [ ] **Step 5: PlayerActor.cpp — 8그룹 빌드 + RegisterGroup**

`apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` 상단 include 추가:
```cpp
#include "<Entity>/Player/PlayerSpriteDirector.h"
```
기존 4-레이어 루프([PlayerActor.cpp:86-118](../../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L86))를 8그룹 빌드로 교체. Life 부착([PlayerActor.cpp:20](../../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L20)) 뒤에 director 부착, 그룹 루프에서 RegisterGroup:
```cpp
		// === 8방향 스프라이트 그룹 (spec §5.5) — groups 지정 시에만 ===
		if (cfg.sprite.groups != nullptr)
		{
			auto *director = actor->AddComponent<PlayerSpriteDirector>();
			auto &reg = SJH::ResourceRegistry::Get();

			for (const auto &grp : *cfg.sprite.groups)
			{
				PlayerSpriteDirector::DirGroup dg{};
				int layerIdx = 0;
				for (const auto &t : *grp.layers)
				{
					auto *atlas = reg.FindUniformAtlas(t.TexturePath);
					if (!atlas)
						atlas = reg.CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
					if (!atlas)
					{
						spdlog::error("[8layer] atlas load 실패: {}", t.TexturePath);
						continue;
					}

					auto child = std::make_unique<SJH::Scene::Actor>(
					    cfg.name + "_" + std::to_string(static_cast<int>(grp.facing)) + "_" + std::to_string(static_cast<int>(grp.pose)) + "_L" + std::to_string(t.DrawOrder));
					SJH::Scene::Actor *childPtr = actor->AddChild(std::move(child));

					auto *spr = childPtr->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
					spr->flipX = t.Flip;
					spr->QueueOffset = t.DrawOrder;

					if (layerIdx < 4) dg.layers[layerIdx] = spr;

					if (t.ColCount > 1)   // 애니(B) 레이어
					{
						auto *seq = childPtr->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
						    spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, cfg.sprite.fps});
						seq->SetIsLoop(true);
						seq->Play();
						dg.bPart = seq;
					}
					++layerIdx;
				}
				director->RegisterGroup(grp.facing, grp.pose, dg);
			}
			// director->OnEnter() 의 Apply() 가 초기 가시성(Front/Idle 만) 적용.
		}
```
> 기존 `if (cfg.sprite.direction != nullptr)` 블록 전체를 위 `groups` 블록으로 대체.

- [ ] **Step 6: PlayerBuilder.cpp — 8그룹 테이블 + i-frame 주입 + child-scan 은퇴**

`apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp`:
파일 상단(함수 밖, anonymous namespace 또는 static)에 8그룹 테이블:
```cpp
#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"   // EFacing / EPose

namespace {
    using TopdownShooter::Entity::Player::PlayerActorConfig;
    using TopdownShooter::Entity::EFacing;
    using TopdownShooter::Entity::EPose;
    namespace P = TopdownShooter::Playable;

    const std::vector<PlayerActorConfig::DirGroupCfg> kPlayerGroups = {
        {EFacing::Front, EPose::Idle, &P::PLAYER_FRONT_IDLE},
        {EFacing::Back,  EPose::Idle, &P::PLAYER_BACK_IDLE},
        {EFacing::Left,  EPose::Idle, &P::PLAYER_LEFT_IDLE},
        {EFacing::Right, EPose::Idle, &P::PLAYER_RIGHT_IDLE},
        {EFacing::Front, EPose::Move, &P::PLAYER_FRONT_MOVE},
        {EFacing::Back,  EPose::Move, &P::PLAYER_BACK_MOVE},
        {EFacing::Left,  EPose::Move, &P::PLAYER_LEFT_MOVE},
        {EFacing::Right, EPose::Move, &P::PLAYER_RIGHT_MOVE},
    };
}
```
`pac.sprite.direction = &PLAYER_FRONT_MOVE;`([PlayerBuilder.cpp:106](../../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L106)) → 교체:
```cpp
		pac.sprite.groups = &kPlayerGroups;   // 8그룹 (4 facing × 2 pose)
```
child-scan 블록([PlayerBuilder.cpp:115-123](../../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L115))은 **삭제** (result.Sprite/SpriteSeq 는 director 가 그룹 소유 — 더 이상 단일 핸들 안 잡음). `PlayerResult.Sprite`/`SpriteSeq` 멤버 사용처 grep 후 미사용이면 멤버도 제거(별도 확인). 

i-frame 주입 — Life 는 CreatePlayerActor 가 `AddComponent<Life>(cfg.life.hp)` 로 생성하므로, PlayerBuilder 가 i-frame/onDeathFx 를 주입하려면 생성된 Life 핸들 필요. CreatePlayerActor 가 Life* 를 반환하지 않으므로, BuildPlayer 에서 `result.SpriteActor->GetComponent<Components::Life>()->SetIFrameSeconds(0.5f)` 로 사후 주입 (`result.SpriteActor` 부착 직후):
```cpp
		// 플레이어 0.5s i-frame (PB mHitInvincibility 미러) — 사후 주입.
		if (auto *life = result.SpriteActor->GetComponent<TopdownShooter::Entity::Components::Life>())
			life->SetIFrameSeconds(0.5f);
```
> include `"apps/_MyApp_/src/Entity/Components/LifeComponents.h"` 추가. (onDeathFx 주입은 §14 Step7 후속 — 본 Task 미포함.)

- [ ] **Step 7: PlayerController — RD5 계산→push + flipX 핵 삭제**

`apps/_MyApp_/src/InputHandler/PlayerController.h`:
- 멤버 추가:
```cpp
		Entity::IActorPresentation *mSink = nullptr;   // OnEnter 캐시 (방향 sink)
		Entity::EFacing mLastFacing = Entity::EFacing::Front;
		float mAttackWindowSec = 0.15f;                // 확정 §6.3
		float mAttackTimer = 0.0f;
```
- `mCachedSprite` 멤버([PlayerController.h:101](../../../apps/_MyApp_/src/InputHandler/PlayerController.h#L101)) **삭제** (+ `SpriteRenderer` forward 선언 제거).

`apps/_MyApp_/src/InputHandler/PlayerController.cpp`:
- `OnEnter()` 에 sink 캐시 추가:
```cpp
		if (GetOwner()) mSink = GetOwner()->GetComponent<Entity::IActorPresentation>();
```
- `OnFirePressed()`(발사 직후, [PlayerController.cpp:276](../../../apps/_MyApp_/src/InputHandler/PlayerController.cpp#L276) onFire 근처)에 공격 윈도우 arm:
```cpp
		mAttackTimer = mAttackWindowSec;
```
- 기존 flipX 핵([PlayerController.cpp:174-177](../../../apps/_MyApp_/src/InputHandler/PlayerController.cpp#L174))을 RD5 계산→push 로 교체:
```cpp
		// === RD5: facing/pose 단일 작성자 (controller 가 계산해 sink 토글) ===
		if (mSink)
		{
			const bool attacking = (mAttackTimer > 0.0f);
			if (attacking) mAttackTimer -= dt;

			// 이동 여부: 입력 벡터 크기 (mInputValue XZ).
			const vmath::vec2 velXZ(mInputValue[0], mInputValue[2]);
			const bool moving = (velXZ[0] * velXZ[0] + velXZ[1] * velXZ[1]) > 0.001f;

			const vmath::vec2 aimXZ(mAimDirection[0], mAimDirection[2]);
			Entity::EFacing facing = attacking ? Entity::Quantize4(aimXZ)
			                       : moving    ? Entity::Quantize4(velXZ)
			                                   : mLastFacing;
			Entity::EPose pose = (attacking || moving) ? Entity::EPose::Move : Entity::EPose::Idle;

			mSink->SetFacing(facing);
			mSink->SetPose(pose);
			mLastFacing = facing;
		}
```
> `mInputValue` 의 XZ 인덱싱 + `mAimDirection` XZ 는 현 controller 가 이미 보유. `Quantize4` 부호 규약(z>0=Front)은 실행 검증에서 확인 — 어긋나면 `Quantize4` 의 부호 1줄 조정.

- [ ] **Step 8: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0. (Entity CMake 에 PlayerSpriteDirector.cpp 추가 → 자동 reconfigure.)

- [ ] **Step 9: 실행 검증 (핵심 — 8방향 + facing)**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
Expected:
- 정지 → Idle 그룹(마지막 facing) 1개만 표시. 이동 → 해당 velocity 방향 Move 그룹 표시(애니 재생).
- 좌클릭 발사 → 0.15s 동안 aim 방향 facing(Move pose). 윈도우 종료 후 velocity/Idle 복귀.
- 4방향 전환 시 다른 그룹 7개는 안 보임(Visible 토글). flipX 핵 없이 좌우 방향 정상.
- **facing 부호 확인**: W(앞) → Back 또는 Front 그룹 — 어긋나면 `Quantize4` z 부호 조정 후 재빌드.

- [ ] **Step 10: 사용자 승인 후 commit**

```
[feat] : PlayerSpriteDirector 8그룹 + PlayerController RD5 단일 작성자 (PB분해 Step6)

- PlayerSpriteDirector(Component, IActorPresentation): (EFacing×EPose) 8그룹 DirGroup 소유
  SetFacing/SetPose → SpriteRenderer.Visible 토글 + 활성 B-part Play + mMoveEffect Play/Stop
- PlayerActorConfig.SpriteCfg: 단일 direction → 8그룹 DirGroupCfg 테이블. PlayerActor 8그룹 빌드+RegisterGroup
- PlayerBuilder: kPlayerGroups 8 테이블 + Life i-frame 0.5s 사후 주입 + child-scan 은퇴
- PlayerController: RD5 facing=attack?aim:move?vel:last·pose=(attack||move)?Move:Idle 계산→push
  mCachedSprite->flipX 핵 삭제 (facing = 어느 그룹이 Visible)
```

---

## Task 7: 철거 (PlayerBehavior + BulletSpawnPlayable + PlayerMovementComponents)

**Files:**
- Delete: `apps/_MyApp_/src/Entity/Player/PlayerBehavior.{h,cpp}`
- Delete: `apps/_MyApp_/src/Entity/Player/BulletSpawnPlayable.{h,cpp}`
- Delete: `<apps>/_MyApp_/src/Entity/Player/PlayerMovementComponents.h`
- Modify: `apps/_MyApp_/src/Entity/CMakeLists.txt` (3줄 제거)
- Modify: `<apps>/_MyApp_/src/Entity/Enemy/EnemyDeathHandler.h` (PB 주석 제거)

- [ ] **Step 1: 참조 0 확인**

```bash
cd apps/_MyApp_/src
grep -rn "PlayerBehavior\|BulletSpawnPlayable\|PlayerMovementComponents\|class PlayerMovement\b" .
```
Expected: 정의 파일 자신 + (있으면) PlayerEntity.h 의 dead 참조만. 다른 live 참조 0. (live 참조가 있으면 해당 Task 누락 — 먼저 해소.)

- [ ] **Step 2: 파일 삭제**

```bash
rm <Entity>/Player/PlayerBehavior.h <Entity>/Player/PlayerBehavior.cpp
rm <Entity>/Player/BulletSpawnPlayable.h <Entity>/Player/BulletSpawnPlayable.cpp
rm <Entity>/Player/PlayerMovementComponents.h
```

- [ ] **Step 3: Entity CMakeLists.txt 3줄 제거**

`apps/_MyApp_/src/Entity/CMakeLists.txt` source 목록에서 제거:
```cmake
#   <Player>/PlayerBehavior.cpp
#   <Player>/BulletSpawnPlayable.cpp
```
(PlayerMovementComponents.h 는 header-only — CMake 미등재라 제거 불필요.)

- [ ] **Step 4: EnemyDeathHandler.h PB 주석 제거**

`<apps>/_MyApp_/src/Entity/Enemy/EnemyDeathHandler.h:13` 의 "PlayerBehavior::Die self-poll 패턴 미러" 주석을 일반화:
```cpp
    ///   - 사망 self-poll — Life::IsAlive()==false 감지 시 death FX delegate + despawn.
```

- [ ] **Step 5: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0, link-clean. (CMake 자동 reconfigure. undefined symbol 없으면 철거 성공.)

- [ ] **Step 6: 실행 검증 (회귀 없음 최종 확인)**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
Expected: WASD 이동 + 8방향 스프라이트 + 좌클릭 발사 + 적 추적/접촉 데미지 + i-frame(0.5s) 전부 정상. PB 삭제로 인한 동작 변화 0 (PB 는 dead 였음).

- [ ] **Step 7: 사용자 승인 후 commit**

```
[remove] : PlayerBehavior + BulletSpawnPlayable + PlayerMovement 철거 (PB분해 Step7)

- dead god-component PlayerBehavior.{h,cpp} 삭제 (분해 완료)
- dead BulletSpawnPlayable.{h,cpp} 삭제 (D4 — Weapon::UseWeapon+onFire 가 대체)
- shadow 버그 PlayerMovementComponents.h 삭제 (DoForward(vec2) override 아님, 사용처 0)
- Entity CMake 2줄 제거 + EnemyDeathHandler.h PB 주석 일반화. grep 0참조 + link-clean
```

---

## Out of Scope (명시적 비스코프)

- **`Components::Movement` 삭제 안 함** (확정 §6.2) — 무해 dead 잔존. 비물리 액터 미래 여지.
- **dash 입력 바인딩 안 함** — `Action::Dash` 미추가. Impulse 미배선 ship. 속도싸움 suppress wiring 은 dash 바인딩 시 controller 에 (dormant).
- **적 자식 센서(hurtbox) 별도 fixture 토폴로지** — Task 5 는 동작 보존(적 body 재사용). 별도 hurtbox/hitbox 센서 child 는 물리 fixture 설계 필요 시 후속.
- **`Life::SetOnDeathFx` 단일 death 기구 통합** (§14 Step7 후속) — EnemyDeathHandler 를 Life sink 로 통합하는 건 코어 분해 범위 밖 (선택·승인 필수).
- **Attack/Hit/Die 스프라이트 상태** — 자산 없음 (Constants.h IDLE/MOVE만). pose 2축 유지.
- **단위 테스트** — `no_auto_tests`.
- **speculative 인터페이스** — `IAnimatable`/`IAttackable` 미채택.

---

## Self-Review

**1. Spec coverage (§13/§4/§14 ↔ Task):**
- §13.1 α 게이트 → Task 0 ✅ / §13.2 IActorPresentation+RD1/2/3/6 → Task 1+2 ✅ / §13.3 RD4 PlayerMovement 삭제 → Task 7 ✅ / §13.4 RD5 → Task 6 ✅ / §13.5 C1 Carrier → Task 5 ✅
- §14 Step0~7 → Task 0~7 1:1 ✅. load-bearing(§3/D5) → Task 3 ✅. IImpulsable → Task 1 ✅. Impulse → Task 4 ✅. 8그룹 → Task 6 ✅.

**2. Placeholder scan:** "TODO"/"implement later"/"similar to Task N" 없음. 모든 code step 에 실제 코드. ✅

**3. Type consistency:**
- `IActorPresentation`/`IImpulsable`/`EFacing`/`EPose`/`Quantize4` — Task 1 정의, Task 2(Life sink)·Task 5(Deliver)·Task 6(director/controller) 사용 일치.
- `Life::DoDamaged(int)`/`DoDie()`/`SetIFrameSeconds`/`SetOnDeathFx` — Task 2 정의, Task 3(IDamageable)·Task 6(SetIFrameSeconds) 사용 일치.
- `Carrier::Projectile(int)`/`ContactCarrier(int)`/`SetOwnerEntity`/`SetDamage` — Task 5 정의, bullet/enemy_factory 사용 일치.
- `PlayerSpriteDirector::RegisterGroup(EFacing,EPose,DirGroup)`/`SetFacing`/`SetPose` — Task 6 정의·사용 일치. `DirGroupCfg`{facing,pose,layers} ↔ `kPlayerGroups` ↔ PlayerActor 루프 일치.

**4. 검증된 그라운딩:** actor.h:92-93 게이트 / Life 현 본문 / Stat enum(DashForce=12,CoolDownSpeed=41,MaxHp=0) / physics_movement.h 패턴 / SpriteRenderer.Visible(MeshRenderer:61, scene_renderer:128) / Constants.h 8벡터 / bullet_factory·enemy_factory·BulletContactHandler·WeaponComponents·CMake 명시 소스 목록 — 전부 실제 코드 확인.

**5. 리스크 노트:** Task 5(가장 무거움 — 2 sub-commit + 실행 검증 2회). Task 6 facing 부호(`Quantize4`)는 실행 검증에서 1줄 조정 여지. Task 5 Spawns↔Entity 순환 link 주의(Carrier.h header-only라 link 불필요, include 경로만).

---

## 다음 세션 구현 진입점

1. 본 plan 을 `superpowers:subagent-driven-development`(권장) 또는 `superpowers:executing-plans` 로 실행.
2. **Task 0 → 7 순서 강제** (의존 그래프). 각 Task: 코드 → 빌드 exit0 → (해당 시) 실행 검증 → **사용자 승인 후 commit**.
3. Task 5 는 5a(Bullet)/5b(적) sub-commit 분리. Task 6 facing 부호는 실행 시 확인.
4. 전체 완료 후: `doc/topdown-shooter-progress.md` 갱신(M4 PlayerBehavior 분해 완료) + 핸드오프 [`doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md`](../../../doc/handoff/2026-06-01-playerbehavior-decomposition.md) §12 변경 기록 추가 + 메모리 [[next_work_playable]] 갱신.
5. 후속 후보(별도 세션): Life::SetOnDeathFx 단일 death 통합(§14 Step7 후속) / dash 입력 바인딩 + 속도싸움 suppress 활성 / 적 hurtbox 센서 child 토폴로지.
