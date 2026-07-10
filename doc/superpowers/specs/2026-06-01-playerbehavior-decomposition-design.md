# Spec — PlayerBehavior 분해 설계 (god-component → Entity/Components 분산)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **상태**: 설계 잠김 (brainstorming 완료). 본 문서 = 잠긴 결정의 직렬화. **코드 0줄.**
> **정본 입력**: [`doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md`](../../../doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md) §13(확정 기구) + §4(분해 테이블) + §14(빌드순서).
> **작성**: 2026-06-01. 브랜치 `game/module/ingame/temp`. 빌드 green.
> **구현 plan**: [`doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md`](../plans/2026-06-01-playerbehavior-decomposition.md) (별도 세션).
> **대상 독자**: 본 프로젝트 게임플레이 작업 에이전트 + 개발자.

---

## 1. 개요 & 배경

### 1.1 무엇을 / 왜

죽은(unattached) god-component `PlayerBehavior` 를 기존 `Entity/Components` taxonomy(컴포넌트별 Stat 데이터)로 **분해**한다. PB 는 5책임(locomotion / dash / survivability / attack / sprite)을 뭉친 god-component이며 3개 desync 진실을 품는다:

- `mNormalSpeed`(3.0) ↔ `PhysicsMovement.mMoveSpeed` (두 번째 사본)
- `mDead`(bool) ↔ `Life::IsAlive()` (두 진실)
- `mAttackDir` ↔ `PlayerController::mAimDirection` (조준 권위 중복)

**핵심 통찰: 이동(MIGRATE)이 아니라 대부분 삭제.** PB 가 verifiably dead(어디서도 `AddComponent` 안 됨)이므로 분해 후 동작 변화가 없다. 진짜 새로 이동하는 조각은 소수(Dash → Impulse, i-frame+Die → Life, sprite 토글 → PlayerSpriteDirector), 나머지는 죽었거나 이미 다른 곳에 존재 → 순삭제.

### 1.2 선행 가중치평가 (A1/A2) — 본 분해의 전제

직전 clean-ddd-hex 가중치평가(11 에이전트)가 확정한 결론:

- **A1 (만장일치)**: 방향 스프라이트 = `PlayerSpriteDirector`(4-레이어 그룹 + `SpriteRenderer.Visible` 토글). PB 의 단일아틀라스 `PlayClip(EPlayerClip)` 모델은 **구조적 obsolete**.
- **A2 (다수)**: 공격/피격 FX = 기존 committed delegate seam(`onFire` / `BulletContactHandler::SetOnHitFx` / enemy `onDeathFx`)에 Spawns 주입. **PB FX 슬롯 부활 금지.**

### 1.3 유일한 동작 변화 = 양성 (load-bearing 버그 수정)

