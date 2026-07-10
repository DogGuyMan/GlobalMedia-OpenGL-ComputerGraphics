# M6 Sequencing + Sound — As-Built 설계 + 잔여 결정 (스냅샷)

> **작성일**: 2026-06-01
> **브랜치**: `game/module/ingame/temp`
> **성격**: 2026-05-31 원본 spec/plan 의 *구현 진행 스냅샷* — 구현 중 진화한 설계(특히 AudioInstance/VfxInstance) + 완료 상태 + 잔여 작업/결정을 **손실 없이** 보존.
> **원본**:
> - spec [`2026-05-31-m6-sequencing-sound-design.md`](2026-05-31-m6-sequencing-sound-design.md) — Q1~Q5 결정 + 5축 (a)~(e). **여전히 유효** (본 문서는 그 위 진화분만 기록).
> - plan [`2026-05-31-m6-sequencing-sound.md`](../plans/2026-05-31-m6-sequencing-sound.md) — Task 1~10. ⚠ Task 3/4 는 구현 중 진화 → **코드/본 문서 우선**.
> **잔여 plan**: [`2026-06-01-m6-remaining-plan.md`](../plans/2026-06-01-m6-remaining-plan.md) (본 문서와 짝).

---

## §0 진행 스냅샷 (2026-06-01 기준)

M6 = spec+plan 작성(2026-05-31) 후 **subagent-driven 구현 착수 → Tasks 1~5 완료·커밋**. 사용자 지시로 Task 5 후 일시 정지.

### 완료 커밋 (구→신)
| 커밋 | 내용 | 비고 |
|---|---|---|
| `b09d2b2` | (엔진) `Actor::DetachChild`/`FindChild`/`FindChildIf` | **사용자 작성** — 트리 탐색·소유권 이전. `src/scene/actor.{h,cpp}` |
| `ce0e2c9` | Spawns lib 골격 — `SequenceContext`/`AutoDespawnOnFinish`/`OneShotSweeper` | `OneShotSweeper` 가 `FindChildIf` 활용 |
| `ceed879` | (사용자 커밋 "[dev] FMOD Instance") — Audio worldPos + `AudioSystem::SetListener` + **`AudioInstance`** + **새 directional 플레이어 스프라이트 PNG 다수** | Task 3 + 자산 |
| `3fb0c6f` | `mFxRoot` 컨테이너 + 매 프레임 `SweepFinishedChildren` (Fog 리펙토링 위 재통합) | main.cpp |
| `326f19a` | `VfxInstance` + `CombatSequences` + `AmbientSequences` | 단발 빌더 |
| `2ff1c6b` | BGM 인라인 → `Spawns::BuildBGM` 이관 | main.cpp |

→ **빌드 통과**. "끝나면 자동 파괴되는 단발 SFX/VFX" 시스템 + BGM 완성. 모든 변경 커밋됨.

---

## §1 As-Built 아키텍처 — One-Shot Instance 패턴

원본 spec §3 카탈로그는 단발을 *`Parallel(EffekseerPlayable ∥ FmodPlayable)` Composite* 로 정의했으나, 구현 중 사용자 결정으로 **"one-shot instance 팩토리"** 패턴으로 진화했다.

```
[fxRoot Actor] (main 멤버, dir.Root() 자식, render 매 프레임 sweep 대상)
   └─ child "AudioInstance"  → FmodStudioPlayable(desc, pos?) + AutoDespawnOnFinish(thatPlayable)
   └─ child "VfxInstance"    → EffekseerPlayable(mgr, effect, pos)  + AutoDespawnOnFinish(thatPlayable)
   └─ ...

매 프레임 render(): Director.Update(dt)  →  SweepFinishedChildren(*fxRoot)
   → 각 child 의 AutoDespawnOnFinish.IsDone() (= 감시 Playable.IsFinished()) 면 RemoveChild(파괴)
   → leaf dtor 가 FMOD instance/Effekseer handle 정리. (Cocos auto-cleanup 정통)
```

### 핵심 프리미티브 (2종, 대칭)
| 함수 | 파일 | 동작 |
|---|---|---|
| `SpawnAudioInstance(Actor& fxParent, FMOD::Studio::EventDescription* desc, optional<vec3> pos)` | `Spawns/AudioInstance.{h,cpp}` | "AudioInstance" Actor + `FmodStudioPlayable`(pos 있으면 3D set3DAttributes) + AutoDespawn + Play. desc=nullptr no-op. **Studio 전용** |
| `SpawnVfxInstance(Actor& fxParent, VFXSystem* vfx, SJH::Effect* effect, const vec3& pos)` | `Spawns/VfxInstance.{h,cpp}` | "VfxInstance" Actor + `EffekseerPlayable`(Static, pos) + AutoDespawn + Play. effect=nullptr no-op |

### 합성 (gameplay 이벤트)
`Spawns/CombatSequences.{h,cpp}`:
- `SpawnHitSpark(ctx, pos)` = `SpawnVfxInstance(spark) + SpawnAudioInstance(event:/Hit)`
- `SpawnEnemyDeathFX(ctx, pos)` = `SpawnVfxInstance(explosion) + SpawnAudioInstance(event:/EnemyDeath)`
- `SpawnPickupChime(ctx, pos)` = `SpawnAudioInstance(event:/Pickup)`

