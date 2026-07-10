# M6 Sequencing 빌더 + 사운드 본격 통합 설계

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **작성일**: 2026-05-31
> **브랜치**: `game/module/ingame/temp`
> **관련 spec**:
> - [`2026-05-24-topdown-shooter-design.md`](2026-05-24-topdown-shooter-design.md) §부록 D M6 (line 1766) — 정의 원천
> - [`2026-05-26-playable-component-interface-design.md`](2026-05-26-playable-component-interface-design.md) — IPlayable / PlayableBase / Composite 정본 (M3.5, 변경 금지)
> - [`2026-05-26-m5-leaf-playables-design.md`](2026-05-26-m5-leaf-playables-design.md) — leaf 4종 + AudioSystem / VFXSystem 정본 (M5)
> - [`2026-05-26-m4-player-behavior-design.md`](2026-05-26-m4-player-behavior-design.md) — PlayerBehavior flat 메서드 (arch-correction D1)
> - [`2026-05-27-particle-stage-design.md`](2026-05-27-particle-stage-design.md) — ParticleStage (구현 진입 전제조건)
> **진행 보고서**: [`doc/topdown-shooter-progress.md`](../../../doc/topdown-shooter-progress.md) M6
> **결정 횟수**: 5건 (Q1~Q5) + Phase 0 scope 4건
> **평가 프레임**: `<.agents>/skills/clean-ddd-hexagonal/SKILL.md` (Ports & Adapters / 의존성 규칙 / 단일 메커니즘 anti-pattern)

---

## §0 Goal

M5 에서 정착된 leaf Playable 4종(`FmodPlayable` / `FmodStudioPlayable` / `EffekseerPlayable` / `TweenPlayable`) + Composite(`SequencePlayable` / `ParallelPlayable` / `IntervalPlayable`) 를 **실제 게임플레이 행동(발사·피격·대시·사망·BGM)에 결합**한다. M6 는 새 leaf 나 새 코어 모듈을 만들지 않고 **"재료를 조립"** 한다 — 부록 D 의 표현대로 *시퀀스가 코드로 정의되어 readability OK* 상태를 만드는 것이 목표.

본 spec 은 부록 D M6 의 5축 (a)~(e) 를 5개 설계 결정(Q1~Q5)으로 직렬화하고, 구체 시그니처 + 시퀀스 카탈로그 + 비스코프 + 구현 진입 전제조건을 정한다. **구현은 본 spec 범위 밖** — plan(`doc/superpowers/plans/2026-05-31-m6-sequencing-sound.md`)에서 Task 분해.

### §0.1 부록 D M6 5축 ↔ 본 spec 매핑

| 부록 D M6 | 본 spec |
|---|---|
| (a) AudioSystem listener/source 셋업 | §5 (Q5 — listener=카메라, FmodPlayable/FmodStudioPlayable 선택적 worldPos) |
| (b) 발사 시퀀스 빌더 | §3 #1 `BuildPlayerAttack` + §4 입력 트리거 |
| (c) 적 피격 시퀀스 | §3 #6 `SpawnHitSpark` + #7 `SpawnEnemyDeathFX` + §6 (Q4) |
| (d) BGM root-level Parallel loop | §3 #8 `BuildBGM` |
| (e) 시퀀스 5~10개 자산화 | §3 카탈로그 9종 (`apps/_MyApp_/src/Spawns/`) |

---

## §0.2 Phase 0 — 실측 검증 발견 (CURRENT STATE 정정)

본 spec 착수 시 코드 실측으로 진행 보고서 서술과 **다른 3가지**를 확인했다. M6 scope 가 이 발견 위에 세워진다.

