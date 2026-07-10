# Handoff — PlayerBehavior 분해 (god-component → Entity/Components 분산)

> **수신자**: 작업 재개 시점의 나 또는 다른 Claude Code.
> **목적**: 죽은(unattached) god-component `PlayerBehavior` 를 기존 `Entity/Components` taxonomy(컴포넌트별 Stat 데이터)로 **분해**한다. 이는 선행 *가중치평가*(아래 §1)의 **"PlayerBehavior 은퇴"** 결론을 구체적 설계로 실행한 것.
> **작성 시점**: 2026-06-01. 브랜치 `game/module/ingame/temp`.
> **진행 단계**: **설계 완료 + clean-ddd-hex 검증 완료. 코드 0줄. 사용자 결정 5건(§7) 미확정 → 확정 후 구현/plan.**
> **산출 경로**: `doc/` 는 gitignore(로컬). 본 핸드오프는 `doc/handoff/`(git 추적).

---

## 1. 선행 컨텍스트 — 가중치평가 결론 (A1/A2) — 본 분해의 전제

직전에 *clean-ddd-hex 가중치평가 Workflow*(11 에이전트)로 "내 M6 plan vs 2026-06-01 핸드오프 plan(4방향 스프라이트/raycast 조준)" 을 비교한 결과:

- **현 상태 (검증)**: 플레이어 = 4-레이어 directional 스프라이트(FRONT_MOVE 1방향만 배선) + `PlayerController::UpdateAim`(raycast 조준) + `Weapon::UseWeapon`(발사) + `PlayerHands` 전부 커밋·live. **`PlayerBehavior` 는 어디에도 `AddComponent` 안 됨 = dead code.** M6 FX 프리미티브(Spawns/) 완료·live. **빌드 green** (wall_factory.h 깨짐 = 거짓이었음).
- **A1 (만장일치, 67.0/75)**: 방향 스프라이트 = **`PlayerSpriteDirector`**(4-레이어 그룹 + `SpriteRenderer.Visible` 토글 + BoxBody 속도 양자화). PB의 단일아틀라스 `PlayClip(EPlayerClip)` 모델은 **구조적 obsolete**.
- **A2 (다수 2/1, 65.0/75)**: 공격/피격 FX = **기존 committed delegate seam**(`onFire` / `BulletContactHandler::SetOnHitFx` / enemy `onDeathFx`)에 Spawns 주입. PB FX 슬롯 **부활 금지**.
- **결론**: PB 은퇴 + PlayerSpriteDirector(방향) + delegate-seam FX. → **본 분해 = 그 은퇴의 실행**.

---

## 2. 큰 통찰 — 이동(MIGRATE)이 아니라 대부분 삭제

PB는 5책임(locomotion/dash/survivability/attack/sprite)을 뭉친 god-component이고, **3개 desync 진실**(mNormalSpeed↔PhysicsMovement / mDead↔IsAlive / mAttackDir↔controller aim)을 품음. 분해하면 **진짜 새로 이동하는 조각은 소수**, 나머지는 죽었거나 이미 다른 곳에 존재 → **순삭제**.

| 진짜 MIGRATE | 목적지 |
|---|---|
| Dash 메커니즘 | **신규 `Impulse`** (Physics/) |
| i-frame + Die | **`Life`** (확장) |
| sprite 토글 + mMoveEffect | **신규 `PlayerSpriteDirector`** (Entity/Player/) |

> **행동 회귀 위험 0** — PB가 verifiably dead라 삭제해도 동작 변화 없음. *유일한 동작 변화는 양성*: §3의 접촉 데미지가 비로소 작동.

---

## 3. ⚙️ Load-bearing 발견 (지금 존재하는 버그)