`Spawns/AmbientSequences.{h,cpp}`:
- `BuildBGM(ctx)` = sceneRoot 밑 "BgmActor" + `FmodStudioPlayable(event:/BGM)` + `SetIsLoop(true)`. **단발 아님 — 지속 loop** (fxRoot/AutoDespawn 미사용).

> 리소스 키(spark/explosion/event:/Hit/EnemyDeath/Pickup) 미존재 시 `FindEffect`/`LoadEvent` nullptr → 자연 no-op (graceful). 실제 자산은 FMOD Studio 프로젝트/bank + Effekseer .efk 추가에 의존.

---

## §2 결정 로그

### §2.1 원본 spec 결정 (유효 — 요약)
Q1 Composite=Component+지연 sweep(Option A) / Q2 명시적 SequenceContext / Q3 PlayerFactory composition root / Q4 임팩트=충돌·사망=self-poll / Q5 listener=카메라 + leaf worldPos. (상세 = 원본 spec §1.)

### §2.2 구현 중 진화 — AudioInstance (사용자 3결정, 2026-05-31)
| # | 결정 | 근거 |
|---|---|---|
| A1 | **AudioInstance = 팩토리 자유함수** (`SpawnAudioInstance`) | Actor 비상속 컨벤션 + 기존 부품(FmodStudioPlayable+AutoDespawnOnFinish) 재사용. "OneShotSweeper 상속" 은 불가(자유함수). |
| A2 | **Studio 전용** | 단발 SFX 를 FMOD Studio 이벤트로 통일. Core(FmodPlayable) 단발 미사용 |
| A3 | **일반 primitive** | `SpawnHitSpark` 등이 오디오 부분을 AudioInstance 에 위임 |

**파생**: 대칭성을 위해 `SpawnVfxInstance`(Effekseer one-shot) 신설 — AudioInstance 와 동일 패턴(Actor+leaf+AutoDespawn). CombatSequences 가 둘을 합성. → 원본 spec §3 의 "Parallel Composite" 단발 정의를 **two independent one-shot instances** 로 대체 (각자 독립 despawn, 더 단순).

### §2.3 엔진 보강 (사용자, 2026-05-31)
`SJH::Scene::Actor` 트리 API 3종 추가 (`src/scene/actor.{h,cpp}`, commit `b09d2b2`):
- `DetachChild(Actor*) → unique_ptr<Actor>` (파괴 없이 분리, OnExit 호출, Godot remove_child 정통)
- `FindChild(name, recursive) → Actor*` (Cocos getChildByName)
- `FindChildIf(Fn, recursive) → Actor*` (술어 탐색, header inline)
→ `OneShotSweeper` 가 `FindChildIf` 로 완료 child 탐색. (단발 폐기는 RemoveChild=파괴, DetachChild 아님 — leaf dtor 가 자원 정리.)

---

## §3 파일 인벤토리 (현 시점)

### 신규 (Spawns lib, `MyApp::Spawns`)
```
apps/_MyApp_/src/Spawns/
├─ SequenceContext.h        # {audio, vfx, reg, world, sceneRoot, fxRoot} POD
├─ AutoDespawnOnFinish.h    # Component — watched IPlayable.IsFinished() → mDone
├─ OneShotSweeper.{h,cpp}   # SweepFinishedChildren(Actor&) — FindChildIf + RemoveChild
├─ AudioInstance.{h,cpp}    # SpawnAudioInstance (Studio 단발)
├─ VfxInstance.{h,cpp}      # SpawnVfxInstance (Effekseer 단발)
├─ CombatSequences.{h,cpp}  # SpawnHitSpark / SpawnEnemyDeathFX / SpawnPickupChime
├─ AmbientSequences.{h,cpp} # BuildBGM
└─ CMakeLists.txt           # myapp_spawns STATIC → MyApp::Entity/Audio/VFX/Tween 단방향 의존
```
`apps/_MyApp_/src/CMakeLists.txt` — `add_subdirectory(Spawns)` + `myapp_client` 우산에 `MyApp::Spawns`.

### 수정
- `src/scene/actor.{h,cpp}` — DetachChild/FindChild/FindChildIf (엔진)
- `apps/_MyApp_/src/Audio/FmodStudioPlayable.{h,cpp}` — ctor `optional<vec3> worldPos` + OnPlay set3DAttributes
- `apps/_MyApp_/src/Audio/AudioSystem.{h,cpp}` — `SetListener(pos,fwd,up)` (Core+Studio)
- `apps/_MyApp_/main.cpp` — Spawns includes + `mFxRoot` 멤버/생성(startup, WramupPlayer 전)/sweep(render)/정리(shutdown) + `SetListener` 호출(render) + BGM `BuildBGM` 이관(WramupFMOD)

---

## §4 잔여 작업 + 결정/블록

