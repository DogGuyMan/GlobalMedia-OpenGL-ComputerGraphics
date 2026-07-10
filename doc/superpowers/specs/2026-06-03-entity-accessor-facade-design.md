# Entity Accessor-facade 설계 — BaseEntity 공유 + PlayerEntity/EnemyEntity + Physics Component화

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

- **날짜**: 2026-06-03
- **브랜치**: `game/module/ingame/temp`
- **상태**: 설계 확정 (구현 plan 대기)
- **대상 모듈**: `apps/_MyApp_/src/Entity/*`, `apps/_MyApp_/src/Physics/*`, `apps/_MyApp_/src/Bootstrap/*` (Player/Enemy 빌드 경로)
- **gitignore**: 본 문서 경로(`doc/superpowers/specs/`)는 프로젝트 컨벤션상 gitignored(로컬 전용). 추적 필요 시 `doc/handoff/` 핸드오프로 별도.

---

## 1. 동기 & 문제

외부 객체(적 AI / HUD / `Carrier` / 디버그)가 Player·Enemy 액터의 능력에 접근할 때 `actor->GetComponent<각각>()` 을 **상시 호출** → (1) 불필요·장황, (2) 인터페이스 조회(`GetComponent<IDamageable>`)는 `Actor::GetComponent` 의 O(N) `dynamic_cast` slow-path([src/scene/actor.h](../../../src/scene/actor.h)).

**목표**: 각 엔티티에 **Accessor-facade Component** 를 1개 부착해, 외부가 facade 하나를 **1회 resolve → 캐시된 형제 컴포넌트에 cheap accessor/verb 로 접근**. facade 자체는 *로직 0* — `OnEnter` 에서 형제 포인터를 캐시(비소유)하고 위임/노출만 한다 (Accessor + Component 캐싱).

```cpp
// Before — GetComponent 산재
auto* life = a->GetComponent<Components::Life>();  if (life && life->IsAlive()) ...
auto* mv   = a->GetComponent<Physics::PhysicsMovement>();  if (mv) mv->DoForward(d, dt);
auto* wp   = a->GetComponent<Components::Weapon>();        if (wp) wp->UseWeapon(aim);

// After — facade 1회 resolve
auto* pe = a->GetComponent<PlayerEntity>();   // 1회 (또는 포인터 보관)
if (pe->IsAlive()) ...
pe->GetMovement()->DoForward(d, dt);
pe->Attack(aim);
```

## 2. 레퍼런스 (사용자 Unity/C# 프로젝트 — ProjectLamb_Sophia)

`Entity`(abstract `MonoBehaviour`, `IXxxAccessible` 다중구현)가 composite 들을 **보유·캐시**하고 `GetXxxComposite()` accessor + 포워딩 verb(`GetDamaged()/Die()/Dash()/Attack()`)를 제공. `Player : Entity`, (`Monster : Entity`). composite(`LifeComposite`/`MovementComposite`/`DashSkill`/`WeaponManager`)는 각자 `Stat` 보유 + 도메인 인터페이스 구현. `Entity` 베이스가 `entityRigidbody`/`entityCollider` 를 `TryGetComponent` 로 캐시.

**C# ↔ C++ 매핑**

| C# Sophia | C++ (본 프로젝트) |
|---|---|
| `Entity`(abstract, `IXxxAccessible`) | `BaseEntity` (Component, 도메인 인터페이스 포워딩) |
| `Player : Entity` | `PlayerEntity : BaseEntity` |
| `Monster : Entity` | `EnemyEntity : BaseEntity` |
| `entityRigidbody` / `entityCollider` (캐시) | `Physics` 컴포넌트 (`GetPhysics()`) — Box2D 가 body+fixture 융합이라 하나 |
| `LifeComposite` (plain class) | `Components::Life` (Component) |
| `MovementComposite` | `Physics::PhysicsMovement` (IMovable) |
| `DashSkill` | `Physics::Impulse` (verb `Dash`) |
| `WeaponManager` | `Components::Weapon` |
| `ModelManager`/`AudioManager`/`VisualFXBucket` (3분리) | `PlayableDirector` **1개** (named playable 이 visual+FMOD+PostFX 통합) → `GetDirector()` |