[`EnemyContactHandler::OnCollisionEnter`](../../../apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp#L13)(현재 `other->GetComponent<Player::PlayerBehavior>()->Hit(mDamage)`)는 **죽은 PB::Hit 를 조회 → nullptr → 적 접촉 데미지가 완전 no-op**이다. `Components::Life::DoDamaged` 로 repoint 하면 (i-frame 게이트가 Life 로 들어가) 모든 데미지원(총알+접촉) 보호 + **접촉 데미지가 비로소 작동**한다.

---

## 2. 목표 / 비목표

### 2.1 목표

1. PB 의 모든 필드/메서드를 §4 분해 테이블대로 MIGRATE / RECONCILE / DROP.
2. 엔진 `Actor::GetComponent<T>` 게이트를 완화하여 **인터페이스 조회**(Unity `GetComponent<IInterface>` 정통)를 가능케 함.
3. 신규 컴포넌트 3종(`Impulse` / `IActorPresentation`+`IImpulsable` 인터페이스 / `PlayerSpriteDirector`) + Carrier base 추출 + Life 확장.
4. load-bearing 접촉 데미지 버그 수정.
5. 8방향 directional 스프라이트(현 PLAYER_FRONT_MOVE 1방향만 배선 → 4 facing × 2 pose = 8 그룹).
6. PB / `BulletSpawnPlayable` / `PlayerMovement`(shadow 버그) 철거.

### 2.2 비목표 (Non-Goals)

- **`Components::Movement` 삭제 안 함** (확정 §6.2) — 무해 dead 로 잔존, 비물리 액터 미래 여지 보존.
- **speculative 인터페이스 추가 안 함** — `IAnimatable` / `IAttackable` 채택 안 함 (YAGNI). `IImpulsable` 만 신규.
- **자산 없는 Attack/Hit/Die 스프라이트 상태 추가 안 함** — pose 는 `{Idle, Move}` 2축만 (Constants.h 에 IDLE/MOVE 만 존재).
- **dash 입력 바인딩 안 함** — `Impulse` 는 미배선 ship(correct-by-construction). `Action::Dash` 바인딩은 후속.
- **단위 테스트 추가 안 함** — 빌드 green + (해당 시) 실행 검증만 (`no_auto_tests` 컨벤션).
- **EnemyDeathHandler 통합 안 함 (이번)** — `Life::SetOnDeathFx` 단일 death 기구 통합은 §14 Step7 후속(선택·승인 필수, 코어 분해 범위 밖).

---

## 3. 잠긴 결정 직렬화 (handoff §13)

> 아래 전부 **재논의 금지**. 직렬화만.

### 3.1 포트 해소 α — `Actor::GetComponent<T>` 게이트 완화

엔진 [`src/scene/actor.h`](../../../src/scene/actor.h) 의 GetComponent SFINAE 게이트를 완화한다:

```cpp
// AS-IS (actor.h:92-93)
template<typename T,
         typename = std::enable_if_t<std::is_base_of_v<Component, T>>>
T* GetComponent() const;

// TO-BE
template<typename T,
         typename = std::enable_if_t<std::is_polymorphic_v<T>>>
T* GetComponent() const;
```

- **본문도 1곳 수정 필요 (구현 중 발견 — 2026-06-01 정정)**: out-of-line 정의의 fast-path 가 `static_cast<T*>(it->second.get())` 인데, `it->second.get()` 의 정적 타입은 `Component*` 다. 게이트가 `is_polymorphic_v` 로 완화되면 `T` 가 **비-Component 인터페이스**(예: `IActorPresentation`)일 때 `static_cast<인터페이스*>(Component*)` = *무관한 polymorphic 타입 간 변환* → **컴파일 에러**. fast-path 가 런타임에 안 타도(typeid 미스) 해당 표현식은 `GetComponent<Interface>()` 인스턴스화 시점에 컴파일돼야 하므로 ill-formed. **fix = fast-path 를 `if constexpr (std::is_base_of_v<Component, T>)` 로 가드** — concrete 는 static_cast 유지(핫패스 RTTI 없음), 인터페이스는 블록 폐기 → slow-path `dynamic_cast` 로만. (Task 0 빌드는 인터페이스 호출이 없어 통과했고, Task 2 Life::OnEnter 의 `GetComponent<IActorPresentation>()` 에서 처음 노출됨.)
- **타입 안전 (정정)**: concrete = `if constexpr` fast-path(`typeid(T)` 매칭 → `static_cast`, RTTI 없음). 인터페이스 = fast-path 블록이 `if constexpr` 로 폐기 → slow-path `dynamic_cast`. (이전 초안의 "fast-path 미스로 static_cast 오용 없음" 은 *오류* — 미스는 런타임 얘기, static_cast 는 인스턴스화 시점에 컴파일돼야 함.)
- **`AddComponent` / `RemoveComponent` 의 `static_assert(is_base_of_v<Component,T>)` 는 유지** ([actor.h:162](../../../src/scene/actor.h#L162), [actor.h:198](../../../src/scene/actor.h#L198)) — 생성/삭제는 concrete Component 만 가능.
- **multi-match**(한 인터페이스 2구현)는 비결정적 first 반환 → 계약: **액터당 해당 인터페이스 1구현**. 다중 수집 필요 시 `ForEachComponent + dynamic_cast`.
- **부수 작업**: [`PhysicsComponent.h:72-73`](../../../apps/_MyApp_/src/Physics/PhysicsComponent.h#L72) 의 "Physics base 로 질의 불가" stale 주석을 정정(이제 가능, *순수 비-Component 비-polymorphic* 만 불가였음).

### 3.2 연출 sink — `IActorPresentation` (RD1=OPT-1, RD2=1개, RD3=베이스-호출, RD6=silent)

- **신규 인터페이스 `IActorPresentation`** (`apps/_MyApp_/src/Physics/Components.Interfaces.h`) — `IContactable` 式 **defaulted no-op 바디**. 스프라이트/FMOD/Effekseer 타입을 **0개** 노출(deps inward).
- **RD1 OPT-1 (Template Method)**: 게임플레이 베이스의 **단일 변이지점**이 sink 로 forward. 예: `Life::DoDamaged` 안에서 HP 감소 직후 `if (mSink) mSink->ReactDamaged(dmg)`; `Life::DoDie` 에서 `mSink->ReactDied(pos)`. (`DoDamaged → virtual DoDie()` 패턴이 이미 동형으로 존재.)
- **RD3**: 베이스 컴포넌트(Life/Weapon/Impulse)가 **직접** sink 인터페이스로 호출 (per-entity 서브클래스 override 아님). 베이스는 *추상 인터페이스*만 이름지음.
- **RD2**: 액터당 sink **1개**. **RD6**: sink 없으면 silent no-op (스프라이트 없는 게임플레이-only 액터 정상).
- **`PlayerSpriteDirector : Component, IActorPresentation`** = sink 구현체(연출측).
- **functional 연출 등록 금지**: `OnXXX(std::function)` 슬롯 부활 안 함. Template-Method virtual + `IActorPresentation` sink 만.
- **유지되는 std::function seam** (OnXXX 로 안 접음): `BulletContactHandler/Carrier` 의 임팩트 FX(월드점 spawn) + `Life::SetOnDeathFx`(사망 spawn). 이건 "타겟 컴포넌트 반응"이 아니라 **Entity → Spawns spawn-at-point delegate** 라 functional 회피 대상 아님.

### 3.3 RD4 — per-entity 서브클래스 = 진짜 분기 생길 때만 (blanket 거부)

- 공유 `Life`/`Movement`/`Weapon`/`Impulse` 베이스 유지. Player/Enemy 차이 = **Stat 숫자(factory config) + 연출(sink)** 로 흡수.
- **`PlayerMovement.h` 삭제** — `DoForward(vec2)` 가 `IMovable::DoForward(vec2, float)` override 가 아닌 shadow 버그(파라미터 1개, dt 누락), 사용처 0 (검증됨).
- blanket 서브클래스 거부 사유: sibling 컴포넌트 합성과 경쟁하는 두 번째 변이축 + ~12클래스 폭발.

### 3.4 RD5 — facing/pose (PlayerController 단일 작성자)

| 상태 | 그룹 | facing |
|---|---|---|
| Idle (이동✗ 공격✗) | `(lastFacing, Idle)` | 마지막 유지 |
| Move (이동✓ 공격✗) | `(velFacing, Move)` 애니 | velocity 4방향 |
| Attack (공격✓, 이동 정지여도) | `(aimFacing, Move)` 애니 | aim 4방향 |

매 프레임 PlayerController 가 계산해 push:
```
facing = attacking ? quantize4(aim) : moving ? quantize4(vel) : last;
pose   = (attacking || moving) ? Move : Idle;
sink->SetFacing(facing); sink->SetPose(pose);
```
- **director 는 토글만** (Visible + B-part anim). velocity 를 **읽지 않는다** → §8.1 "pose 작성자 2명" 해소(작성자 1명 = PlayerController).
- 공격 종료 시: velocity 있으면 그 방향 Move, 없으면 Idle.
- 오늘의 `mCachedSprite->flipX` 핵([PlayerController.cpp:174-177](../../../apps/_MyApp_/src/InputHandler/PlayerController.cpp#L174)) **삭제** — facing = 어느 그룹이 Visible 인가.

### 3.5 C1 — Carrier 배달

```cpp
target->GetComponent<Entity::IDamageable>()->DoDamaged(dmg);   // 게이트 완화(§3.1)로 인터페이스 직접 조회
// (+ IImpulsable 넉백: target->GetComponent<Entity::IImpulsable>()->DoImpulse(knockbackDir))
```
- 다중 effect fan-out 시만 `ForEachComponent + dynamic_cast`.
- C2 base+subtypes(Projectile / ContactCarrier) / C3 EnemyContactHandler 흡수 + 적 자식 센서 Carrier / C4 `SetOnHitFx` 흡수 — §5.6.

---

## 4. 분해 테이블 (handoff §4 — PB 전 필드/메서드)

| PB 조각 | 목적지 | 액션 | 비고 |
|---|---|---|---|
| `mSpriteSeq` / `mCurrentClip` / `EPlayerClip` | PlayerSpriteDirector | RECONCILE | 단일 seq 폐기 → per-(EFacing,EPose) DirGroup. 2축(EFacing×EPose) 분리 |
| `PlayClipInternal` | PlayerSpriteDirector private `Apply()` | MIGRATE | Visible 토글 + 활성 그룹 B-part Play |
| 속도기반 Idle↔Move 자동스왑 | (없음) — PlayerController 가 계산·push | RECONCILE | RD5 단일 작성자. director 자가구동 안 함 |
| "Hit/Attack clip 끝→Idle" poll | — | **DROP** | 끝낼 clip 없음(자산 없음) |
| `mMoveEffect` | PlayerSpriteDirector `SetMoveEffect(IPlayable*)` | MIGRATE | Move 진입 Play / 이탈 Stop. sprite 동거 FX 슬롯 |
| `mAttackPlayable` | — `onFire` Composite seam | **DROP** | A2. [PlayerBuilder.cpp:47-73](../../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L47) inline |
| `mHitPlayable` | — `SetOnHitFx` seam | **DROP** | A2. 플레이어 피격 FX 기본 없음(YAGNI) |
| `mDashPlayable` | — 미래 dash 바인딩 site | **DROP** | A2. dash 바인딩 아직 없음 |
| `mDiePlayable` | **`Life::SetOnDeathFx`** delegate | RECONCILE | `std::function<void(const vec3&)>`. EnemyDeathHandler 동일 패턴 |
| `mAttackDir` / `GetAttackDirection` | — `PlayerController::mAimDirection` | **DROP** | 유일 소비자 BulletSpawnPlayable 도 dead |
| `mNormalSpeed`(3.0) | — `PhysicsMovement.mMoveSpeed` | **DROP** | 죽은 세 번째 사본 |
| `Move(vel)` body | — `PhysicsMovement::DoForward` | **DROP** | 두 번째 이동기구 금지 |
| `Idle()` / `Attack(dir)` | — | **DROP** | flat verb 은퇴. Weapon 변경 ~0 |
| `Hit(damage)` | **`Life::DoDamaged`** (게이트 fold) | RECONCILE | **load-bearing** (§1.3) |
| `Dash(dir)` | **신규 `Impulse::DoImpulse(dir)`** | MIGRATE | Player Dash + Monster Knockback |
| `mDashSpeed`(7.5) | `Impulse.mImpulseForce` = Stat(DashForce) | MIGRATE | ✓존재 (값 12) |
| `mDashDuration`(0.3) | `Impulse.mDurationSec` plain float | MIGRATE | 맞는 enum 없음 |
| `mDashCooldown`(0.8) | `Impulse.mCooldown` = Stat(CoolDownSpeed) | MIGRATE | 이름=rate인데 sec 저장(semantic flag) |
| `mDashTimer` / `mDashCooldownTimer` | `Impulse.mActiveTimer` / `mCooldownTimer` plain | MIGRATE | |
| `IsDashing()` | `Impulse.IsActive()` | MIGRATE | controller 가 속도 양보(§3.4 / Step4) |
| `mHitInvincibility`(0.5) | **`Life.mIFrameSeconds`** plain float | MIGRATE | 확정 §6.1 (Stat 아님) |
| `mInvincibilityTimer` | `Life.mInvincibleTimer` plain | MIGRATE | |
| `mDead`(bool) | (없음) → `mDeathFxFired` latch | **DROP** | desync 제거. `IsAlive()`(mCurHp<=0) 파생 |
| `Die()` | **`Life::DoDie()`** (현재 empty) | MIGRATE | one-shot guard → onDeathFx(pos) → SetActive(false) |
| `IsAlive()` | `Life::IsAlive()` 이미 존재 | **DROP** | PB 는 forwarder였음 |
| `Update(dt)` god | **3분할**: Impulse(타이머) + Life(i-frame+death) + (PlayerController: pose) | RECONCILE | 각자 자기 런타임만 |
| `Init`/`SetBody`/`SetSceneRoot` / `mBody`/`mSceneRoot` | (없음) | **DROP** | per-component ctor + `FindPhysics(GetOwner())` |

---

## 5. 신규/확장 컴포넌트 설계 (실제 시그니처)

> 시그니처는 현재 헤더와 정합 검증됨(드리프트 0). 코드 스타일 = Client PascalCase, 헤더가드 `__XXX_H__`(또는 기존 `_XXX_` 관용), `#pragma once` 미사용.

### 5.1 `IActorPresentation` — 신규 (`apps/_MyApp_/src/Physics/Components.Interfaces.h`)

`IContactable` 양식([apps/_MyApp_/src/Physics/Components.Interfaces.h:25-41](../../../apps/_MyApp_/src/Physics/Components.Interfaces.h#L25)) 모델 — protected ctor, copy/move delete, **defaulted no-op 바디**:

```cpp
namespace TopdownShooter::Entity
{
    enum class EFacing : int { Front = 0, Back, Left, Right };
    enum class EPose   : int { Idle  = 0, Move };

    /// @brief 게임플레이 베이스 → 연출 sink (Template-Method forward 대상). RD1 OPT-1.
    /// @details IContactable 式 defaulted no-op — director 가 쓰는 verb 만 override.
    ///          스프라이트/FMOD/Effekseer 타입 0개 (deps inward).
    class IActorPresentation
    {
      protected:
        IActorPresentation() = default;
      public:
        virtual ~IActorPresentation() = default;
        IActorPresentation(const IActorPresentation&) = delete;
        IActorPresentation& operator=(const IActorPresentation&) = delete;
        IActorPresentation(IActorPresentation&&) = delete;
        IActorPresentation& operator=(IActorPresentation&&) = delete;

        virtual void ReactDamaged(int /*dmg*/)             {}  // 피격 flash 등 (자산 있으면)
        virtual void ReactDied   (vmath::vec3 /*pos*/)     {}  // 사망 연출
        virtual void ReactAttack (vmath::vec2 /*aimDir*/)  {}  // 공격 연출 (현재 자산 없음 → 미사용)
        virtual void FaceAim     (vmath::vec2 /*aimDir*/)  {}  // 편의 — sink 가 quantize (현재 controller 가 quantize)
        virtual void SetFacing   (EFacing /*facing*/)      {}  // RD5 live 경로
        virtual void SetPose     (EPose   /*pose*/)        {}  // RD5 live 경로
    };
}
```

> `EFacing`/`EPose` 를 `Components.Interfaces.h`(또는 인접 작은 enum 헤더)에 둬 PlayerController·Director·config 가 공유. `quantize4(vmath::vec2) → EFacing` 자유 함수도 인접 배치.

### 5.2 `IImpulsable` — 신규 (`apps/_MyApp_/src/Physics/Components.Interfaces.h`)

`IMovable` 스타일(protected ctor, copy/move delete, pure virtual). **단일 메서드**:

```cpp
class IImpulsable
{
  protected:
    IImpulsable() = default;
  public:
    virtual ~IImpulsable() = default;
    IImpulsable(const IImpulsable&) = delete;
    IImpulsable& operator=(const IImpulsable&) = delete;
    IImpulsable(IImpulsable&&) = delete;
    IImpulsable& operator=(IImpulsable&&) = delete;

    /// @brief 단위 방향 dir 로 일회성 타임드 속도 버스트. DoForward(지속)의 대칭 파트너.
    virtual void DoImpulse(vmath::vec2 dir) = 0;
};
```

> **IMovable 오버로드 금지** — `PlayerController::SetMovableTarget(IMovable*)` 계약([PlayerController.h:52](../../../apps/_MyApp_/src/InputHandler/PlayerController.h#L52))을 깨지 않기 위해 신규 인터페이스.

### 5.3 `Life` 확장 (`apps/_MyApp_/src/Entity/Components/LifeComponents.h`)

> **⚠ 구현 갱신 (2026-06-02, `f1e1a23`)**: Task 2 의 본 설계(plain `float mIFrameSeconds`/`mInvincibleTimer`/`mDeathDelaySeconds`/`mDying`/`mDeathTimer`)는 직후 **`SJH::Timer` 마이그레이션**으로 `std::optional<SJH::Timer::Timer> mInvincibleTimer` + `mDieTimer` (+ `ArmInactive(slot, s)` 헬퍼)로 **재작성·커밋**됨. **공개 계약은 동일 — Task 6 영향 없음**: `SetIFrameSeconds(float)`/`SetDeathDelaySeconds(float)`/`SetOnDeathFx`/`IsInvincible()`/`DoDamaged`/`DoDie` + **sink forwarding `ReactDamaged`/`ReactDied` 인택트**. "부재=nullopt"(DDD Value Object) 패턴 — `SetIFrameSeconds(s>0)`=`emplace(s)+Tick(s)`(finished 장전, 스폰 즉시 무적 방지), `s<=0`=`reset()`. 아래 float 코드블록은 **원설계 의도 기록**(현행은 optional<Timer>). 사망지연 길이↔디졸브 길이(0.5s)는 공유 상수 매칭(FX spec §4.3). 정본 = 현재 `LifeComponents.h` + `doc/handoffs/2026-06-02/2026-06-02-timer-migration-followup-agent-prompt.md` §1.2/§2.

현재 `Life`([LifeComponents.h](../../../apps/_MyApp_/src/Entity/Components/LifeComponents.h)) = `Component, ILivable, IDieable, IDamageable`. `mMaxHp`(Stat) + `mCurHp`(int) + 빈 `DoDie()`. 확장(원설계 — 현행은 위 노트의 optional<Timer>):

```cpp
class Life : public SJH::Scene::Component,
             public ILivable, public IDieable, public IDamageable
{
  protected:
    Algebraic::Numeric::Stat mMaxHp;
    int   mCurHp;
    // === 신규 (i-frame: plain float 확정 §6.1) ===
    float mIFrameSeconds  = 0.0f;     // PB mHitInvincibility 미러 (기본 0 = 무적 없음)
    float mInvincibleTimer = 0.0f;
    bool  mDeathFxFired   = false;    // one-shot death guard (mDead 대체)
    std::function<void(const vmath::vec3&)> mOnDeathFx;          // spawn-at-point seam (§3.2)
    IActorPresentation* mSink = nullptr;  // OnEnter 1회 캐시 (§3.1)

  public:
    Life();                                   // (기존)
    Life(int max_hp, int cur_hp = -1);        // (기존)
    Life(int max_hp, int cur_hp, float iframe);  // 신규 — iframe 주입

    // Fluent setter (PlayerBuilder 주입)
    Life& SetIFrameSeconds(float s) { mIFrameSeconds = s; return *this; }
    Life& SetOnDeathFx(std::function<void(const vmath::vec3&)> fx) { mOnDeathFx = std::move(fx); return *this; }
    bool  IsInvincible() const { return mInvincibleTimer > 0.0f; }

    void OnEnter() override;     // mSink = GetComponent<IActorPresentation>()  (ctor 금지)
    void Update(float dt) override;  // i-frame 감소 + (안전망) death poll
    void OnExit() override {}

    bool IsAlive() const override { return 0 < mCurHp; }
    int  GetHp() const override { return mCurHp; }
    int  GetMaxHp() const override { return static_cast<int>(mMaxHp.GetValue()); }

    void DoDamaged(int damage) override;  // i-frame 게이트 fold + ReactDamaged + arm timer + DoDie
    void DoDie() override;                // one-shot → ReactDied + onDeathFx(pos) → SetActive(false)
};
```

`DoDamaged` 본문 (게이트 fold — PB::Hit 의 i-frame 로직 흡수):
```cpp
void Life::DoDamaged(int damage) {
    if (IsInvincible()) return;                 // i-frame early-return (모든 데미지원 보호)
    mCurHp -= damage;
    if (mSink) mSink->ReactDamaged(damage);     // Template-Method forward
    mInvincibleTimer = mIFrameSeconds;          // arm i-frame
    if (!IsAlive()) { mCurHp = 0; DoDie(); }
}
```
`DoDie` 본문:
```cpp
void Life::DoDie() {
    if (mDeathFxFired) return;                  // one-shot
    mDeathFxFired = true;
    const vmath::vec3 pos = GetOwner() ? GetOwner()->GetTransform().Translate : vmath::vec3(0.0f);
    if (mSink) mSink->ReactDied(pos);
    if (mOnDeathFx) mOnDeathFx(pos);            // spawn-at-point seam
    if (GetOwner()) GetOwner()->SetActive(false);
}
```
헤더 의존 추가: `<functional>`, `<vmath.h>`. (header-only 유지 가능.)

### 5.4 `Impulse` — 신규 (`<Physics>/physics_impulse.h`)

```cpp
namespace TopdownShooter::Physics
{
    class Impulse : public SJH::Scene::Component, public Entity::IImpulsable
    {
      public:
        Impulse()
          : mImpulseForce(7.5f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
            mCooldown    (0.8f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed) {}

        void OnEnter() override { mBody = Components::FindPhysics(GetOwner()); }  // 비소유
        void OnExit()  override { mBody = nullptr; }
        void Update(float dt) override {                       // 타이머 감소만
            if (mActiveTimer   > 0.0f) mActiveTimer   -= dt;
            if (mCooldownTimer > 0.0f) mCooldownTimer -= dt;
        }

        void DoImpulse(vmath::vec2 dir) override;              // cd/active 게이트 → SetLinearVelocity(n*force) XZ→XY → arm
        void ApplyKnockback(vmath::vec2 fromXZ) { DoImpulse(fromXZ); }  // convenience (Monster Knockback)
        bool IsActive() const { return mActiveTimer > 0.0f; }

      private:
        Algebraic::Numeric::Stat mImpulseForce;   // = Stat(DashForce, base 7.5)
        Algebraic::Numeric::Stat mCooldown;       // = Stat(CoolDownSpeed, base 0.8 — sec 저장)
        float mDurationSec   = 0.3f;              // plain (맞는 enum 없음)
        float mActiveTimer   = 0.0f;
        float mCooldownTimer = 0.0f;
        Components::Physics* mBody = nullptr;     // FindPhysics — PhysicsMovement 와 동일 b2Body 취급
    };
}
```
`DoImpulse` 본문 (PB::Dash 미러, XZ→Box2D XY = Z→-Y):
```cpp
void Impulse::DoImpulse(vmath::vec2 dir) {
    if (mCooldownTimer > 0.0f || IsActive()) return;
    if (!mBody || !mBody->GetBody()) return;
    const float len = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1]);
    if (len <= 0.001f) return;
    vmath::vec2 n(dir[0]/len, dir[1]/len);
    const float force = mImpulseForce.GetValue();
    mBody->GetBody()->SetLinearVelocity(b2Vec2(n[0]*force, -n[1]*force));
    mActiveTimer   = mDurationSec;
    mCooldownTimer = mCooldown.GetValue();
}
```
- **거주**: `Physics/` (PhysicsMovement 옆, b2Body 동일 취급). D2 = (a).
- **invincibility/FX 슬롯 부착 금지** (미니 god 방지) — i-frame = Life, FX = seam.
- **속도싸움(Step4)**: `Impulse`·`PhysicsMovement` 둘 다 `SetLinearVelocity`. 잠긴 해소 = **controller 가 `Impulse::IsActive()` 중 `DoForward` suppress**. 단 dash 입력 미배선이라 이 wiring 은 `Action::Dash` 바인딩 시 활성(현재 dormant) — `Impulse` 는 미배선 ship.

### 5.5 `PlayerSpriteDirector` — 신규 (`Entity/Player/PlayerSpriteDirector.{h,cpp}`)

```cpp
namespace TopdownShooter::Entity::Player
{
    class PlayerSpriteDirector : public SJH::Scene::Component, public IActorPresentation
    {
      public:
        struct DirGroup {
            std::array<SJH::Sprite::SpriteRenderer*, 4> layers{};  // E·H·B·F 4-레이어
            SJH::SpriteSequence::SpriteSequencePlayable* bPart = nullptr;  // ColCount>1 애니 (B) 레이어
        };

        // 빌드 시 그룹 등록 (CreatePlayerActor)
        PlayerSpriteDirector& RegisterGroup(EFacing f, EPose p, DirGroup g);
        // 단발 generic FX 주입 훅 (EPlayerClip enum 없음, 자산 의존 없음)
        void SetMoveEffect(SJH::Playable::IPlayable* p) { mMoveEffect = p; }
        void PlayOneShot  (SJH::Playable::IPlayable* p);

        // === IActorPresentation (sink — siblings 가 push) ===
        void SetFacing(EFacing f) override;   // Apply()
        void SetPose  (EPose   p) override;    // Apply() + Move 진입/이탈 시 mMoveEffect Play/Stop
        // ReactDamaged/ReactDied/ReactAttack/FaceAim 은 자산 있으면 override, 없으면 defaulted no-op

        EFacing CurrentFacing() const { return mFacing; }
        EPose   CurrentPose()   const { return mPose; }

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override {}     // **velocity 안 읽음** (RD5 — controller 단일 작성자)

      private:
        void Apply();   // (mFacing,mPose) 그룹의 4레이어 Visible=true, 나머지 전부 false; B-part Play

        DirGroup mGroups[4][2]{};   // [EFacing][EPose]
        EFacing  mFacing = EFacing::Front;
        EPose    mPose   = EPose::Idle;
        SJH::Playable::IPlayable* mMoveEffect = nullptr;
    };
}
```
- **presentation-only LEAF**: physics/Stat/movement/input **없음**. siblings 가 push, 자신은 never call back.
- `Apply()` 토글 기구: `SpriteRenderer.Visible`(base `MeshRenderer.Visible`, [scene_renderer.cpp:128](../../../src/render/scene_renderer.cpp#L128) 가 컬링에 사용) 으로 활성 그룹만 보이게.
- `SetVelocitySource` **없음** — RD5(controller 단일 작성자)가 §8.1(pose 작성자 2명)을 해소.

### 5.6 Carrier base + subtypes (`Spawns/`)

**얇은 베이스: 배달만** (확정 §6.4):

```cpp
namespace TopdownShooter::Spawn::Carrier
{
    /// @brief 데미지 배달 공통 베이스 (얇음). 수명/센서/이동은 subtype 책임.
    class CarrierBase : public SJH::Scene::Component, public Physics::IContactable
    {
      protected:
        SJH::Scene::Actor* mOwnerEntity = nullptr;   // 자기 발사자 (자가 피해 방지)
        float mDamage = 0.0f;
        std::function<void(const vmath::vec3&)> mOnHitFx;   // SetOnHitFx 흡수 (C4 — spawn-at-point seam 유지)

        /// @brief C1 배달: target 의 IDamageable→DoDamaged + (있으면) IImpulsable→DoImpulse 넉백.
        void Deliver(SJH::Scene::Actor* target, vmath::vec2 knockbackDir);

      public:
        CarrierBase& SetOwnerEntity(SJH::Scene::Actor* o) { mOwnerEntity = o; return *this; }
        CarrierBase& SetDamage(float d) { mDamage = d; return *this; }
        CarrierBase& SetOnHitFx(std::function<void(const vmath::vec3&)> fx) { mOnHitFx = std::move(fx); return *this; }
    };

    // flying + lifetime + self-despawn (기존 Projectile 진화 — Component+IContactable+IDieable)
    class Projectile : public CarrierBase, public Entity::IDieable { /* OnCollisionEnter → Deliver + DoDie */ };

    // sensor + persistent (적 자식 센서 — EnemyContactHandler 흡수, C3)
    class ContactCarrier : public CarrierBase { /* OnTriggerEnter/OnCollisionEnter → Deliver, despawn 없음 */ };
}
```
`Deliver` 본문:
```cpp
void CarrierBase::Deliver(SJH::Scene::Actor* target, vmath::vec2 knockbackDir) {
    if (!target || target == mOwnerEntity || !target->IsActive()) return;
    if (auto* dmg = target->GetComponent<Entity::IDamageable>())  dmg->DoDamaged(static_cast<int>(mDamage));
    if (auto* imp = target->GetComponent<Entity::IImpulsable>())  imp->DoImpulse(knockbackDir);
    if (mOnHitFx) mOnHitFx(target->GetTransform().Translate);   // seam 유지
}
```
- 현 [`Projectile.h`](../../../apps/_MyApp_/src/Spawns/Projectile.h)(`namespace Spawn::Carrier`, Component+IContactable+IDieable, mOwnerEntity/mDamageInfo/SetUp)는 `CarrierBase` 로 공통 추출하며 진화.
- C4: `BulletContactHandler::SetOnHitFx` 의 임팩트 FX seam → `CarrierBase.mOnHitFx` 로 흡수 (functional 유지 — Entity→Spawns spawn-at-point).

### 5.7 PlayerController 변경 (RD5)

[`PlayerController`](../../../apps/_MyApp_/src/InputHandler/PlayerController.h) — 매 프레임 RD5 규칙 계산 → `IActorPresentation` sink push:

- 신규 멤버: `IActorPresentation* mPresentationSink = nullptr` (또는 `GetComponent` 캐시), `float mAttackWindowSec = 0.15f`(확정 §6.3), `float mAttackTimer = 0.0f`.
- `OnFirePressed`/발사 시: `mAttackTimer = mAttackWindowSec` (공격 윈도우 arm).
- `Update(dt)`: aim/velocity 로 facing/pose 계산 → `sink->SetFacing/SetPose`. **`mCachedSprite->flipX` 핵 삭제**.
```
attacking = (mAttackTimer > 0.0f);  if (attacking) mAttackTimer -= dt;
moving    = (입력/속도 > 임계);
facing = attacking ? quantize4(aimXZ) : moving ? quantize4(velXZ) : mLastFacing;
pose   = (attacking || moving) ? EPose::Move : EPose::Idle;
sink->SetFacing(facing); sink->SetPose(pose); mLastFacing = facing;
```
- `SpriteRenderer* mCachedSprite` 멤버 은퇴 (facing = Visible 그룹).

---

## 6. 확정된 열린 디테일 (Phase 0 사용자 승인)

| # | 결정 | 선택 | 근거 |
|---|---|---|---|
| **6.1** i-frame 저장 | **plain float** (`mIFrameSeconds`/`mInvincibleTimer`) | handoff §4 라인근접 권고. 최소 변경. Stat(Tenacity) 전환은 후일 (Tenacity=끈기 ≠ i-frame 초, semantic 불일치 회피) |
| **6.2** Components::Movement 운명 | **유지** (삭제 안 함) | §14(최신 잠금)이 Movement 삭제 미포함 → 일관. 무해 dead 잔존, 비물리 액터 여지 보존. 비목표(§2.2)에 명시 |
| **6.3** 공격 윈도우 | **짧은 타이머 ~0.15s** (`PlayerController.mAttackWindowSec`) | 단발 무기에 자연스러운 짧은 aim-lock. fire 시 arm, 그 동안 facing=quantize4(aim) |
| **6.4** Carrier 베이스 두께 | **얇은 베이스 (배달만)** | C1 배달 로직만 공유. lifetime/sensor 는 Projectile/ContactCarrier subtype 책임 |

---

## 7. clean-ddd-hexagonal 매핑

| 패턴 | 본 분해의 적용 |
|---|---|
| **God Aggregate 제거** | PB(5책임 god) → Life / Impulse / PlayerSpriteDirector / (기존) PhysicsMovement·Weapon 단일책임 컴포넌트로 분할 |
| **Anemic Domain Model 회피** | behavior-rich — Life 가 자기 damage/i-frame/death 로직 소유(서비스 아님). DoDamaged/DoDie = 도메인 행위 |
| **Dependency Rule (inward)** | gameplay/combat 컴포넌트 → push → PlayerSpriteDirector(LEAF, never calls back). controller → `IMovable`/`IImpulsable` port. Entity → Spawns via std::function delegate(의존 역전) |
| **Ports & Adapters** | **Port** = `IActorPresentation`/`IDamageable`/`IImpulsable`/`IMovable` (게임플레이 베이스가 *추상 인터페이스*만 이름지음). **Adapter** = `PlayerSpriteDirector`(IActorPresentation 구현). 베이스는 sink 의 concrete 타입(스프라이트/FMOD)을 모름 |
| **Template Method** | `Life::DoDamaged`/`DoDie` = 단일 변이지점이 sink 로 forward (RD1 OPT-1). 기존 `DoDamaged→virtual DoDie()` 와 동형 |
| **Value Object** | `Stat`(불변 BaseValue + modifier) = config 수치 값 객체. PlayerActorConfig = PoD config(Value Object 의미) |
| **YAGNI / 비-premature** | speculative `IAnimatable`/`IAttackable` 미채택. dash+knockback = 단일 `Impulse` (separate 안 함) |
| **dynamic_cast 비용** | 인터페이스 조회 slow-path O(N)는 액터당 컴포넌트 소수(자식 미순회)라 무해. 핫패스는 OnEnter 1회 캐시(`Life.mSink`) |

---

## 8. 4-엔진 정통 매핑

| 결정 | 엔진 정통 |
|---|---|
| `GetComponent<Interface>` (게이트 완화) | **Unity** `GetComponent<IInterface>()` — 컴포넌트가 구현한 인터페이스로 조회 |
| Carrier (hitbox/hurtbox) | **격투/슈터 정통** — hitbox(Carrier) 가 hurtbox(IDamageable) 에 데미지 배달. owner 자가 피해 방지 |
| `IActorPresentation` sink (defaulted no-op) | **Unreal** `Gameplay → Cosmetic` 분리 / **Cocos** action vs node. Template-Method 훅 |
| `Visible` 토글 directional | **Cocos/Unity** 2D — 방향별 노드/스프라이트 set active. flipX 대신 그룹 가시성 |
| std::function spawn-at-point seam | **BulletFactory delegate** 정통 (Entity→Spawns 단방향, 의존 역전) |
| Impulse vs Movement 대칭 | DoImpulse(일회 타임드) ⟂ DoForward(지속) — 별도 인터페이스 (계약 분리) |

---

## 9. 참조 그래프 (의존 방향 — 전부 inward, acyclic)

```
gameplay/combat 컴포넌트 (Life/Weapon/Impulse) ──push──▶ PlayerSpriteDirector (LEAF — never calls back)
PlayerController ──▶ IMovable / IImpulsable / IActorPresentation (ports)
모든 컴포넌트 ──FindPhysics──▶ Components::Physics (b2Body)
Entity ──std::function delegate──▶ Spawns (의존 역전 보존)
Carrier ──GetComponent<IDamageable/IImpulsable>──▶ 타겟 컴포넌트
```
- **FX seam 발화처** (A2 committed): `onFire`([PlayerBuilder.cpp:47](../../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L47)) / `BulletContactHandler::SetOnHitFx`(→Carrier) / **`Life::SetOnDeathFx`**(신규).
- **데미지 라우팅**: `EnemyContactHandler` → `Life::DoDamaged` repoint (Step3) → 이후 ContactCarrier 흡수 (Step5).
- **생성**: `CreatePlayerActor` 가 8 child-actor 그룹 빌드(현 PLAYER_FRONT_MOVE-only 루프 [PlayerActor.cpp:86-118](../../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L86) 확장) → `RegisterGroup`. PlayerBuilder 의 child-scan([PlayerBuilder.cpp:115-123](../../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L115))은 은퇴/repoint.

---

## 10. 절대 하지 말 것 (handoff §10 + 금지사항)

- `IMovable` 오버로드(SetMovableTarget 계약) → 신규 `IImpulsable`.
- `Components::Movement` 로 walk speed 이전(세 번째 죽은 사본). speed = `PhysicsMovement.mMoveSpeed`.
- `Impulse` 에 invincibility/FX 슬롯 부착(미니 god). i-frame=Life, FX=seam.
- PB FX 슬롯 부활(A2). FX = `onFire`/`SetOnHitFx`/`onDeathFx` delegate.
- 단일 `SpriteRenderer` flipX/PlayClip 로 방향 표현(A1). 방향 = 그룹 `Visible`.
- pose 작성자 2명 / 속도 `SetLinearVelocity` 충돌 방치 — controller 가 `Impulse::IsActive()` 중 DoForward suppress.
- 자산 없는 Attack/Hit/Die 스프라이트 상태 추가.
- `functional` OnXXX 연출 등록 → Template-Method virtual + `IActorPresentation` sink.
- blanket per-entity 서브클래스 (RD4) → 공유 베이스 + Stat config + sink. `PlayerMovement` 삭제.
- ctor 에서 virtual/sink 해소 → `OnEnter`. 액터당 Life-family/IActorPresentation 1구현.
- `doc/`(gitignore) 에 추적 문서 — 추적 핸드오프는 `doc/`(단수).

---

## 11. 빌드 순서 (handoff §14 — plan 의 Task 분해 입력)

| Step | 작업 | 경합 |
|---|---|---|
| **0** | 엔진 `GetComponent<T>` 게이트 `is_base_of<Component>`→`is_polymorphic` (actor.h) + `PhysicsComponent.h:73` 주석 정정 | 비경합·전제 |
| **1** | `IActorPresentation`(defaulted) + `IImpulsable`(DoImpulse) + `EFacing`/`EPose`/`quantize4` 추가 (Entity/Components) | 비경합 |
| **2** | `Life` 확장: i-frame fold + `DoDie` 구현 + `SetOnDeathFx` + OnEnter sink 캐시 + DoDamaged/DoDie 에서 React* | 비경합 |
| **3** | `EnemyContactHandler` → `Life::DoDamaged` repoint (load-bearing) + PlayerBehavior.h include 제거 | Entity |
| **4** | `Impulse`(physics_impulse.h, IImpulsable, DashForce/CoolDownSpeed Stat) + 속도싸움 suppress 계약(미배선 ship) | Physics |
| **5** | `CarrierBase`+subtypes(Projectile/ContactCarrier) — `GetComponent<IDamageable>` 배달. Bullet→Carrier / 적 자식 센서 Carrier. SetOnHitFx·EnemyContactHandler 흡수 | Entity/Spawn |
| **6** | `PlayerSpriteDirector : Component, IActorPresentation` — 8그룹 빌드 + SetFacing/SetPose Visible 토글. PlayerController RD5 계산→push, flipX 핵 삭제 | **main.cpp Fog Agent 경합** |
| **7** | 철거: `PlayerBehavior.{h,cpp}` + `BulletSpawnPlayable.{h,cpp}` + `PlayerMovement.h`. grep 0참조. link-clean | — |

> M6 잔여(R1a/R1b FX 배선)는 Step 2-3·5 의 delegate-seam/Carrier 에 자연 합류.

---

## 12. self-review (스펙 검증)

- ✅ §13 전 결정 커버: α 게이트(3.1) / IActorPresentation+RD1/2/3/6(3.2) / RD4(3.3) / RD5(3.4) / C1(3.5).
- ✅ §4 분해 테이블 전 항목 직렬화 (4절).
- ✅ 시그니처 실제 헤더 정합 (드리프트 0 검증): actor.h 게이트, <Life>/Stat/PhysicsMovement/IMovable/IContactable/Projectile/SpriteRenderer.Visible/Constants.h 8벡터/Stat enum 값.
- ✅ 4 열린 디테일 확정 반영 (6절).
- ✅ clean-ddd 매핑(7) + 4-엔진 정통(8) + 비목표(2.2/10).
- ✅ 코드 0줄 (문서만). 빌드 순서(11) = plan 입력.
- ⚠ **plan 시 주의**: Step6 `SpriteCfg` 8그룹 config 형태(현 `direction` 단일 vector* → 8 그룹 테이블) 는 plan Task 에서 구체화. Step4 속도싸움 wiring 은 dash 바인딩까지 dormant. Step5 Carrier 가 Step3 repoint 를 흡수(중복 배달 코드 통합).