| 발견 | 실태 (`592e99b` HEAD) | M6 영향 |
|---|---|---|
| **PlayerBehavior 미배선** | `CreatePlayerActor`([PlayerActor.h:83](../../../apps/_MyApp_/src/Entity/Player/PlayerActor.h))는 Life/Physics/Controller 만 부착. `WramupPlayer`([main.cpp:519](../../../apps/_MyApp_/main.cpp))에 `AddComponent<PlayerBehavior>`·`Init`·`Set*Playable` 호출 0회. 참조처는 `EnemyContactHandler`(`GetComponent`→**nullptr**)·`BulletSpawnPlayable`(인스턴스화 안 됨)뿐 | **M6 가 행동 배선 전체를 흡수** (Phase 0 결정) — 부착+Init+multi-clip+입력 트리거+슬롯 SET |
| **단일 클립 sprite** | [main.cpp:551-555](../../../apps/_MyApp_/main.cpp) — `{0, FrameCount, 4.0f}` whole-atlas 루프 1개. PlayerBehavior 가 기대하는 multi-clip(Idle/Move/Attack/Hit, `EPlayerClip` 0~3) 미설정 | §4 — `RegisterClip` ×4 |
| **적 death/despawn 부재** | `Life::DoDie()`([LifeComponents.h:41](../../../apps/_MyApp_/src/Entity/Components/LifeComponents.h)) 빈 구현. 적 despawn 메커니즘 자체가 없음 — 적은 죽어도 안 사라짐 | §6 — 신규 `EnemyDeathHandler` |

> 진행 보고서의 "M4 완전 배선"(2026-05-26)은 main 분할(`614b366`) + 파일 손상 복구 과정에서 유실된 것으로 추정. M6 가 복원 책임을 흡수.

---

## §1 핵심 결정 (Q1~Q5)

| # | 항목 | 결정 | 거부안 + 근거 |
|---|---|---|---|
| **Q1** | 시퀀스 소유 + one-shot 라이프사이클 | **Option A — Composite=Component(전용 child Actor) + 지연 sweep**. 재사용 슬롯=플레이어 자식 Actor마다 Composite Component 1개(Scene 자동 tick), 단발=FX 컨테이너 child + `AutoDespawnOnFinish` + 매 프레임 sweep | **거부: PlayerBehavior unique_ptr 소유 + 수동 tick** — Scene/PlayerBehavior/TransientSystem 3갈래 틱 = M3.5 가 제거한 `PlayableTickSystem`(병렬 스케줄러) 부활. 단일 메커니즘 anti-pattern. PlayerBehavior 가 스케줄링까지 겸직 = SRP 누수 |
| **Q2** | 빌더 의존성 전달 | **명시적 `SequenceContext` 구조체** 주입. 빌더가 `const SequenceContext&` 로 Audio/VFX/Registry 수령 | **거부: 싱글톤 reach-in** — `Manager::Get()` 내부 호출 = Service Locator(숨은 의존). 빌더가 "무엇에 의존하는가"를 선언 못 함. 명시 주입이 clean-arch + isolation 원칙 부합 |
| **Q3** | PlayerBehavior 배선 위치 | **`CreatePlayerActor` 팩토리 흡수** — player 의 단일 composition root. PlayerActorConfig 에 sprite/behavior cfg 추가, factory 가 Behavior+SpriteRenderer+multi-clip+5슬롯까지 조립 | **거부: WramupPlayer 인라인** — main 비대화. **거부: 전용 setup 자유함수** — factory 가 이미 composition root 인데 조립을 2자리로 분산 |
| **Q4** | Enemy 피격/사망 트리거 | **임팩트=충돌 이벤트(`BulletContactHandler`), 사망=적 self-poll(`EnemyDeathHandler`)** | **거부: 둘 다 BulletContactHandler** — 사망 로직이 공격자(총알)에 결합, 다른 사망 원인 불가. **거부: Life onDeath 콜백** — 공유 컴포넌트 `Life` 시그니처 변경, self-poll 가 PlayerBehavior::Die 패턴과 일관 |
| **Q5** | 2D 공간오디오 | **listener=카메라 위치** 매 프레임 갱신 + **FmodPlayable/FmodStudioPlayable 선택적 `worldPos`** (주면 3D, 안 주면 2D). velocity=0(doppler 없음) | **거부: listener=플레이어** — 카메라 follow 라 근사 동일하나 panning 기준은 화면(카메라)이 정통. **거부: Core만 3D** — Studio 이벤트(적음/임팩트)도 공간감 필요 |

### Phase 0 scope 결정 (4건, 재확인)