`EnemyContactHandler::OnCollisionEnter`([EnemyContactHandler.cpp:13-14](../../apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp#L13))가 **죽은 `PlayerBehavior::Hit` 를 `GetComponent` 로 호출 → nullptr → 적 접촉 데미지가 완전 no-op**. → `Components::Life::DoDamaged` 로 repoint 하면 (i-frame 게이트가 Life로 들어가) **모든 데미지원(총알+접촉) 보호 + 접촉 데미지 작동**. (결정 D5)

---

## 4. 전체 분해 테이블 (PB 모든 필드/메서드)

| PB 조각 | 목적지 | Stat/기구 | 액션 | 비고 |
|---|---|---|---|---|
| `mSpriteSeq` | PlayerSpriteDirector | per-(EFacing,EPose) DirGroup{4 SpriteRenderer* + B-part seq} | RECONCILE | 단일 seq 폐기, 그룹 소유 |
| `mCurrentClip`+`EPlayerClip` | PlayerSpriteDirector | 2축 분리: `EFacing{F,B,L,R}`(Visible) × `EPose{Idle,Move}`(seq) | RECONCILE | Attack/Hit는 상태에서 탈락(자산 없음) |
| `PlayClipInternal` | PlayerSpriteDirector | private `Apply()`: Visible 토글 + 활성 그룹 B-part Play | MIGRATE | A1 기구 |
| 속도기반 Idle↔Move 자동스왑 | PlayerSpriteDirector::Update | BoxBody 속도 → EPose/EFacing 자가구동 | MIGRATE | "작성자 1명" 결정 필요 |
| "Hit/Attack clip 끝→Idle" poll | (없음) | — | **DROP** | 끝낼 clip 없음 |
| `mMoveEffect` | PlayerSpriteDirector | `SetMoveEffect(IPlayable*)`; Move 진입 Play/이탈 Stop | MIGRATE | **유일하게 sprite에 동거하는 FX 슬롯** |
| `mAttackPlayable` | (없음) — `onFire` Composite seam | PlayerBuilder.cpp:47-73 inline | **DROP** | A2 |
| `mHitPlayable` | (없음) — `SetOnHitFx` seam | BulletContactHandler.h | **DROP** | A2, 플레이어 피격 FX 기본 없음(YAGNI) |
| `mDashPlayable` | (없음) — 미래 dash 바인딩 site | Composite 주입 | **DROP** | A2, dash 바인딩 아직 없음 |
| `mDiePlayable` | **Life::SetOnDeathFx** delegate | `std::function<void(const vec3&)>` | RECONCILE | EnemyDeathHandler::DeathFx와 동일 패턴 |
| `mAttackDir`+`GetAttackDirection` | (없음) — PlayerController::mAimDirection | per-shot `box2dForward=(aim.x,-aim.z)` | **DROP** | 유일 소비자 BulletSpawnPlayable도 dead |
| `mNormalSpeed`(3.0) | (없음) — PhysicsMovement.mMoveSpeed | Stat(MoveSpeed) 이미 존재 | **DROP** | 죽은 세번째 사본 |
| `Move(vel)` body | (없음) — PhysicsMovement::DoForward | PhysicsMovement.h:43-54 | **DROP** | 두번째 이동기구 금지 |
| `Idle()` | (없음)/Director::SetPose(Idle) | 속도 양자화 | **DROP** | flat verb 은퇴 |
| `Attack(dir)` | (없음) — Weapon::UseWeapon+onFire | — | **DROP** | 순삭제, Weapon 변경 ~0 |
| `Hit(damage)` | **Life::DoDamaged** (게이트 folded) | i-frame early-return + mCurHp-=dmg + i-frame arm | RECONCILE | **load-bearing** (§3) |
| `Dash(dir)` | **신규 Impulse::DoImpulse(dir)** | cd/active 게이트 → SetLinearVelocity(n*force) XZ→XY → 타이머 arm | MIGRATE | Player Dash + Monster Knockback |
| `mDashSpeed`(7.5) | Impulse.mImpulseForce | **Stat(DashForce)** ✓존재 | MIGRATE | |
| `mDashDuration`(0.3) | Impulse.mDurationSec | plain float(맞는 enum 없음) | MIGRATE | |
| `mDashCooldown`(0.8) | Impulse.mCooldown | **Stat(CoolDownSpeed)** ✓존재 | MIGRATE | 이름=rate인데 sec 저장(semantic flag) |
| `mDashTimer` | Impulse.mActiveTimer | plain 런타임 | MIGRATE | |
| `mDashCooldownTimer` | Impulse.mCooldownTimer | plain 런타임 | MIGRATE | |
| `IsDashing()` | Impulse.IsActive() | bool query | MIGRATE | PhysicsMovement가 양보해야(속도싸움) |
| `mHitInvincibility`(0.5) | **Life.mIFrameSeconds** | plain float(후일 Stat(Tenacity) 선택) | MIGRATE | **RECONCILE: 라인근접상 dash 옆이나 Life로** |
| `mInvincibilityTimer` | Life.mInvincibleTimer | plain 런타임 | MIGRATE | |
| `mDead`(bool) | (없음) → mDeathFxFired latch | IsAlive()(mCurHp<=0) 파생 | **DROP** | 두 진실 desync 제거 |
| `Die()` | **Life::DoDie()** (현재 empty) | one-shot guard → onDeathFx(pos) → SetActive(false) | MIGRATE | clip 탈락(A1), FX=seam(A2) |
| `IsAlive()` | Life::IsAlive() 이미 존재 | 0<mCurHp | **DROP** | PB는 forwarder였음 |
| `Update(dt)` god | **3분할**: Impulse(dash 타이머)+Life(i-frame+death poll)+Director(속도→pose) | 각자 자기 런타임만 tick | RECONCILE | |
| `Init`/`SetBody`/`SetSceneRoot` | (없음) | per-component ctor + CreatePlayerActor | **DROP** | Pattern-C config로 대체 |
| `mBody`/`mSceneRoot` | (없음) | `FindPhysics(GetOwner())`/`SetVelocitySource`/`Director::Root()` | **DROP** | god 중앙화 제거 |

---

## 5. 신규 컴포넌트 3종 (설계)

### 5.1 `Impulse` — `apps/_MyApp_/src/Physics/physics_impulse.h` (+선택 .cpp)
- `namespace TopdownShooter::Physics`, `class Impulse : SJH::Scene::Component, Entity::IImpulsable`.
- **범용 물리 속도버스트**: Player Dash + Monster Knockback. PhysicsMovement 옆 거주(b2Body 동일 취급). body 는 `Components::FindPhysics(GetOwner())` 로 OnEnter 에서 해소(비소유).
- Stat: `mImpulseForce = Stat(DashForce, Natural)`, `mCooldown = Stat(CoolDownSpeed, Natural)`. plain: `mDurationSec`/`mActiveTimer`/`mCooldownTimer`. 기본 7.5/0.3/0.8 (PB 미러).
- API: `DoImpulse(vec2 dir)`(cd/active 게이트→SetLinearVelocity XZ→XY→타이머 arm), `ApplyKnockback(vec2 fromXZ)`(convenience), `IsActive()`, `Update(dt)`(타이머 감소).

### 5.2 `IImpulsable` — 신규 인터페이스 (`Entity/Components/Components.Interfaces.h`)
- IMovable 스타일(protected ctor, copy/move delete, pure virtual). 단일: `virtual void DoImpulse(vmath::vec2 dir) = 0;`
- **IMovable 오버로드 금지** — `PlayerController::SetMovableTarget(IMovable*)` 계약 깨짐. DoImpulse(일회 타임드) = DoForward(지속)의 대칭 파트너.

### 5.3 `PlayerSpriteDirector` — `apps/_MyApp_/src/Entity/Player/PlayerSpriteDirector.{h,cpp}`
- `namespace TopdownShooter::Entity::Player`, plain `Component`(Entity 인터페이스 없음). **presentation-only sink** (PB #5 + A1 4-레이어 토글).
- 소유: per-(EFacing,EPose) `DirGroup{ array<SpriteRenderer*,4> layers; SpriteSequencePlayable* bPart; }`.
- API: `RegisterGroup`/`SetVelocitySource(BoxBody*)`/`SetFacing`/`SetPose`/`FaceAim`/`SetMoveEffect`/`PlayOneShot` + `CurrentFacing`/`CurrentPose` readers.
- `Update(dt)`: BoxBody 속도로 facing+pose 자가구동. `Apply()`: 그룹 4레이어 `SpriteRenderer.Visible` 토글.
- **physics/Stat/movement/input 없음 — leaf**. siblings가 push, 자신은 never call back. `PlayOneShot` = 얇은 generic 주입-IPlayable 훅(EPlayerClip enum 없음, 자산 의존 없음).

---

## 6. 참조 그래프 (의존 방향 — 전부 inward, acyclic)

```
gameplay/combat 컴포넌트  ──push──▶  PlayerSpriteDirector (LEAF — never calls back)
controller  ──▶  IMovable / IImpulsable
컴포넌트  ──FindPhysics──▶  b2Body
Entity  ──std::function delegate──▶  Spawns (의존 역전 보존)
```
- **PlayerController::Update**: 오늘의 `mCachedSprite->flipX`([PlayerController.cpp:174-177]) 대체 → facing=aim이면 `dir->FaceAim(mAimDirection)`, A1-literal(velocity)면 director 자가구동. **flipX 핵 은퇴** — facing=어느 그룹이 Visible인가.
- **Weapon**: 라이브 Attack/Hit 스프라이트 없음 → 기본 참조 없음. (미래 attack pose 시만 `dir->PlayOneShot`.)
- **Life/Impulse**: director가 `IsInvincible()`/`IsActive()` 폴(Director→읽기, inward). 역방향 없음.
- **FX seam 발화처** (A2 committed, PB 슬롯 아님): `onFire`(PlayerBuilder.cpp:47-73, PlayerController.cpp:276 발화) / `BulletContactHandler::SetOnHitFx` / **`Life::SetOnDeathFx`(신규)** / (미래) dash site.
- **데미지 라우팅 (load-bearing)**: `EnemyContactHandler` → `Life::DoDamaged` repoint + PlayerBehavior.h include 제거.
- **생성**: CreatePlayerActor가 8 child-actor 그룹 빌드(현 FRONT_MOVE-only 루프 [PlayerActor.cpp:86-118] 확장) → `RegisterGroup` → `SetVelocitySource(boxBody)`. PlayerBuilder의 child-scan(result.Sprite/SpriteSeq [PlayerBuilder.cpp:115-123])은 은퇴/repoint.

---

## 7. 🙋 사용자 결정 5건 (미확정 — 확정 후 구현)

| # | 질문 | 옵션 | **권고** |
|---|---|---|---|
| **D1** speed-home | walk-speed Stat 단일 위치 + `Components::Movement` 운명 | (a) 둘 다 IMovable Strategy 유지 / (b) Movement 삭제 / (c) MovementBase 추출 | PhysicsMovement.mMoveSpeed(현행). **(b) 삭제** if 비물리 액터 무계획. `cfg.physics.world==nullptr` 분기 실사용 확인 |
| **D2** impulse-home | Impulse 위치/인터페이스 + i-frame 소속 | (a) Physics/+IImpulsable, i-frame→Life / (b) Entity/Components(Box2D 누수) / (c) dash+invuln 묶음 | **(a)** |
| **D3** director-api | anim API + EFacing 구동 | (a) EPose{Idle,Move}+얇은 PlayOneShot / (b) PlayOneShot 제거 / (c) full Attack/Hit clip(아트 필요) | **(a)**. **facing = aim vs velocity 확인**(탑다운=보통 aim, A1=velocity). **pose 작성자 1명** |
| **D4** weapon-dir | Weapon이 attackDir 저장? IAttackable? | (a) per-shot 수신, 저장X, IAttackable X / (b) mAttackDir 저장 / (c) IAttackable 채택 | **(a)**. `BulletSpawnPlayable` 삭제 |
| **D5** enemy-retarget | EnemyContactHandler→Life::DoDamaged? | (a) repoint(i-frame Life로) / (b) 유지(no-op 영구) / (c) wrapper로 총알만 i-frame bypass | **(a)** (load-bearing). Die() SetActive(false) vs death-anim 확인 |

---

## 8. clean-ddd 판정 + 2 통합 리스크

**판정: ✅ YES** — god-component 제거, 각 책임 단일 Stat-소유, behavior-rich(anti-anemic), deps inward, YAGNI(speculative IAnimatable/IAttackable 없음, dash+knockback 단일 컴포넌트).

**⚠ 분해가 자동 해결 못 하는 통합점 2 (명시적 결정 필요)**:
1. **pose 작성자 2명** — director 자가구동(속도) + sibling SetPose 동시 = 두 작성자. **한 명만**(권장: director 자가구동, controller는 facing만 push).
2. **속도 싸움** — Impulse·PhysicsMovement 둘 다 `SetLinearVelocity`. PB는 `if(IsDashing()) Move()return`으로 회피→분해 후 사라짐. controller가 `Impulse::IsActive()` 중 `DoForward` 억제(권장, 최소) OR Impulse가 `ApplyLinearImpulse`(가산이나 컨벤션 이탈).
3. (관찰) `Components::Movement` vs `PhysicsMovement` 근중복은 분해가 악화는 안 하나 지금이 정리 타이밍(D1).

---

## 9. 빌드 순서 (의존 정렬 — 결정 확정 후)

| Step | 작업 |
|---|---|
| **0** | 속도 확인(no-op) — walk speed가 PhysicsMovement.mMoveSpeed에 이미 있음 인증. (선택) `GetSpeed()` 추가. D1 확정 |
| **1** | **Life 확장** (load-bearing 먼저) — `mIFrameSeconds`+`mInvincibleTimer`+`mDeathFxFired`+`SetOnDeathFx`+`IsInvincible`; i-frame 게이트를 `DoDamaged`에 fold; `DoDie()` 구현(one-shot→onDeathFx→SetActive(false)); `Update()` i-frame 감소+death poll; `Life(hp,cur,iframe)` ctor. header-only(+<functional>,<vmath.h>) |
| **2** | **EnemyContactHandler repoint** → `Life::DoDamaged`, PlayerBehavior.h include 제거. PlayerBuilder가 `SetIFrameSeconds(0.5)`+`SetOnDeathFx(...)` 주입. **M6 R1a/R1b FX 배선이 동일 seam 패턴으로 합류** |
| **3** | **IImpulsable + Impulse** — 인터페이스 추가, `Physics/physics_impulse.h`, CreatePlayerActor(physics 분기) AddComponent. **미배선 ship**(dash 바인딩 없음 = correct-by-construction) |
| **4** | **속도싸움 통합** (dash 실제 트리거 시) — controller가 `Impulse::IsActive()` 중 DoForward skip(권장) OR ApplyLinearImpulse. `Action::Dash` 바인딩 결정 보류(오늘 없음) |
| **5** | **PlayerSpriteDirector** — `Entity/Player/`(presentation-only). CreatePlayerActor 4-레이어 루프를 FRONT_MOVE→8그룹 확장(데이터 Constants.h에 이미 있음). `RegisterGroup`+`SetVelocitySource`. `Apply()`+`Update`+`SetMoveEffect`. PlayerController flipX 핵 → FaceAim/자가구동. child-scan 은퇴. **⚠ main.cpp = Fog Agent 경합(메모리 참조)** |
| **6** | **철거** — `PlayerBehavior.{h,cpp}` + `BulletSpawnPlayable.{h,cpp}` 삭제(D4). EnemyDeathHandler.h의 PB 주석 제거. grep 0참조 확인. `_MyApp_` 빌드 link-clean |
| **7** | (선택, 승인 필수) EnemyDeathHandler를 `Life::SetOnDeathFx`로 통합 → player+enemy **단일 death 기구**. 코어 분해 범위 밖 |

---

## 10. 절대 하지 말 것
- `IMovable` 오버로드(SetMovableTarget 계약). → 신규 `IImpulsable`.
- `Components::Movement`로 walk speed 이전(세번째 죽은 사본). speed = PhysicsMovement.
- Impulse에 invincibility/FX 슬롯 부착(미니 god). i-frame=Life, FX=seam.
- PB FX 슬롯 부활(A2). FX = onFire/SetOnHitFx/onDeathFx delegate.
- 단일 SpriteRenderer flipX/PlayClip로 방향 표현(A1). 방향 = 그룹 Visible.
- pose 작성자 2명 / 속도 SetLinearVelocity 충돌 방치(§8).
- 자산 없는 Attack/Hit/Die 스프라이트 상태 추가(D3-c).
- `doc/`(gitignore)에 추적 문서 작성 — `doc/`(단수).

---

## 11. 진입점 파일 (verbatim)
- 분해 원본: [PlayerBehavior.{h,cpp}](../../apps/_MyApp_/src/Entity/Player/PlayerBehavior.h) (dead).
- 목적지: [LifeComponents.h](../../apps/_MyApp_/src/Entity/Components/LifeComponents.h), [WeaponComponents.{h,cpp}](../../apps/_MyApp_/src/Entity/Components/WeaponComponents.h), [Components.Interfaces.h](../../apps/_MyApp_/src/Entity/Components/Components.Interfaces.h), [PhysicsMovement.h](../../apps/_MyApp_/src/Physics/PhysicsMovement.h).
- Stat: [Stat.h](../../apps/_MyApp_/src/Algebraic/Stat.h) + [Algebraic.Common.h](../../apps/_MyApp_/src/Algebraic/Algebraic.Common.h) — `ENumericStatType{DashForce=12, CoolDownSpeed=41, MoveSpeed=3, MaxHp=0, Tenacity=5, ...}`.
- 스프라이트 데이터: [Constants.h](../../apps/_MyApp_/src/Playable/Constants.h) (8 방향 벡터, IDLE/MOVE만, B-part만 ColCount>1).
- 재타겟: [EnemyContactHandler.cpp:13](../../apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp#L13), [PlayerController.cpp:174](../../apps/_MyApp_/src/InputHandler/PlayerController.cpp), [PlayerActor.cpp:86](../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L86), [PlayerBuilder.cpp:115](../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L115).
- 관련 핸드오프: [2026-06-01-player-wasd-4direction-sprite-switching.md](2026-06-01-player-wasd-4direction-sprite-switching.md) (A1 PlayerSpriteDirector 원안), [2026-06-01-raycast-hand-vfx-handoff.md](2026-06-01-raycast-hand-vfx-handoff.md) (조준/발사 본구현).

---

## 13. 확정 설계 — 세션 2 (포트 해소 + OnXXX sink + Carrier + facing) [2026-06-01, 모두 잠김]

> §1(연출) + §2(Carrier)의 *기구* 확정. clean-ddd-hex Workflow(7 에이전트) + 사용자 결정. 위 §5(컴포넌트 설계)·§7(D1~D5)·§9(빌드순서)를 이 절이 **갱신/확정**한다.

### 13.1 포트 해소 기구 (정본 — §1/C1 공통)
- **α: `Actor::GetComponent<T>` 게이트 완화** — `enable_if<is_base_of_v<Component,T>>` → **`is_polymorphic_v<T>`** (engine `src/scene/actor.h`). 인터페이스(`IDamageable`/`IActorPresentation` 등)도 조회 가능(Unity `GetComponent<IInterface>` 정통). concrete=fast-path typeid→`static_cast` / 인터페이스=slow-path `dynamic_cast`(O(N), 무해).
  - **⚠ 구현 중 정정 (2026-06-01, Task 2)**: 초안의 "static_cast 오용 없음(인터페이스 typeid 맵 키 미스로 fast-path 미스)" 은 **오류**였다. fast-path 가 런타임에 안 타도 `static_cast<T*>(it->second.get())` (정적 타입 `Component*`)는 `GetComponent<Interface>()` **인스턴스화 시점에 컴파일**돼야 하는데, 인터페이스 T 는 Component 와 무관한 polymorphic 타입이라 `static_cast<Interface*>(Component*)` = ill-formed → 컴파일 에러. **fix = fast-path 를 `if constexpr (std::is_base_of_v<Component,T>)` 로 가드** (concrete 만 static_cast 유지=핫패스 RTTI 없음, 인터페이스는 블록 폐기 → slow-path dynamic_cast 로만 조회). Task 0 빌드는 인터페이스 호출이 없어 통과했고 Task 2 Life::OnEnter 의 `GetComponent<IActorPresentation>()` 에서 처음 노출됨.
- **OnEnter 1회 캐시**: 핫패스 포트(예: Life의 sink)는 `OnEnter`에서 `GetComponent<IActorPresentation>()` 1회 해소 후 멤버 캐시. **ctor 금지**(GetOwner null + dynamic type=base).
- **multi-match**(한 인터페이스 2구현)는 비결정적 first → 계약: **액터당 해당 인터페이스 1구현**. 수집 필요 시 `ForEachComponent + dynamic_cast`.
- 부수: `PhysicsComponent.h:73` "base 질의 불가" stale 주석 정정(이제 가능, 순수 비-Component만 불가였음).

### 13.2 연출 sink — `IActorPresentation` (RD1=OPT-1, RD2=1개, RD3=베이스-호출, RD6=silent)
- **`IActorPresentation`** (신규, `Entity/Components/` — `IContactable` 式 **defaulted no-op 바디**, 스프라이트/FMOD/Effekseer 타입 0): `ReactDamaged(int)` / `ReactDied(vec3)` / `ReactAttack(vec2)` / `FaceAim(vec2)` / `SetFacing(EFacing)` / `SetPose(EPose)`. (defaulted라 director가 쓰는 verb만 override.)
- **RD1 OPT-1 (Template Method)**: 게임플레이 베이스의 **단일 변이지점**이 sink로 forward. 예: `Life::DoDamaged` 안에서 HP 감소 직후 `if(mSink) mSink->ReactDamaged(dmg)`; `Life::DoDie`에서 `mSink->ReactDied(pos)`. (이미 `DoDamaged→virtual DoDie()` 패턴 존재 — 동형.)
- **RD3**: 베이스 컴포넌트(Life/Weapon/Impulse)가 **직접** sink 인터페이스로 호출(per-entity 서브클래스 override 아님 — RD4). 베이스는 *추상 인터페이스*만 이름지음(deps inward).
- **RD2**: 액터당 sink 1개. **RD6**: sink 없으면 silent no-op(스프라이트 없는 게임플레이-only 액터 정상).
- **`PlayerSpriteDirector : Component, IActorPresentation`** = sink 구현체(연출측). 4-레이어 (EFacing×EPose) 그룹 소유, `SetFacing/SetPose`가 그룹 `SpriteRenderer.Visible` 토글 + B-part anim. `ReactDamaged/Died/Attack`는 (자산 있으면) flash/연출, 없으면 no-op.
- **std::function 유지 seam (OnXXX로 안 접음)**: `BulletContactHandler/Carrier`의 임팩트 FX(월드점 spawn) + `Life::SetOnDeathFx`(사망 spawn) — 이건 "타겟 컴포넌트 반응"이 아니라 Entity→Spawns *spawn-at-point* delegate라 functional 회피 대상 아님.

### 13.3 RD4 — per-entity 서브클래스 = **(나) 차이있을 때만** [확정]
- 공유 `Life`/`Movement`/`Weapon`/`Impulse` 베이스 유지. Player/Enemy 차이 = **Stat 숫자(factory config) + 연출(sink)**로 흡수. **gameplay 서브클래스는 진짜 분기 생길 때만.**
- **`PlayerMovement.{h}` 삭제** (`DoForward(vec2)`가 `IMovable::DoForward(vec2,float)` override 아닌 shadow 버그, 사용처 0).
- blanket 서브클래스 거부 사유: sibling 컴포넌트 합성(SimplePursueAI/Carrier 등)과 경쟁하는 두번째 변이축 + ~12클래스 폭발.

### 13.4 RD5 — facing/pose (PlayerController 단일 작성자) [확정]
| 상태 | 그룹 | facing |
|---|---|---|
| Idle (이동✗ 공격✗) | `(lastFacing, Idle)` | 마지막 유지 |
| Move (이동✓ 공격✗) | `(velFacing, Move)` MoveSprite | velocity 4방향 |
| Attack (공격✓, 이동 정지여도) | `(aimFacing, Move)` MoveSprite | aim 4방향 |
- 공격 종료 시: velocity 있으면 그 방향 Move(Sprite), 없으면 Idle.
- `facing = attacking ? quantize4(aim) : moving ? quantize4(vel) : last; pose = (attacking‖moving) ? Move : Idle;` → 매 프레임 PlayerController가 계산해 `sink->SetFacing/SetPose` push. director는 토글만(단일 작성자).

### 13.5 C1 Carrier 배달 [확정]
- `target->GetComponent<Entity::IDamageable>()->DoDamaged(dmg)` (+`IImpulsable` 넉백). 게이트 완화(13.1)로 인터페이스 직접 조회 가능. 다중 effect fan-out 시만 `ForEachComponent + dynamic_cast`.
- C2 base+subtypes(Projectile/ContactCarrier) / C3 EnemyContactHandler 흡수 + 적 자식 센서 Carrier / C4 SetOnHitFx 흡수 — §7/§8 그대로.

---

## 14. 갱신 빌드 순서 (확정안 — §9 대체)

| Step | 작업 | 경합/비고 |
|---|---|---|
| **0** | 엔진: `GetComponent<T>` 게이트 `is_base_of<Component>`→`is_polymorphic` 완화 (actor.h) + `PhysicsComponent.h:73` 주석 정정 | 비경합·가산적·전제 |
| **1** | `IActorPresentation`(IContactable 式 defaulted) + `IImpulsable`(DoImpulse) 인터페이스 추가 (Entity/Components) | 비경합 |
| **2** | `Life` 확장: i-frame 게이트 fold + `DoDie` 구현(one-shot+SetActive(false)) + `SetOnDeathFx` + OnEnter sink 캐시 + DoDamaged/DoDie에서 `ReactDamaged/ReactDied` 호출 | 비경합 |
| **3** | `EnemyContactHandler` → `Life::DoDamaged` repoint (load-bearing: 접촉 데미지 작동) | Entity |
| **4** | `Impulse`(`Physics/physics_impulse.h`, IImpulsable, DashForce/CoolDownSpeed Stat) + 속도싸움(controller가 `IsActive()` 중 DoForward suppress) | Physics |
| **5** | `Carrier` base+subtypes(Projectile/ContactCarrier) — `GetComponent<IDamageable>` 배달. Bullet→Carrier / 적 자식 센서 Carrier. `BulletContactHandler::SetOnHitFx`·`EnemyContactHandler` 흡수 | Entity/Spawn |
| **6** | `PlayerSpriteDirector : Component, IActorPresentation` — 8그룹 빌드 + SetFacing/SetPose Visible 토글 + React* override. PlayerController가 RD5 규칙 계산→push, flipX 핵 삭제 | **main.cpp Fog Agent 경합** |
| **7** | 철거: `PlayerBehavior.{h,cpp}` + `BulletSpawnPlayable.{h,cpp}` + `PlayerMovement.h`. grep 0참조. `_MyApp_` link-clean | — |

> M6 잔여(R1a/R1b FX 배선)는 Step 2-3·5의 delegate-seam/Carrier에 자연 합류.

---

## 12. 변경 기록
| 일자 | 변경 |
|---|---|
| 2026-06-01 | 분해 설계 작성 — clean-ddd-hex Workflow(6 에이전트) 산출. PB 전 필드 분해 테이블 + 신규 3컴포넌트(Impulse/IImpulsable/PlayerSpriteDirector) + 참조그래프 + load-bearing 수정 + 결정 5건 + 빌드순서 7스텝. 가중치평가 A1/A2 결론 위에 구축. 코드 미변경, 결정 미확정. |
| 2026-06-01 (세션 2) | **§13/§14 확정** — 포트해소 α(GetComponent 게이트 완화 `is_polymorphic`)+OnEnter 캐시 / RD1 OPT-1(Template-Method→`IActorPresentation` sink) / C1 `GetComponent<IDamageable>` / RD2 sink 1개 / RD3 베이스-호출-sink / RD4 (나) blanket 서브클래스 거부 + `PlayerMovement` 삭제 / RD5 facing=attack?aim:move?vel:last·pose=(attack‖move)?Move:Idle / RD6 silent no-op. clean-ddd-hex Workflow(7 에이전트). 모든 §1/§2 기구 잠김. 코드 미변경. |