### Task 6/7 — PlayerFactory + PlayerSequences 🔴 블록
- **PlayerSequences.{h,cpp}** (Spawns) — 슬롯 빌더 `BuildPlayerAttack/Hit/Dash/Die/MoveEffect`. host(player Actor) 밑 child Actor 의 Composite 반환 → `pb->Set*Playable`. (원본 plan Task 6 = 그대로 유효.)
- **PlayerFactory.{h,cpp}** (Spawns) — `CreatePlayerActorFull(config, ctx)` composition root (의존 사이클 회피 spec §2.1: Spawns→Entity 단방향).
- **PlayerActorConfig 확장** (Entity) — SpriteCfg + BehaviorCfg.
- **🔴 결정 필요 (사용자 = "새 스프라이트 통합")**: repo 에 새 directional 플레이어 스프라이트가 들어옴 — `apps/_MyApp_/resources/texture/player/` 의 `BACK/FRONT × IDLE/MOVE × frame` **개별 PNG**. 현 `SJH::Sprite::UniformAtlas` 는 **단일 그리드 PNG 만 지원**. → 개별 PNG 를 애니로 쓰는 **로딩 방식 미확정**:
  - (후보 a) 개별 텍스처 N장 로드 + sprite 가 프레임마다 texture 전환 (UniformAtlas 확장 또는 신규 sprite 컴포넌트)
  - (후보 b) 빌드/런타임에 grid atlas 로 패킹 후 기존 UniformAtlas
  - (후보 c) 방향별 atlas 파일 분리
  - **재개 시 사용자에 질의 필수.**

### Task 8 — 입력 트리거 🟡 경합
- 좌클릭 → `pb->Attack(ndcDir)` (bullet = #1 슬롯 BulletSpawnPlayable) / Shift → `pb->Dash` / 인라인 G키(main ~285)·ShotComposite(main ~320) 제거.
- **경합**: `src/input/mouse_input.{h,cpp}` + `apps/_MyApp_/src/InputHandler/PlayerController.h` 를 **다른 Agent 수정 중** → 그쪽 완료 후 착수.

### Task 9 — 적 피격/사망 FX 🟢 가능 (multi-file)
- **EnemyDeathHandler.{h,cpp}** (신규 Component, Entity) — `Update` self-poll: `!Life::IsAlive()` → `SpawnEnemyDeathFX(...) + owner->SetActive(false)`. (PlayerBehavior::Die self-poll 미러. 현 `Life::DoDie` 빈 구현 + despawn 부재 보완.)
- **BulletContactHandler** (Entity) — `std::function<void(vec3)> mOnHitFx` delegate 추가, `HandleHit` 의 `DoDamaged` 직후 호출. delegate 바인딩은 bullet 생성 closure(PlayerSequences BuildPlayerAttack 의 BulletFactory)에서 → Task 6 와 연동.
- **WaveController**(Stage, `SpawnEnemy` at `WaveController.cpp:51`)·**enemy_factory**(EnemyConfig 에 `onDeathFx` 추가) — death delegate 주입. WaveController 에 ctx/delegate 멤버+setter 추가, 생성 지점(StageBuilder/CreateStageActor)에서 바인딩. (BulletFactory delegate 정통 — Stage→Spawns 직접 의존 회피.)

---

## §5 경합 + 가드레일 (동시 Agent 환경)

본 세션 중 **2개 이상의 Agent 가 동시 작업** 중임이 드러남:
- **Fog/Bloom PostFX Agent** — `main.cpp` 공유 (현 `game_application` 클래스 + fog/bloom `POSTFX_PROGRAM_CONFIGS`). 1회 reset 으로 미커밋 main.cpp 변경 유실 → 재통합함.
- **입력 Agent** — `mouse_input.{h,cpp}` / `PlayerController.h` 공유.

**가드레일** (재개 시 준수):
1. **별도 파일(Spawns/Entity)** 위주 작업 — main.cpp 는 *surgical*(작은 distinct 영역) 만.
2. main.cpp 변경은 **즉시 커밋** (미커밋 두면 타 Agent reset 으로 유실). 사용자가 자주 fold 함.
3. 경합 파일(mouse_input/PlayerController) 은 해당 Agent 완료 후.
4. `git diff --stat` 로 baseline 확인 후 착수 (HEAD 가 이동했을 수 있음).

---

## §6 비스코프 (변동 없음)
ObjectPool / WaveController FSM화 / 보스(M7) / 단위 테스트([[no_auto_tests]]) / 새 코어 모듈 / `IPlayable`·Composite 변경 / Player FSM(arch-correction D1 폐기). FmodPlayable(Core) worldPos 는 AudioInstance Studio전용이라 **보류**(필요 시 후속).

---

## 변경 기록
| 일자 | 변경 |
|---|---|
| 2026-06-01 | As-built 스냅샷 작성 — Tasks 1~5 완료(6 커밋) + AudioInstance/VfxInstance one-shot 패턴 진화 + Actor 트리 API + 잔여 6/7/8/9 + 경합 가드레일 보존. 작성 Claude (사용자 "손실없이 유지" 지시) |