| scope | 결정 |
|---|---|
| PlayerBehavior 행동 배선 | **M6 흡수** (Q3 와 연동) |
| 3D audio 범위 | **2D 공간오디오** (Q5) |
| 시퀀스 자산화 위치 | **기존 `Spawns/` 흡수** (새 `Sequences/` 안 만듦) |
| Enemy 연출 범위 | **피격 + 사망 둘 다 M6** |

---

## §2 아키텍처 (Ports & Adapters)

```
[Domain 행동]                    [Port]               [Adapter → 외부 SDK]
PlayerBehavior   ──참조(raw)──>   IPlayable*    ──┬──  FmodPlayable        → FMOD Core
EnemyDeathHandler                (Composite       ├──  FmodStudioPlayable  → FMOD Studio
BulletContactHandler              tree)           ├──  EffekseerPlayable   → Effekseer
                                                  ├──  TweenPlayable       → Tweeny
                                                  └──  BulletSpawnPlayable  → bullet_factory

[Composition Root]  Spawns/ 자유함수 + SequenceContext = adapter 인스턴스화 + Composite 조립 (한 자리)
[Driver]            SJH::Scene (Actor 트리 Update) = Playable 의 *유일한* tick 경로
```

- **의존성 규칙 ✅**: `PlayerBehavior` 는 `IPlayable*` 포트에만 의존(FMOD/Effekseer 무지). adapter 인스턴스화는 `Spawns/`(composition root)에 격리.
- **단일 메커니즘 ✅**: 모든 Playable(슬롯·단발)이 `Component::Update`(Scene 구동) 단일 경로. M3.5 의 "Component 시스템이 PlayableTickSystem 역할" 결정 계승.
- **데이터 흐름**: 입력/충돌 → `PlayerBehavior::Attack/Hit/Dash/Die` 또는 `EnemyDeathHandler` → 슬롯 `IPlayable*->Stop();Play()` → Scene 이 다음 프레임부터 자동 tick → leaf 가 FMOD/Effekseer 구동.

### §2.1 의존 사이클 해소 (plan-time 발견)

빌더를 `Spawns/` 에 두면 (1) `PlayerSequences` 가 `PlayerBehavior`/`BulletSpawnPlayable`(Entity) 에 의존(**Spawns→Entity**) + (2) Q3 의 "Entity 팩토리가 슬롯 빌더 호출"(**Entity→Spawns**) = **순환**. 해소(기존 `BulletFactory` delegate 정통 — "Entity 순환 의존 회피"):

| 규칙 | 내용 |
|---|---|
| **`MyApp::Spawns` → `MyApp::Entity` 단방향** | Spawns 가 Entity 에 의존, 역방향 금지 |
| Entity 소비자는 **`std::function` FX delegate** | `BulletContactHandler::SetOnHitFx` / `EnemyDeathHandler(DeathFx)` 가 `SpawnHitSpark`/`SpawnEnemyDeathFX` 를 *주입받음* → Entity 는 Spawns 무지 |
| **플레이어 composition root = Spawns 거주** | `<Spawns>/PlayerFactory.h` 의 `CreatePlayerActorFull(config, ctx)` 가 Entity 의 base `CreatePlayerActor`(Life/Physics/Controller, **변경 0**) 호출 후 sprite+behavior+5슬롯 조립. Q3 "factory 흡수"의 실질(단일 config-driven 호출 = 완전체 플레이어)은 유지, 위치만 Entity→Spawns(올바른 composition 계층) |

> 즉 Q3 의 "factory 가 전부 조립" 결정은 substance 불변 — `CreatePlayerActorFull` 한 호출이 완전체를 생산. Entity 의 base 팩토리는 그대로.

---

## §3 신규 메커니즘 + 시퀀스 카탈로그 (Client, `apps/_MyApp_/src/Spawns/`)

### §3.1 신규 메커니즘 (4종)

