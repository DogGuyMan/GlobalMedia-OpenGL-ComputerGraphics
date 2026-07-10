# Resume Handoff — Entity Accessor-facade 후속 리팩토링 (2026-06-03)

> **이 문서 = 다음 세션의 단일 진입점.** Entity Accessor-facade(Part A~D') **구현·커밋 완료** + `CreatePlayerActor` 자유함수 분해 **완료**. 남은 것은 **빌더(BuildPlayer/EnemyBuilder) 후속 리팩토링 백로그 ⓐ~ⓓ + config 정리** — *분석만 됨, 미구현*. 이 문서 하나로 무손실 재개 가능(임계 사실 전부 인라인).
>
> 작성: 2026-06-03 / 브랜치 `game/module/ingame/temp` / 작성 시 HEAD `f91e980`.
> ⚠ **이 repo는 다중 에이전트 병렬 커밋 중**(Box2D raycast / HealthBar / World Text / 사용자 직접 git). 재개 시 **반드시 `git log --oneline -12` + `git status --short` 재측정** 후 아래 표 보정.

---

## TL;DR + 다음 행동

- **완료(커밋)**: Entity Accessor-facade 전체 + Physics body 컴포넌트화 + 적 넉백 + CreatePlayerActor 자유함수 분해. (SHA 아래 §1)
- **다음 = 빌더 후속 리팩토링** (분석 완료, 미구현). 우선순위:
  1. **ⓐ `AttachSpriteLayer` 공유 헬퍼** (가장 가치 큼 — 3-빌더 진짜 중복 제거). ← **여기부터 권장**
  2. **ⓑ BuildPlayer god-function 분할** (자유함수, `f91e980` 방식 그대로).
  3. **ⓒ `AttachEntityPresentation` 공유 헬퍼** (Player·Enemy 연출 배선 공통화).
  4. ⓓ facade-director 입력 콜백 정리 — **보류**(순이득 아닌 trade-off).
  5. config: `kGroups`/`kFps` → **`Playable/Constants.h`**(presentation), *`PlayerActor.h` 아님*.
- **카덴스**: 단계마다 빌드 GREEN 확인 → 사용자 승인 → path-scoped 커밋. 사용자가 "내가 지시한 것만 진행"을 강조함 → **지시 범위 밖 진행 금지**.

```bash
# 빌드(검증의 전부 — no_auto_tests) / 실행(GUI 육안)
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
cmake --build --preset ninja --target _MyApp_      # exit 0 (sb7 gl.h+gl3.h #warning 1건만 허용)
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

---

## §1. State of the world (재측정 — 2026-06-03)

```
브랜치: game/module/ingame/temp
HEAD:   f91e980  [refactor] CreatePlayerActor 자유함수 분해
```

**이 세션 작업 커밋 체인 (★=내 작업):**

| SHA | 제목 | 비고 |
|---|---|---|
| ★`f91e980` | CreatePlayerActor 자유함수 분해 (init/wire 분리 + if/else 분기함수 + 파이프라인) | HEAD |
| `b73e1e0` | [chore] 주석 변경 | 사용자/병렬 |
| ★`4ba33a5` | EnemyEntity facade + 적 넉백 — Impulse SJH::Timer화(force 2.5) + AI/Controller 게이트 + 넉백방향=비행방향 fix | Part D+D' |
| ★`e034d49` | Entity Accessor-facade — BaseEntity(공유 4캐시)+PlayerEntity 부착 + Entity↔Playable CMake 순환 회피 | Part B+C |
| `17464bd` | Entity Impls | 사용자/병렬 |
| `9a9cbb3`,`bdf83af` | Box2D raycast 구현/헤더 | **병렬(내 작업 아님)** |
| ★`4719e5d` | 총알 kinematic→dynamic+sensor (벽 명중 despawn fix) | Part A 후속 |
| ★`32d7c93` | 물리컴포넌트 수정 (= BodyConfig + BoxBody/CircleBody ctor eager 생성) | Part A |
| `1bb2937`,`5e0343f`,… | enemy hp bar / HealthBar 적 확장 | 병렬(HealthBar) |

**working tree 미커밋 (전부 *내 작업 아님* — 미접근):**
```
 M apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp   ← 내가 안 건드림(분석만). linter 화살표 정규화 or 사용자 추정 — 만지기 전 diff 확인
 M apps/_MyApp_/src/VFX/CMakeLists.txt            ← 병렬(VFX)
 M src/timer/CMakeLists.txt                       ← 병렬
 m extern/Catch2 (서브모듈)  /  ?? Example/ (C# 참조 — 사용자가 추가)
 M doc/handoff/...(여러)  ?? doc/handoff/...COMPLETE...md  ← 이전 PB분해 핸드오프들
```

**빌드: GREEN** — `cmake --build --preset ninja --target _MyApp_` → exit 0 (작성 시점 확인).

---

## §2. 완료된 아키텍처 (grounded — 재개 시 이걸 전제로)

### Entity Accessor-facade (커밋 `e034d49`,`4ba33a5`)
- **`apps/_MyApp_/src/Entity/BaseEntity.{h,cpp}`** — 공유 facade. `class BaseEntity : Component, ILivable, IDieable, IDamageable, IImpulsable`. **`OnEnter`에서 4캐시**: `mLife`(GetComponent<Life>), `mPhysics`(FindPhysics), `mDirector`(GetComponent<PlayableDirector>), `mImpulse`(GetComponent<Impulse>). 위임: IsAlive/GetHp/GetMaxHp/DoDamaged/DoDie(→Life), DoImpulse/IsImpulseActive(→Impulse). accessor: GetPhysics/GetDirector/Play(key)(→Director). **헤더는 fwd-decl만**(완전형은 .cpp) → Entity 헤더가 Playable/Physics 헤더 안 끌어옴.
- **`Entity/Player/PlayerEntity.{h,cpp}`** — `class PlayerEntity : BaseEntity, IMovable`. 캐시 `mMovement`(GetComponent<**PhysicsMovement**> — 자기매칭 회피 위해 구체타입)/`mWeapon`. accessor GetMovement/GetWeapon + verb DoForward/Dash/Attack.
- **`Entity/Enemy/EnemyEntity.h`** — `class EnemyEntity : BaseEntity {}` header-only.
- **CMake 순환 회피**: `Playable/CMakeLists.txt`에서 `MyApp::Entity` 링크 **제거**(IActorPresentation은 헤더-only라 include로 해소), `Entity/CMakeLists.txt`에 `MyApp::Playable` **PRIVATE** 추가(BaseEntity.cpp .cpp-only 사용).

### Physics body 컴포넌트화 (커밋 `32d7c93`,`4719e5d`)
- **`Physics/PhysicsComponent.h`** — `struct BodyConfig{world,bodyType,startPosition,linearVelocity,linearDamping,density,friction(=0.2 기본),isSensor,categoryBits,maskBits,heightOffset}` + protected `MakeBody(cfg)`/`InitBody(body,cfg)` + **concrete `OnEnter`**(owner userdata 등록 — ctor엔 GetOwner=null이라 여기서). `OnExit`/`Update` pure 유지.
- **`PhysicsComponent.Imp.h`** — `BoxBody(BodyConfig,size)` / `CircleBody(BodyConfig,radius)` **ctor에서 eager body 생성**.
- **5 call-site 전부 변환**: PlayerActor.cpp(Box), EnemyFactory.h(Circle), **wall_factory.h(static), pickup_factory.h(static+sensor), bullet_factory.h(kinematic→dynamic+sensor+velocity)**. friction 미설정 site(벽/픽업/총알)는 BodyConfig 기본 0.2로 box2d 기본 보존.
- **총알**: `bullet_factory.h` — dynamic+sensor(월드중력 0 → 안 떨어짐, static 벽과 접촉 생성), `proj->SetLaunchDir(cfg.dir)`.

### 적 넉백 (커밋 `4ba33a5`)
- **`Physics/PhysicsImpulse.h`** — active/cooldown을 `SJH::Timer`로(arm-inactive: 생성 직후 `Tick(base)`→finished). `IsActive()`=`!activeTimer.IsTimesUp()`. **force 2.5**(7.5의 1/3, 사용자 튜닝). active 0.3s/cooldown 0.8s.
- **`Entity/Enemy/EnemyFactory.h`** — `AddComponent<Physics::Impulse>()` + `AddComponent<EnemyEntity>()`.
- **`SimplePursueAI.{h,cpp}`** — `OnEnter`에서 `mEntity`(BaseEntity) 캐시. `Update` 3분기: 사망(`!IsAlive()`→속도0) / **넉백(`IsImpulseActive()`→return, 추적 skip)** / 추적. (raw `GetComponent<Life>` 매프레임 제거.)
- **`PlayerController.{h,cpp}`** — `mEntity` 캐시 + `Update`에서 `if(!IsImpulseActive()) DoForward(...)` 게이트. **dormant**(dash 입력 미배선 → 항상 false → 플레이어 행동 변화 0).
- **`Spawns/Carrier.h`** — `Projectile::SetLaunchDir(box2dDir)` + `mLaunchDir`. 넉백 = **비행방향**(`Deliver(other, vmath::vec2(mLaunchDir[0], -mLaunchDir[1]))` = box2d XY→world XZ). *위치차분(enemy-bullet) 폐기* — 관통깊이로 부호 뒤집혀 "플레이어 쪽 돌진" 버그 유발했음.

### CreatePlayerActor 자유함수 분해 (커밋 `f91e980`)
- **`Entity/Player/PlayerActor.cpp`** — anonymous namespace 자유함수 8개: `[1]` 초기화 `InitLife`/`InitPhysicsBranch`(if physics guard)/`InitMovementBranch`(else guard)/`InitController`(if keyboard)/`InitSprite`(if direction)/`InitFacade`. `[2]` 연결 `WireWeapon`(SetWorld)/`WireController`(setter fluent 체인 + SetUp, IMovable 타깃=PhysicsMovement 우선·없으면 Movement). 본체 = 순차 파이프라인. **동작 보존**(컴포넌트 집합·순서·wiring 동일).

---

## §3. 남은 백로그 (분석 완료 — 미구현). *다음 작업 = 여기*

### ⓐ `AttachSpriteLayer` 공유 헬퍼 — 🔴 최우선 (진짜 중복)
"텍스처 1장 → atlas find-or-create + SpriteRenderer(+애니) 부착" 패턴이 **3곳 동일**:
- `Bootstrap/PlayerBuilder.cpp` 8그룹 내부루프 (현재 ~:95-120)
- `Entity/Player/PlayerActor.cpp` `InitSprite`(4-layer, 현재 dead — BuildPlayer가 direction=nullptr)
- `Bootstrap/EnemyBuilder.cpp` (현재 ~:44-57, owner-direct)

**제안 헬퍼** (위치: presentation 레이어 — 예 `Playable/` 또는 sprite 인접; **도메인 의존 0**, reg/texCfg만):
```cpp
// EntityTextureConfig = TopdownShooter::Playable::EntityTextureConfig (Playable/Constants.h)
SJH::Sprite::SpriteRenderer*
AttachSpriteLayer(SJH::Scene::Actor& target, SJH::ResourceRegistry& reg,
                  const TopdownShooter::Playable::EntityTextureConfig& t, float fps)
{
    auto* atlas = reg.FindUniformAtlas(t.TexturePath);
    if (!atlas) atlas = reg.CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
    if (!atlas) { spdlog::error("[sprite] atlas 실패: {}", t.TexturePath); return nullptr; }
    auto* spr = target.AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
    spr->flipX = t.Flip;  spr->QueueOffset = t.DrawOrder;
    if (t.ColCount > 1) {
        auto* seq = target.AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
                        spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, fps});
        seq->SetIsLoop(true);  seq->Play();
    }
    return spr;
}
```
→ BuildPlayer 8그룹 내부가 ~4줄로(`auto* cp=spriteActor->AddChild(...); auto* spr=AttachSpriteLayer(*cp,reg,t,kFps); if(spr&&li<4) dg.layers[li]=spr;`). 3곳 dedup.
⚠ `EnemyBuilder.cpp` 수정 필요 → **§4 충돌매트릭스 확인**(HealthBar 트랙이 EnemyBuilder 만짐).

### ⓑ BuildPlayer god-function 분할 (자유함수, `f91e980` 방식)
`PlayerBuilder.cpp` 본체에서 전용 블록을 anonymous-namespace 자유함수로:
- `BuildPlayerDirectionalGroups(spriteActor, director)` ← 8그룹(현재 ~:77-124)
- `RegisterPlayerCombatPlayables(director, spriteActor)` ← fire/hit/death Register(현재 ~:134-184)

### ⓒ `AttachEntityPresentation` 공유 헬퍼 (Player·Enemy 연출 공통)
두 빌더 공통 배선 = director 부착 + `"death"`=SpriteDissolve(dur) + SetDeathDelaySeconds(dur) + AttachHealthBar(color) + 기본 `"hit"`=SpriteHitFlash. **Player는 "hit"을 Parallel(vignette∥hitflash∥sound)로 overwrite**(PlayableDirector 동일키 overwrite 지원). 위치 = **Bootstrap(composition root)**. timing은 **pre-entry로 통일**(현재 Player는 healthbar를 post-entry — 동작 동등).
```cpp
struct EntityPresentationConfig { float dissolveSeconds; float deathDelaySeconds; vmath::vec4 healthBarColor; };
Playable::PlayableDirector* AttachEntityPresentation(SJH::Scene::Actor& actor, const EntityPresentationConfig& cfg);
```

### ⓓ facade-director 입력 콜백 (보류 — trade-off)
PlayerController가 이미 `mEntity`(BaseEntity) 캐시 → `mEntity->Play("fire")`/`Play("hit")`로 BuildPlayer의 `SetFireCallback`/`SetDamageCallback` 배선(:186-193) 제거 가능. **단 순이득 아님**: controller가 cue명("fire"/"hit") 하드코딩(결합↑) vs 콜백 분리(decoupled). 사용자 결정 필요. (⚠ `mEntity` lazy 캐시라 첫 Update 전 OnFirePressed 시 null → OnEnter 캐시로 옮겨야 안전.)

### config 정리: `kGroups`/`kFps` → `Playable/Constants.h` (*`PlayerActor.h` 아님*)
- 판정: 이건 **presentation 데이터**(BuildPlayer가 소비, CreatePlayerActor는 안 봄). `PlayerActor.h`는 *도메인 팩토리* config(hp/speed/damage/physics)용. → `Playable/Constants.h`(텍스처 세트 옆)가 정위치.
- `kFps`(8.0f)는 **`SpriteCfg::fps`(8.0f)와 중복** — 단일 소스 아님. presentation 상수 `PLAYER_ANIM_FPS`로.

---

## §4. 병렬 트랙 충돌 매트릭스 (재측정)

| 파일/영역 | 소유 | 재개 행동 |
|---|---|---|
| `Bootstrap/PlayerBuilder.cpp` | **미커밋 `M`(내 작업 아님)** — linter/사용자 추정 | ⓐⓑ 진행 전 `git diff`로 내용 확인. 충돌 시 사용자 조율 |
| `Bootstrap/EnemyBuilder.cpp` | **병렬: HealthBar 트랙** (커밋 `5e0343f`) | ⓐⓒ가 수정 필요 → **surgical만**, 사용자 확인. HealthBar 미커밋 있으면 대기 |
| `Physics/` (raycast `9a9cbb3`/`bdf83af`) | **병렬(내 작업 아님)** | 미접근 |
| `src/Text`,`src/text`,`Manager`,`main.cpp` | 병렬: World Text / Fog | ⛔ 미접근 |
| `VFX/CMakeLists.txt`,`src/timer/CMakeLists.txt`,`extern/Catch2`,`Example/` | 병렬/사용자 | ⛔ 미접근 |
| `Entity/`,`Playable/Constants.h`,`Spawns/Carrier.h` | **내 작업(커밋됨)** | ⓐ(헬퍼)·config는 여기 추가 — 충돌 낮음 |

**⚠ 사용자 병렬 패턴**: 같은 working tree에서 staging/커밋/빌드. **`git add -A`/`git commit`(인덱스 전체) 금지 → 반드시 `git commit <경로>` path-scoped.** 탈락 커밋은 `git reflog` 복구.

---

## §5. Locked decisions / 판단 (재논쟁 금지)

1. **PlayerEntity 캐싱 = OnEnter(씬 진입) 시점** — `AddComponent` 때 아님. `CreatePlayerActor`의 액터는 `mEntered=false`라 OnEnter deferred. `dir.Root().AddChild`(BuildPlayer)에서 enter → 모든 컴포넌트 OnEnter 일괄 발화, 그때 형제(+director) 전부 맵에 존재 → 캐시됨. **부착 순서 무관**. `InitFacade` 마지막은 관례/가독성 — **현 코드 정확**.
2. **config 위치 by 레이어**: 게임플레이 수치 → `PlayerActorConfig`(domain). 연출 수치(sprite fps/방향테이블/FX타이밍) → `Playable/Constants.h`(presentation). 섞지 말 것.
3. **clean-ddd-hexagonal 평가**: 이 프로젝트는 solo/single-entry/fixed-infra/게임 → 스킬의 "Skip" 컬럼. Aggregate/Repository/UseCase/CQRS = **Premature 안티패턴(도입 금지)**. 적용 원칙 = 의존방향+책임분리+포트(이미 대부분 충족). **실용 plan(ⓐⓑⓒ 자유함수/DRY)과 clean-ddd plan은 수렴** — 후자는 레이어 명명/정당화만 추가, *다른 무거운 구조 아님*.
4. **넉백 방향 = 총알 비행방향**(cfg.dir, box2d XY → world XZ via `(x, -y)`). 위치차분 폐기(이중변환 부호반전 + 관통노이즈).
5. **Impulse force 2.5** (사용자 1/3 튜닝). `Impulse::DoImpulse(dir)` 계약 = **world XZ in** → 내부서 box2d 변환(`SetLinearVelocity(x, -z)`). PhysicsMovement도 동일 계약.
6. **CreatePlayerActor 리팩토링 = 자유함수**(빌더 클래스 거부됨). [1]init/[2]wire 분리 + if/else→반대 guard 두 함수 + fluent 체인.

---

## §6. Guardrails & conventions (carry-forward)

- **커밋 = 사용자 승인 후**(무단 금지). **path-scoped `git add <경로>`** (`-A`/`.` 금지). `Co-Authored-By` 미사용.
- **`no_auto_tests`** — 단위테스트 작성 금지. 검증 = `cmake --build --preset ninja --target _MyApp_` **exit 0** + GUI 육안(`cd build_ninja/apps/_MyApp_ && ./_MyApp_`).
- **주석 한국어**. 헤더가드 `__XXX_H__`/`_XXX_`(#pragma once 금지). `long` 금지(int32_t/uint64_t). 경로 슬래시. windows.h는 `#ifdef _WIN32`+NOMINMAX.
- ctor에서 sink/FindPhysics/virtual 해소 금지 → `OnEnter`/lazy.
- clangd Information-level "No header providing … directly included"(missing-includes) = box2d/vmath 우산 헤더 전파 **노이즈**(빌드 무관). 무시.
- **사용자 작업 스타일**: "내가 지시한 내용만 진행" 강조 — **지시 범위 밖 작업/커밋 금지**. 분석 요청엔 분석만(구현은 별도 지시 시).