**구조적 차이(의도)**: C# 은 composite 를 `new` 로 보유(일부) + sibling `MonoBehaviour`(일부). C++ 은 **전부 Actor 소유 Component** → facade 는 포인터 캐시(비소유). C# 이 순수 클래스로 Life/Movement 를 분리한 이유(불필요한 lifecycle 메서드 상속 회피)는 C++ 에선 못 살림(우리 Component 는 Update 필요 — Life i-frame Tick, Impulse 쿨다운). 그래서 형제는 Component, facade 는 그 위 Accessor 레이어.

## 3. 잠긴 결정 (재논의 금지)

- **D1 소유 모델 = cache-sibling**: `CreateXActor` 가 형제 컴포넌트를 부착, facade 가 `OnEnter` 에서 포인터 캐시(비소유). (`Actor` = GameObject 가 컴포넌트 소유, facade = 접근 레이어.)
- **D2 인터페이스 스타일 = (C) 하이브리드**: 타입드 accessor + 소수 verb. **신규 `IXxxAccessible` 없음**(YAGNI). 기존 도메인 인터페이스(`ILivable`/`IDieable`/`IDamageable`/`IMovable`/`IAttackable`/`IImpulsable`) 재사용.
- **범위**: DataSystem(`Extras`/`Referer`/`Affector`/`Modifier`) **미포팅**, modifier/UI 바인딩 **미확장**. `Stat` 만 보유.
- **D4 공유**: `BaseEntity` = Player·Enemy 공통 accessor 베이스. **`EnemyEntity` 동시 진행**.
- **D5 Physics Component화**: 인라인 외부 body 생성([PlayerActor.cpp:27-47](../../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L27-L47), [EnemyFactory.h:32-50](../../../apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h#L32-L50))을 `BoxBody`/`CircleBody` **ctor** 안으로 이관(eager). `BaseEntity` 가 `Physics` 캐시(= C# `entityRigidbody`/`entityCollider`).
- **D6 Physics/Impulse/Movement 분리 유지**(병합 안 함): body는 *공통 기질*, Movement/Impulse는 *선택적 구동기*(5 바디 중 Impulse 보유=Player·Enemy뿐, Wall/Pickup/Bullet은 없음). 병합 시 정적 바디에 dash 기계 + 매프레임 timer tick = god-body(C# Sophia도 Rigidbody/Movement/Dash 분리). facade가 *접근*만 통합.
- **D7 적 넉백 활성화 (ii)**: 적 `Impulse` 부착 + `Impulse` 시간로직 `SJH::Timer` 모듈화 + 이동자(`SimplePursueAI`/`PlayerController`)가 `IsImpulseActive()` 게이트로 자유이동 skip(C# `MoveTick` 정본). 플레이어는 *대시*(Impulse)용 게이트만 — **dormant**(dash 입력 미배선), knockback 없음. (§8.5)

## 4. 아키텍처 (레이어)

```
       (외부: 적 AI / HUD / Carrier / 디버그)
                     │ GetComponent<PlayerEntity|EnemyEntity>() 1회
                     ▼
 PlayerEntity (얇음)            EnemyEntity (얇음)
   : BaseEntity, IMovable         : BaseEntity
   캐시 IMovable*/Weapon*         (추가 캐시 없음 — 베이스로 충분)
   GetMovement()/GetWeapon()
   + Dash()/Attack()
                     │ is-a
                     ▼
 BaseEntity (공유·큼) : Component, ILivable, IDieable, IDamageable, IImpulsable
   캐시 Life* / Physics* / PlayableDirector* / Impulse*(nullable)
   IsAlive/GetHp/GetMaxHp/DoDamaged/DoDie (→Life)
   DoImpulse (→Impulse, 넉백)   GetPhysics() (→body 질의)
   GetDirector()/Play(key) (→연출+Audio)
                     │ 캐시 (OnEnter, 비소유)
   ┌──────────┬───────────┬─────────────┬──────────────┐
 Life      Physics      PlayableDir.   Impulse      (Weapon/Movement: Player만)
(체력/사망) (b2Body)     (연출+FMOD+PostFX) (넉백/Dash)
```

핵심: facade 는 로직을 갖지 않는다(그건 형제 컴포넌트). Cocos `cc.Component` / Unity `[SerializeField]+GetComponent` 캐싱 정통.

## 5. Part A — Physics body 생성 Component화 (D5)

### 5.1 불변식 (반드시 보존)
- **body 는 eager 생성**: `SimplePursueAI` 가 ctor 에서 `b2Body*` 직접 수령([EnemyFactory.h:53](../../../apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h#L53)), `PhysicsMovement`/`Impulse` 가 `OnEnter` 에서 `FindPhysics` → body 접근. ⇒ body 는 `OnEnter` 가 아니라 **ctor 시점**에 존재해야 한다.
- **owner userdata 등록은 OnEnter**: `b2Body::GetUserData().pointer = GetOwner()` 는 `GetOwner()` 필요 → ctor 에선 null. ⇒ ctor 는 body+fixture 만 만들고, **owner 등록은 `Physics::OnEnter` 로 이동**(contact 콜백은 런타임=전 OnEnter 후에만 userdata 를 읽으므로 안전).

### 5.2 설계
```cpp
// apps/_MyApp_/src/Physics/PhysicsComponent.h
namespace TopdownShooter::Physics::Components {

  struct BodyConfig {                         // b2BodyDef + fixture 공통부 (shape 는 subtype)
      b2World*    world         = nullptr;
      vmath::vec2 startPosition = vmath::vec2(0.0f, 0.0f);
      float       linearDamping = 0.0f;
      float       density       = 1.0f;
      float       friction      = 0.3f;
      bool        isSensor      = false;
      uint16_t    categoryBits  = 0;
      uint16_t    maskBits      = 0;
      float       heightOffset  = 0.0f;
  };

  class Physics : public SJH::Scene::Component, public IContactable {
      b2Body* mBody = nullptr;  float mHeightOffset = 0.0f;  bool mIsSensor = false;
    protected:
      // b2BodyDef → CreateBody (owner 무관). subtype ctor 가 호출 후 fixture 추가.
      b2Body* MakeBody(const BodyConfig& c);
      void    InitBody(b2Body* b, const BodyConfig& c);   // mBody/mIsSensor/mHeightOffset 세팅 (owner 無)
    public:
      Physics() = default;
      Physics& SetBody(b2Body*);        // 하위호환 유지(외부 주입 경로) — 리팩토링 후 호출처 0
      Physics& SetHeightOffset(float);  Physics& SetSensor(bool);
      b2Body* GetBody() const;  float GetHeightOffset() const;  bool IsSensor() const;
      void OnEnter() override;          // ⬅ owner userdata 등록 + fixture sensor 전파 (concrete 化)
      // OnExit/Update 은 여전히 pure — subtype 이 빈 구현 제공 (Physics 는 abstract 유지)
      void OnExit()  override = 0;
      void Update(float) override = 0;
  };
}

// apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h
class BoxBody : public Physics {
    vmath::vec2 mHalfSize;
  public:
    BoxBody(const BodyConfig& c, vmath::vec2 size);   // ctor: InitBody(MakeBody(c),c) + b2PolygonShape::SetAsBox fixture
    void OnExit() override {}  void Update(float) override {}
};
class CircleBody : public Physics {
    float mRadius;
  public:
    CircleBody(const BodyConfig& c, float radius);    // ctor: InitBody + b2CircleShape fixture
    void OnExit() override {}  void Update(float) override {}
};
```

### 5.3 호출부 변환
```cpp
// PlayerActor.cpp (physics 분기) — 인라인 20줄 삭제
Physics::Components::BodyConfig bc;
bc.world=cfg.physics.world; bc.startPosition=cfg.physics.startPosition;
bc.linearDamping=cfg.physics.linearDamping; bc.density=cfg.physics.density;
bc.friction=cfg.physics.friction; bc.isSensor=cfg.physics.isSensor;
bc.categoryBits=cfg.physics.categoryBits; bc.maskBits=cfg.physics.maskBits;
auto* pb = actor->AddComponent<Physics::Components::BoxBody>(bc, cfg.physics.size);

// EnemyFactory.h — 인라인 16줄 삭제 (하드코딩 값 정확 보존)
Physics::Components::BodyConfig bc;
bc.world=cfg.world; bc.startPosition=cfg.pos; bc.linearDamping=0.5f;
bc.density=1.0f; bc.friction=0.3f;
bc.categoryBits=Physics::ToBits(Physics::PhysicsLayer::Enemy);
bc.maskBits=Physics::ToBits(Physics::EnemyMask);
auto* pb = actor->AddComponent<Physics::Components::CircleBody>(bc, 0.4f);
// SimplePursueAI 는 pb->GetBody() 로 body 수령 (eager — 보존)
actor->AddComponent<SimplePursueAI>(cfg.playerTarget, pb->GetBody(), cfg.speed);
```

## 6. Part B — `BaseEntity` (공유)

```cpp
// apps/_MyApp_/src/Entity/BaseEntity.h  (현 빈 .cpp 를 처음으로 구현)
class BaseEntity : public SJH::Scene::Component,
                   public ILivable, public IDieable, public IDamageable, public IImpulsable {
  protected:
    Components::Life*                      mLife     = nullptr;
    Physics::Components::Physics*          mPhysics  = nullptr;   // = entityRigidbody/Collider
    TopdownShooter::Playable::PlayableDirector* mDirector = nullptr;
    Physics::Impulse*                      mImpulse  = nullptr;   // Player+Enemy 부착(§8.5). null-guard=비엔티티 바디(벽 등) 대비
  public:
    void OnEnter() override;   // mLife=GetComponent<Life>; mPhysics=FindPhysics(owner);
                               // mDirector=GetComponent<PlayableDirector>; mImpulse=GetComponent<Impulse>
    void OnExit()  override;   // 포인터 nullptr
    void Update(float) override {}   // 로직 없음
    // ── Life 위임 (ILivable/IDieable/IDamageable) ──
    bool IsAlive()  const override { return mLife && mLife->IsAlive(); }
    int  GetHp()    const override { return mLife ? mLife->GetHp()    : 0; }
    int  GetMaxHp() const override { return mLife ? mLife->GetMaxHp() : 0; }
    void DoDamaged(int d) override { if (mLife) mLife->DoDamaged(d); }   // i-frame 은 Life 내부
    void DoDie()          override { if (mLife) mLife->DoDie(); }
    // ── Impulse 위임 (IImpulsable) — 넉백/대시 ──
    void DoImpulse(vmath::vec2 dir) override { if (mImpulse) mImpulse->DoImpulse(dir); }
    bool IsImpulseActive() const { return mImpulse && mImpulse->IsActive(); } // 버스트 활성 창 = 이동 suppress 게이트 (§8.5)
    // ── accessor ──
    Physics::Components::Physics*               GetPhysics()  const { return mPhysics; }
    TopdownShooter::Playable::PlayableDirector* GetDirector() const { return mDirector; } // 포인터 반환 = 전방선언 OK
    void Play(const std::string& key);   // ⬅ .cpp 정의 (mDirector->Play 호출 = 완전형 필요, 헤더 인라인 불가)
};
```

> **헤더 경량화(순환 회피)**: `BaseEntity.h` 는 `PlayableDirector`/`Physics`/`Impulse`/`Life` 를 **전방선언**(멤버는 포인터). 완전형이 필요한 `OnEnter`(GetComponent/FindPhysics 는 `typeid`/`dynamic_cast` 로 완전형 요구)와 `Play()` 는 **`BaseEntity.cpp`** 에서만 해당 헤더 include. → `MyApp::Entity` 헤더가 `Playable` 헤더를 안 끌어옴(§10 CMake 참조).

**중복 IDamageable 처리(의도)**: `Life` 도 `IDamageable` 계속 구현(i-frame/sink/die 보존 — PlayerBehavior 분해 Task2 잠긴 결정). 한 액터에 IDamageable 2개(Life + facade)지만 **둘 다 Life 로 흘러 무해**(facade 는 pure pass-through, 더블 데미지 없음). `Carrier` 는 `GetComponent<IDamageable>` 로 어느 쪽을 찾아도 동일 결과 → **Carrier 불변**(커밋된 `f45211c` churn 0).

## 7. Part C — `PlayerEntity` (얇음)

```cpp
// apps/_MyApp_/src/Entity/Player/PlayerEntity.h
class PlayerEntity : public BaseEntity, public IMovable {
  protected:
    IMovable*           mMovement = nullptr;  // PhysicsMovement (인터페이스 캐시 — 물리/비물리 양분기)
    Components::Weapon* mWeapon   = nullptr;
  public:
    void OnEnter() override;   // BaseEntity::OnEnter() 호출 후 mMovement/mWeapon 캐시
    // accessor
    IMovable*           GetMovement() const { return mMovement; }
    Components::Weapon* GetWeapon()   const { return mWeapon; }
    // verb
    void DoForward(vmath::vec2 dir, float dt) override { if (mMovement) mMovement->DoForward(dir, dt); } // IMovable
    void Dash(vmath::vec2 dir)   { DoImpulse(dir); }                       // BaseEntity 기반 (player 의미)
    void Attack(vmath::vec2 aim) { if (mWeapon) mWeapon->UseWeapon(aim); } // ranged bullet
};
```

**변경점 vs 기존 [PlayerEntity.h](../../../apps/_MyApp_/src/Entity/Player/PlayerEntity.h)**:
- `mMovementComponentPtr`(구체 `Movement*`) → **`IMovable* mMovement`** (물리 분기 `PhysicsMovement` 커버).
- `mWaeponComponentPtr`/`mPlayerControllerPtr` 제거(Controller 는 facade 비노출 — 사용자 결정), `mWeapon` 만.
- `mKeyboard`/`mMouse` 멤버 제거(facade 책임 아님).
- **`IAttackable` 제거**(기존 선언에서 드롭): ① 플레이어는 ranged(`Weapon.UseWeapon`) 라 melee `DoAttack(IDamageable&)` 부적합, ② `GetNormalAtk` 이 읽을 `Weapon::Damage` 는 **private 게터 없음** → 컴파일 불가. 발사는 `Attack(aimDir)` verb 로 충분(YAGNI — 데미지 getter 안 만듦).
- **부착**: `CreatePlayerActor` 끝(모든 형제 부착 후) `actor->AddComponent<PlayerEntity>()`.

**IMovable 중복(무해)**: PlayerEntity + PhysicsMovement 둘 다 IMovable. `GetComponent<IMovable>` 소비처 0(controller 는 `SetMovableTarget(pm)` 명시 포인터) → 모호조회 없음.

## 8. Part D — `EnemyEntity` (신규)

```cpp
// apps/_MyApp_/src/Entity/Enemy/EnemyEntity.h
class EnemyEntity : public BaseEntity {
    // 추가 캐시 없음 — Life/Physics/Director/Impulse(=BaseEntity)로 충분.
    // (적 전용 능력 생기면 여기 확장: AI 핸들 등)
};
```
- **부착**: `CreateEnemyActor` 끝 `actor->AddComponent<EnemyEntity>()`.
- **정리**: `EnemyDeathHandler`/`SimplePursueAI` 의 raw `GetComponent<Components::Life>` 를 facade(`IsAlive()`) 경유로(§8.5). ⚠ `SimplePursueAI` ctor body 주입은 **유지**(eager 필요).

### 8.5 Part D' — Impulse 활성화 (ii): Timer 모듈화 + 이동 suppress (적 넉백 + 플레이어 대시)

**공통 패턴 (사용자 C# 레퍼런스 = 정본)**: *이동 로직이 dash/impulse 상태를 조회해 early-return* — 버스트가 덮어쓰이지 않게 자유이동을 일시 Block.
- **C# 정본**: `Player.MoveTick()` → `if (DashSkillAbility.GetIsDashState()) return;` (대시 중 이동 스킵). 상태/시간 = `DashSkill` + `CoolTimeComposite`(Timer).
- **C++ 매핑**: 이동자(`PlayerController`/`SimplePursueAI`)가 `entity->IsImpulseActive()` 게이트로 **skip(`return`)**. 넉백/대시 창 = `Impulse` + `SJH::Timer`. **단일 source-of-truth**(별도 hold 타이머 없음 → drift 0).
- **정정(사용자 2026-06-03)**: 플레이어는 *knockback* 은 없지만 **Dash 가 Impulse 사용** → 플레이어도 동일 게이트(아래 D). 즉 *플레이어 대시 = C# 정본 1:1*, *적 넉백 = 일반화*.

**A. 적 Impulse 부착 (Gap1)**: `CreateEnemyActor` → `actor->AddComponent<Physics::Impulse>()`. `BaseEntity::DoImpulse`/`IsImpulseActive` 가 적에서 실효. 넉백 출처 = `Projectile`(총알, 실제 방향). `ContactCarrier`(적↔플레이어)는 dir=0 라 플레이어 무영향.

**B. Impulse 시간 로직 → `SJH::Timer`** (사용자: "시간 의존 로직은 Timer 모듈", C# `CoolTimeComposite` 패러티, Life 마이그레이션 일관):
- `mActiveTimer`/`mDurationSec`(0.3s) → `SJH::Timer activeTimer_{0.3f}`; `mCooldownTimer`/`mCooldown`(0.8s) → `SJH::Timer cooldownTimer_{0.8f}`. 힘값 `mImpulseForce`(7.5 Stat) 유지.
- **arm-inactive**(Life 정통): 생성 직후 `Tick(base)` → finished(비활성). `DoImpulse` → `Reset()`(발동). `Update` → `Tick(dt)`. `IsActive()`=`!activeTimer_.IsTimesUp()`; 쿨다운 게이트=`!cooldownTimer_.IsTimesUp()`.
- **동작 보존**(0.3s/0.8s 동일, float→Timer 메커니즘만). 적·플레이어 Impulse 공통.

**C. 적 suppress (Gap2)** — `SimplePursueAI`:
- `OnEnter` 에서 `EnemyEntity*`(BaseEntity*) 캐시.
- `Update`: ① 사망 `if (!entity->IsAlive()) { body 0; return; }` ← 기존 매 프레임 raw `GetComponent<Life>` 를 **facade 로 교체**(refactor 시연). ② **넉백 `if (entity->IsImpulseActive()) return;`** — 추적 속도 설정 **스킵**(0 설정 아님 — 버스트 보존). 0.3s 후 자동 재개.
- ctor body 주입 유지.

**D. 플레이어 suppress (대시)** — `PlayerController`:
- 입력→`DoForward` 경로 앞에 **`if (entity->IsImpulseActive()) return;`**(대시 버스트 보존). ⚠ *입력 없어도* `PhysicsMovement::DoForward(0)` 가 속도를 0 으로 만들어 버스트를 죽이므로 **DoForward 자체를 skip**(controller 가 active 동안 호출 안 함). entity 핸들은 controller `OnEnter`/lazy 캐시.
- **현재 dormant**: dash *입력 바인딩*(key→`DoImpulse`) 미배선 → 플레이어 Impulse 발화 0 → 게이트 발화 0 → **플레이어 행동 변화 0**. 게이트는 *대시 입력 도입 시 즉시 동작*하도록 미리 박음(C# `MoveTick` 정본 1:1).
- **범위 밖(후속)**: dash 입력 액션 바인딩(`Action::Dash`→`DoImpulse(dir)`), 플레이어 *피격 knockback*(현재 없음).

## 9. 소비처 영향 요약

| 소비처 | 영향 |
|---|---|
| `Carrier`(Projectile/ContactCarrier, `f45211c`) | **불변** — `GetComponent<IDamageable>/<IImpulsable>` 그대로(facade 가 추가 만족) |
| `PlayerController` | 입력 `DoForward` 를 `IsImpulseActive` 동안 **skip**(대시 버스트 보존, §8.5 D) — **dormant**(dash 입력 미배선 → 행동 변화 0). 그 외 형제 캐시 유지 |
| `SimplePursueAI` | ctor body 주입 유지. Life poll→facade `IsAlive()` + **넉백 중 추적 suppress(`IsImpulseActive()`)** (§8.5 C) |
| `EnemyDeathHandler` | Life poll → `EnemyEntity::IsAlive()` facade 경유 |
| HUD/HealthBar | **미접근**(병렬 트랙) |

## 10. 파일 변경 & CMake

| 파일 | 변경 |
|---|---|
| `apps/_MyApp_/src/Physics/PhysicsComponent.h` | `BodyConfig` 추가 / `MakeBody`·`InitBody` protected / `OnEnter` concrete(owner 등록) / `OnExit`·`Update` pure 유지 |
| `apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h` | `BoxBody(BodyConfig,size)` / `CircleBody(BodyConfig,radius)` ctor 생성. **기존 빈 `OnEnter() override {}` 제거**(베이스 concrete OnEnter 상속) — `OnExit`/`Update` 만 빈 override 유지 |
| `apps/_MyApp_/src/Entity/BaseEntity.h` | `IImpulsable` 추가 + Physics/Director/Impulse 멤버·accessor |
| `apps/_MyApp_/src/Entity/BaseEntity.cpp` | **빈 파일 첫 구현**(OnEnter 4캐시) |
| `apps/_MyApp_/src/Entity/Player/PlayerEntity.h` | `IMovable*`/Weapon 캐시 + verb (위 §7) |
| `apps/_MyApp_/src/Entity/Player/PlayerEntity.cpp` | **신규**(OnEnter 캐시) |
| `apps/_MyApp_/src/Entity/Enemy/EnemyEntity.h` | **신규** |
| `<Entity>/Enemy/EnemyEntity.cpp` | **신규**(필요 시 — 헤더 온리 가능) |
| `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` | 인라인 body 삭제 → `AddComponent<BoxBody>(bc,size)` + `AddComponent<PlayerEntity>()` |
| `apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h` | 인라인 body 삭제 → `AddComponent<CircleBody>(bc,radius)` + `AddComponent<EnemyEntity>()` |
| `Entity/CMakeLists.txt` | `PlayerEntity.cpp`(+`EnemyEntity.cpp`) source 추가. `SJH::playable` 링크 복구(BaseEntity 가 `PlayableDirector.h` 노출) + `MyApp::Playable` / Physics include 정합 |

> ⚠ **CMake 순환 주의**: `BaseEntity.cpp` 가 `apps/_MyApp_/src/Playable/PlayableDirector.h` 의 **컴파일 심볼**(`Play`/`Register`/`Update` from `PlayableDirector.cpp`) 사용 → `MyApp::Entity` 가 `MyApp::Playable` 에 **링크 의존**. 한편 `MyApp::Playable`([Playable/CMakeLists.txt](../../../apps/_MyApp_/src/Playable/CMakeLists.txt))는 `MyApp::Entity` 에 PUBLIC 링크(`IActorPresentation`). ⇒ **Entity↔Playable CMake 양방향 = 순환 위험**.
> - **핵심 관찰**: `IActorPresentation`([Components.Interfaces.h](../../../apps/_MyApp_/src/Entity/Components/Components.Interfaces.h))는 **순수 인터페이스(헤더 온리, 컴파일 심볼 0)**. 즉 Playable→Entity 는 *심볼* 이 아니라 *include 경로* 만 필요.
> - **회피책**: ① `BaseEntity.h` 는 `PlayableDirector` **전방선언**(포인터 멤버), 완전형은 `BaseEntity.cpp` 에서만 include → Entity *헤더* 가 Playable 헤더를 안 끌어옴. ② Playable 의 `MyApp::Entity` 링크를 **include-only 로 강등**(`target_include_directories` 또는 헤더 온리 interface 타겟) 고려 → CMake 순환 끊김. 단방향 링크 `myapp_entity → myapp_playable` 만 남김. (plan 에서 링크 그래프 확정·빌드 GREEN 검증.)

## 11. 회귀 가드레일 & 검증

- **Physics 파라미터 완전 보존**: b2BodyDef(type/position/linearDamping) + fixture(density/friction/isSensor/filter bits) + shape(box halfSize = size×0.5 / circle radius=0.4) **동일**. Enemy 하드코딩 값(damping 0.5/density 1/friction 0.3/radius 0.4/Enemy filter) 보존.
- **owner userdata 타이밍**: `SetBody-time` → `Physics::OnEnter`. contact 콜백은 런타임에만 읽음 → 안전. (단 OnEnter 누락 시 userdata null → contact dispatch 실패. OnEnter 구현 필수.)
- **eager body 불변식**: ctor 생성으로 `SimplePursueAI`/`FindPhysics` 보존.
- **커밋 = 사용자 승인 · path-scoped** (`git add -A`/`.` 금지 — HealthBar/cull/Example 병렬 휩쓸지 말 것). `Co-Authored-By` 미사용.
- **미접근**: `main.cpp`, HUD(`HealthBarFactory.h` 등 사용자 IDE 오픈), `src/material/pass.h`(cull), Timer 코어, 셰이더, `Carrier`(불변), `EnemyBuilder.cpp`(HealthBar 트랙 — `EnemyFactory.h` 만 surgical).
- **검증 = 빌드 + GUI**(no_auto_tests):
  - `cmake --build --preset ninja --target _MyApp_` → exit 0
  - GUI: ① 좌클릭 발사→적 명중 HP감소·총알소멸 / 벽 명중 소멸 ② 적 접촉→플레이어 HP감소(i-frame) ③ 적 사살→dissolve ④ WASD+4방향+발사 정상 ⑤ 크래시 0 (= body Component화 회귀 0 증명) ⑥ **적 총알 명중 시 잠깐 뒤로 밀린 뒤 추적 재개**(넉백 — Impulse+Timer+AI suppress) ⑦ WASD 이동 평소와 동일(플레이어 게이트 dormant — 무변화여야 정상)
- **구현 전 `git log/status` 재측정**(병렬 트랙 churn — HEAD 가 수시 이동).

## 12. 유보 / 후속 (이번 범위 밖)

- modifier(`Affector`)/UI 바인딩/`Extras`/`Referer` DataSystem 포팅.
- **dash 입력 바인딩**(`Action::Dash` → `DoImpulse(dir)`) + 플레이어 *피격 knockback* — 플레이어 게이트(§8.5 D)는 이미 박혀 dash 입력 도입 즉시 동작. (현재 플레이어 knockback 없음 — 사용자 명시.)
- `PlayerController` 가 facade(`PlayerEntity`) 경유로 Weapon/Movement 접근하도록 재배선(현재 자기 형제 직접 캐시 유지).
- 조준(aim) accessor — 필요 시 `PlayerEntity::GetAim*()` 추가(현재 Controller 내부 유지).
- `Physics::SetBody` 외부주입 API 제거(리팩토링 후 호출처 0 이 되면).

## 13. 구현 순서(플랜 입력 — 개략)

1. **Part A** Physics Component화(BodyConfig+ctor+OnEnter) → PlayerActor/EnemyFactory 호출부 전환 → 빌드 GREEN + GUI(물리 회귀 0). *가장 위험 — 먼저·독립 검증.*
2. **Part B** BaseEntity 구현(빈 .cpp 첫 구현, 4캐시).
3. **Part C** PlayerEntity 확장 + PlayerActor 부착.
4. **Part D** EnemyEntity 신설 + EnemyFactory 부착.
5. **Part D' (ii)** `Impulse` 시간로직 `SJH::Timer` 모듈화 + 적 `Impulse` 부착 + `SimplePursueAI` suppress(facade `IsAlive`/`IsImpulseActive`) + `PlayerController` 게이트(dormant) + `EnemyDeathHandler` Life poll facade 정리. 빌드 GREEN + GUI(적 넉백 보임, 플레이어 무변화).
6. CMake 정합(링크 그래프/순환 회피) — 각 단계 빌드 GREEN.

(각 단계 = 서브에이전트 구현 no-commit → 오케스트레이터 diff 검증 → 사용자 승인 커밋. Task별 빌드+GUI 게이트.)