| 신규 | 파일 | 시그니처 / 책임 |
|---|---|---|
| `SequenceContext` | `apps/_MyApp_/src/Spawns/SequenceContext.h` | `struct SequenceContext { Audio::AudioSystem* audio; VFX::VFXSystem* vfx; SJH::ResourceRegistry* reg; };` POD. `Manager::Get().Audio()/.VFX()` + `ResourceRegistry::Get()` 로 1회 조립 |
| `AutoDespawnOnFinish` | `apps/_MyApp_/src/Spawns/AutoDespawnOnFinish.h` | `class AutoDespawnOnFinish : public SJH::Scene::Component` — ctor `(SJH::Playable::IPlayable* watched)`. `Update(dt)` 에서 `watched->IsFinished()` 면 `mDone=true`. getter `IsDone()`. Component hook 들 `OnEnter/OnExit` empty |
| `SweepFinishedChildren` | `Spawns/OneShotSweeper.{h,cpp}` | 자유함수 `void SweepFinishedChildren(SJH::Scene::Actor& fxParent)` — `fxParent.GetChildren()` 순회하며 `AutoDespawnOnFinish` 컴포넌트가 `IsDone()` 인 child 를 수집 후 `fxParent.RemoveChild(child)`. **반복자 무효화 회피 = Update 밖 지연 sweep** (Cocos end-of-frame cleanup 정통) |
| `fxRoot` 컨테이너 | (main 멤버 `SJH::Scene::Actor* mFxRoot`) | 단발 시퀀스 전용 부모 Actor — sweep 대상을 FX 로 한정(전체 Root 순회 회피). startup 에서 `Director::Get().Root().AddChild` |

**`render()` 통합** (main.cpp): `Director::Get().Update(dt)`([main.cpp:189](../../../apps/_MyApp_/main.cpp)) **직후** `SweepFinishedChildren(*mFxRoot)` 1회 호출 (finished_ 플래그가 최신인 시점).

### §3.2 빌더 시그니처 규약 (Q1 + Q2 귀결)

```cpp
// 재사용 슬롯 빌더 — host 에 AddComponent<Composite> 후 raw 반환 (host 소유, Scene tick)
SJH::Playable::IPlayable* BuildPlayerAttack(SJH::Scene::Actor& host, const SequenceContext& ctx, /* deps */);

// 단발 빌더 — fxParent 밑 child Actor + Composite + AutoDespawnOnFinish 부착 후 Play() (fire-and-forget)
void SpawnHitSpark(SJH::Scene::Actor& fxParent, const SequenceContext& ctx, const vmath::vec3& worldPos);
```

### §3.3 시퀀스 카탈로그 (9종 — decision e: 5~10개)

| # | 함수 | 종류 | 구성 (Composite) | 트리거 | 비고 |
|---|---|---|---|---|---|
| 1 | `BuildPlayerAttack` | 슬롯 | `Parallel(BulletSpawnPlayable ∥ EffekseerPlayable "muzzle" ∥ FmodPlayable "gun_shot")` | `PlayerBehavior::Attack` | 인라인 좌클릭(281-308) 이관 |
| 2 | `BuildPlayerHit` | 슬롯 | `Parallel(TweenPlayable shake ∥ FmodStudioPlayable "Damaged")` | `PlayerBehavior::Hit` | **인라인 G키(246-266) 이관** |
| 3 | `BuildPlayerDash` | 슬롯 | `Parallel(EffekseerPlayable "dashTrail" ∥ FmodPlayable "dash")` | `PlayerBehavior::Dash` | dash 리소스 신규 |
| 4 | `BuildPlayerDie` | 슬롯 | `Sequence(FmodStudioPlayable "Death")` | `PlayerBehavior::Die` | |
| 5 | `BuildPlayerMoveEffect` | 슬롯(loop) | `EffekseerPlayable "footDust"(FollowOwner)` + `SetIsLoop(true)` | `PlayClipInternal(Move)` | `mMoveEffect` 슬롯 |
| 6 | `SpawnHitSpark` | 단발 | `Parallel(EffekseerPlayable "spark" ∥ FmodPlayable "hit")` @적 위치 | `BulletContactHandler` | 임팩트 |
| 7 | `SpawnEnemyDeathFX` | 단발 | `Parallel(EffekseerPlayable "explosion" ∥ FmodStudioPlayable "EnemyDeath")` @적 위치 | `EnemyDeathHandler` | 사망 |
| 8 | `BuildBGM` | ambient(loop) | `ParallelPlayable(FmodStudioPlayable "BGM")` + `SetIsLoop(true)` | startup | **인라인 BGM(424-431) 이관** |
| 9 | `SpawnPickupChime` (선택) | 단발 | `FmodPlayable "pickup"` | `PickupTriggerLogger` | M6 여유 시 |