---

## §7. Pointers

- **완료 작업 spec/plan (🔴 gitignored — 로컬 전용, 다른 머신에 안 따라감)**: `doc/superpowers/specs/2026-06-03-entity-accessor-facade-design.md` + `doc/superpowers/plans/2026-06-03-entity-accessor-facade.md`. → Part A~D' 정본. **남은 백로그(ⓐ~ⓓ+config)는 spec 없음 — 본 §3에 인라인.**
- 선행 핸드오프: `doc/handoffs/2026-06-03/2026-06-03-pb-decomposition-COMPLETE-resume-handoff.md` (PlayerBehavior 분해 — 이 facade 작업의 전제. 그건 이미 완료된 분해, 본 문서가 그 *후속*).
- 메모리(`.claude/.../memory/`): [[next_work_playable]], [[playable_director_foundation]], [[user-parallel-git-and-builds]], [[propertyblock_gl_bool_gap]].

---

## §8. Change log (append-only)
- **2026-06-03 (본 문서 생성)**: Entity Accessor-facade(Part A~D' = `32d7c93`/`4719e5d`/`e034d49`/`4ba33a5`) + CreatePlayerActor 자유함수 분해(`f91e980`) **완료·커밋**. 빌드 GREEN. 남은 백로그 ⓐ(AttachSpriteLayer 3-빌더 dedup, 최우선)/ⓑ(BuildPlayer 분할)/ⓒ(AttachEntityPresentation)/ⓓ(facade-director 콜백, 보류) + config(kGroups/kFps→Playable/Constants.h) **분석만**. 판단 6종 잠금(§5). 병렬: Box2D raycast/HealthBar/World Text, 사용자 병렬 git, `M PlayerBuilder.cpp` 미커밋(내 작업 아님).