> **핵심 설계 결정 — 슬롯 Composite 는 sprite 클립을 담지 않는다.** 공격/피격 애니는 `PlayerBehavior::PlayClipInternal(clipIdx)` 가 player 의 multi-clip `mSpriteSeq->PlayClip(clipIdx)` 로 구동한다. 슬롯 Composite 는 **효과/스폰/오디오만** 담아 SpriteRenderer 이중 구동을 방지. 부록 D (b) 의 `∥ SpriteSequencePlayable(attackClip)` 문구는 PlayerBehavior multi-clip 도입(M4) 이전 표기이므로 이 결정으로 대체한다.

### §3.4 leaf 시그니처 정합 (실제 헤더 ↔ spec drift 체크)

| leaf | 실제 ctor ([검증]) | M6 사용 |
|---|---|---|
| `FmodPlayable` | `(::FMOD::System*, SJH::Sound*)` → **§5 에서 `std::optional<vmath::vec3> worldPos = std::nullopt` 추가** | reg.FindSound 로 Sound* 획득 |
| `FmodStudioPlayable` | `(::FMOD::Studio::EventDescription*)` → **§5 에서 `std::optional<vmath::vec3> worldPos = std::nullopt` 추가** | audio.LoadEvent 로 desc 획득 |
| `EffekseerPlayable` | `(::Effekseer::ManagerRef, SJH::Effect*, const vmath::vec3& spawnPos, TrackPolicy)` (변경 0) | vfx.GetManager() + reg.FindEffect |
| `TweenPlayable<T>` | `(tweeny::tween<T>, std::function<void(T)>)` (변경 0). ⚠ `step(int32_t ms)` 강제 | shake 람다 |
| `BulletSpawnPlayable` | `(PlayerBehavior*, ...)` — `OnPlay` 가 `behavior->GetAttackDirection()` 으로 발사 | #1 슬롯에 포함 |
| Composite | `SequencePlayable::Append/Insert/AppendInterval`, `ParallelPlayable::Join` (변경 0) | 빌더 조립 |

---

## §4 PlayerBehavior 배선 (Q3 = factory 흡수)

### §4.1 PlayerActorConfig 확장

```cpp
struct PlayerActorConfig {
    // ... 기존 (name, life, movement, controller, physics) ...
    struct SpriteCfg {
        SJH::Sprite::UniformAtlas* atlas = nullptr;
        std::vector<SJH::SpriteSequence::SpriteFrameClip> clips; // Idle/Move/Attack/Hit ×4
    };
    struct BehaviorCfg {
        SequenceContext ctx{};
        float dashSpeed = 7.5f, dashDuration = 0.3f, dashCooldown = 0.8f, hitInvincibility = 0.5f;
    };
    SpriteCfg   sprite;
    BehaviorCfg behavior;
};
```

### §4.2 CreatePlayerActor 조립 순서 (기존 + 신규)

1. Life / (Physics: BoxBody + PhysicsMovement + PlayerController) — **기존 유지**
2. `SpriteRenderer(atlas)` + `SpriteSequencePlayable` — `RegisterClip` ×4 (sprite.clips)
3. `PlayerBehavior` — `Init(spriteSeq, normalSpeed, dashSpeed, ...)`
4. 슬롯 child Actor 5개(`Player_AtkSeq`/`HitSeq`/`DashSeq`/`DieSeq`/`MoveFx`) 생성 → `Build*(child, ctx, ...)` → `pb->Set*Playable(returned)`

> `WramupPlayer`(main)는 atlas 로드 + clip 정의 + config 채우고 `CreatePlayerActor` 호출만 — factory 가 단일 composition root.

### §4.3 입력 트리거 복구 (현재 부재 — 신규)

| 입력 | 현재 | M6 |
|---|---|---|
| 좌클릭 | 인라인 ShotComposite(281-308) | `pb->Attack(ndcDir)` — bullet 은 #1 슬롯의 `BulletSpawnPlayable` 가 처리 |
| Shift | 미사용 | `pb->Dash(wasdDir)` |
| 적 접촉 | `EnemyContactHandler`→`pb->Hit` (기존, pb null→no-op) | pb 부착되어 실작동 |

---

## §5 2D 공간오디오 (Q5 = listener=카메라)

### §5.1 AudioSystem 확장

```cpp
// AudioSystem 신규 메서드
void SetListener(const vmath::vec3& pos, const vmath::vec3& forward, const vmath::vec3& up);
```
- `render()` 에서 매 프레임 `mCamera` owner Transform 으로 갱신. 내부에서 Core `set3DListenerAttributes(0, pos, vel=0, fwd, up)` + Studio `setListenerAttributes(0, attr)` 동시 호출. **velocity=0 → doppler 없음** (Phase 1 리서치).
- 호출 위치: `Manager::Get().Update(dt)` 가 `AudioSystem::Update` 를 부르지만 카메라를 모르므로, main `render()` 에서 `Manager::Get().Audio().SetListener(camPos, ...)` 를 `Manager::Update` 전후로 명시 호출.

### §5.2 leaf worldPos 확장 (M5 leaf = Client, 시그니처 확장 허용)

| leaf | 확장 | 동작 |
|---|---|---|
| `FmodPlayable` | ctor 에 `std::optional<vmath::vec3> worldPos = std::nullopt` | `has_value()` → `FMOD_3D` 모드 + `channel_->set3DAttributes`. `nullopt` → 2D (기존) |
| `FmodStudioPlayable` | 동상 | `has_value()` → `instance_->set3DAttributes`. `nullopt` → 2D |

> 코어 `IPlayable`/`PlayableBase`/Composite 는 **불변** (M3.5 확정). 확장은 Client leaf ctor 한정.
> BGM(#8)·Damaged(#2) 등 비위치 사운드는 worldPos 미전달 → 2D 유지.

---

## §6 Enemy 피격/사망 (Q4)

### §6.1 피격 (임팩트 = 충돌 이벤트 소유)

`BulletContactHandler::HandleHit(other)` ([BulletContactHandler.cpp:20](../../../apps/_MyApp_/src/Entity/Bullet/BulletContactHandler.cpp)):
- 기존 `life->DoDamaged(mDamage)` **직후**, `other` 가 Life 보유 + 생존 시 `SpawnHitSpark(*fxRoot, ctx, other->GetTransform().Translate)` (#6).
- BulletContactHandler 가 `fxRoot` + `SequenceContext` 접근 필요 → ctor 또는 setter 로 주입 (bullet_factory 에서).

### §6.2 사망 (적 상태전이 = 적 소유, self-poll)

신규 `EnemyDeathHandler : SJH::Scene::Component` (`Entity/Enemy/EnemyDeathHandler.{h,cpp}`):
- `enemy_factory` 가 부착 (ctx + fxRoot 주입).
- `Update(dt)`: `!mDead && GetOwner() && !Life::IsAlive()` 감지 시 →
  - `SpawnEnemyDeathFX(*fxRoot, ctx, pos)` (#7)
  - `GetOwner()->SetActive(false)` (despawn — `Life::DoDie` 빈 구현 + despawn 부재 보완)
  - `mDead = true`
- `PlayerBehavior::Die` self-poll([PlayerBehavior.cpp:146](../../../apps/_MyApp_/src/Entity/Player/PlayerBehavior.cpp)) 패턴 미러.

> b2Body despawn 정리(`SetActive(false)` 후 물리 body 처리)는 plan 의 Task 에서 기존 Bullet 의 `SetActive(false)` 처리 방식과 정합하게 확정.

---

## §7 main.cpp 이관 (인라인 제거)

| 제거 대상 | 위치 | 대체 |
|---|---|---|
| G키 DamageComposite | [main.cpp:246-266](../../../apps/_MyApp_/main.cpp) | #2 `BuildPlayerHit` 슬롯 (EnemyContactHandler→Hit 트리거) |
| 좌클릭 ShotComposite | [main.cpp:281-308](../../../apps/_MyApp_/main.cpp) | #1 `BuildPlayerAttack` 슬롯 (좌클릭→Attack 트리거) |
| BGM 인라인 | [main.cpp:424-431](../../../apps/_MyApp_/main.cpp) (`WramupFMOD`) | #8 `BuildBGM` |

> 이관 후 G키는 디버그 토글(F1 ImGui)만 남거나 제거. 좌클릭은 Attack 으로 일원화.

---

## §8 4-엔진 정통 매핑

| M6 요소 | 정통 (Phase 1 리서치) |
|---|---|
| `Spawns/` 자유함수 빌더 | Cocos2d `cc.Sequence`/`cc.Spawn` immediate-mode builder + Godot AnimationLibrary named registry (코드가 곧 named builder) |
| `AutoDespawnOnFinish` + 지연 sweep | Cocos 완료 액션 auto-remove + node `removeFromParent` cleanup / Godot `finished` signal → `queue_free` |
| 슬롯 = 재사용 Composite (`Stop→Play`) | Unity PlayableDirector binding 재사용 (캐릭터별 시퀀스 재활용) |
| listener=카메라 / source worldPos | FMOD 3D events — panning=listener 위치, attenuation=source, velocity=0=no doppler |
| `SequenceContext` 명시 주입 | Clean Architecture 의 composition root / explicit DI (Service Locator 회피) |

---

## §9 명시적 비스코프

| 항목 | 사유 |
|---|---|
| ObjectPool / Bullet spawn 풀링 | M4 측정 보류 (부록 D M4 (d)). M6 무관 |
| WaveController FSM 화 / Stage State | M7 (`SJH::FSM` Stage 전용) |
| 보스 시퀀스 / boss_atlas | M7 |
| 단위 테스트 | [[no_auto_tests]] — 사용자 명시 요청 시에만 |
| 새 코어 모듈(`SJH::*`) | 금지 — 시퀀스/사운드 leaf 는 Client 거주 |
| `IPlayable`/`PlayableBase`/Composite 변경 | M3.5 7결정 확정 — 사용만 |
| `Director`→`Manager` 문서 정합 | 별건 (진행 보고서 정합성 갭 #3). M6 코드는 `Manager` 사용 |
| Player FSM (`PlayerStateMachine`) | arch-correction D1 폐기 — flat PlayerBehavior 유지 |

### §9.1 구현 진입 전제조건 (plan 에 blocking step 으로 명시)

1. **ParticleStage `mStages` 배선 — ✅ working tree 에 이미 완료.** 진행 보고서(커밋본 `592e99b`)는 "미배선"이라 했으나, **working tree `main.cpp:132-147` 에 이미 배선됨**([worldCam, ParticleStage, screenCam, ScreenQuadStage]) + `VFX().Draw()` 직접 호출 없음. 구현 진입 시 빌드/실행으로 파티클 PostFX 적용만 확인하면 됨. revert/미배선 시에만 spec [`2026-05-27-particle-stage`](2026-05-27-particle-stage-design.md) §4.5 적용. (진행 보고서의 stale "0순위" 표기 → 사실상 완료.)
2. (권장) M4 시각 검증 — 본 M6 가 PlayerBehavior 배선을 흡수하므로 M4 별도 검증은 M6 검증에 흡수 가능.

---

## §10 수용 기준 (spec 완료 정의)

- [x] (a)~(e) 5축 전부를 Q1~Q5 + scope 결정으로 커버 (§0.1 매핑)
- [x] 각 결정에 채택/거부 + 근거 명시 (§1)
- [x] PlayerBehavior 슬롯 5개 전부 카탈로그 #1~#5 로 연결 정의
- [x] main.cpp 인라인 G키(246)/좌클릭(289)/BGM(421) 이관 명시 (§7)
- [x] 시그니처가 실제 헤더와 정합 (§3.4 drift 체크)
- [x] 코어 IPlayable 불변, 신규는 전부 Client

---

## 변경 기록

| 일자 | 변경 |
|---|---|
| 2026-05-31 | M6 spec 초안 — Phase 0 실측 발견(PlayerBehavior 미배선/단일클립/적 death 부재) 3건 + Q1~Q5 결정 5건 + scope 4건. Option A(Composite=Component+지연 sweep) / SequenceContext 명시 주입 / factory 흡수 / 임팩트=충돌·사망=self-poll / listener=카메라. 시퀀스 카탈로그 9종. clean-ddd-hexagonal 평가 + 4-엔진 정통 매핑. 작성 Claude (DogGuyMan Q1~Q5 결정) |
