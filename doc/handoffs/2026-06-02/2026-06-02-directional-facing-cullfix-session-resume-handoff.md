> 🔴 **SUPERSEDED (2026-06-03) → 단일 진입점: [`2026-06-03-pb-decomposition-COMPLETE-resume-handoff.md`](2026-06-03-pb-decomposition-COMPLETE-resume-handoff.md)**
> directional = committed `a16eef0`. 이후 Task 5(Carrier)+Task 7(PB 철거)까지 구현 완료. 본 문서의 directional/foundation/cull 잠긴결정·verbatim 은 *참조용* 유효(특히 cull fix 는 여전히 미커밋) — 다만 최신 전체 상태는 후속 문서로.

# Resume Handoff — PlayableDirector 연출 + Directional Facing + Cull Fix (세션 전체)

> **단일 진입점 (single entry point).** 이 문서 하나로 이번 세션 전체 작업을 무손실 재개할 수 있다.
> 참조 spec/plan 은 *선택적 깊이* — 임계 사실은 전부 이 문서에 인라인했다 (spec/plan 은 **gitignored** 라 다른 머신에 안 따라감, 아래 §Pointers 참조).
>
> 작성: 2026-06-02 / 브랜치 `game/module/ingame/temp` / 작성 시점 HEAD `e4f79e4`.
> **🔴 이 문서가 `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-resume-handoff.md` 를 supersede 한다** (그쪽은 directional 착수 전 상태 — 지금은 DONE+committed).

---

## TL;DR + 다음 행동

이번 세션은 `_MyApp_`(탑다운 슈터) 에 **연출(Playable) 파이프라인**을 정착시켰다. 시간순 4단계:

1. **PlayableDirector + PostFXRegistry foundation** — verb→`Play(key)` 디렉터 + PostFX 머티리얼 레지스트리. onFire/onDamage 마이그레이션. ✅ **committed** (`8f276a9`, `aa684f7`).
2. **hit-FX 트랙 + 스프라이트 연출 + 코어 GL_BOOL 갭 수정 + HP-grayscale + P4 적 포트** — "hit"/"death" PostFX + sprite hit-flash/dissolve, 체력비율→무채색 상시 바인더, 적도 동일 연출. ✅ **committed** (`aa684f7`).
3. **Directional Facing/Pose** — 플레이어 4방향(Front/Back/Left/Right)×2포즈(Idle/Move) 스프라이트 전환. spec→plan→subagent-driven 실행. ✅ **committed** (`a16eef0 "[dev] : Playable 4방향"`).
4. **Cull fix (D=오른쪽 투명 버그)** — `src/material/pass.h` AlphaTest 패스 `CullMode=0`(컬링 비활성). ⚠️ **APPLIED + 빌드 GREEN + UNCOMMITTED + GUI 미검증**.

**👉 다음 행동 (단 하나):**
> `_MyApp_` 를 실행해 **D키(오른쪽)/우향 발사 시 플레이어 스프라이트가 더 이상 투명하지 않은지** 육안 확인한다. 정상이면 사용자에게 보고 → 사용자가 `src/material/pass.h` 커밋(커밋은 사용자 권한). 비정상이면 §"Cull fix 상세" 의 대안(A: 셰이더 UV-flip)으로 전환.
>
> 빌드: `cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -4`
> 실행: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`

> ⚠️ **재개 에이전트에게:** 이 브랜치는 다중 에이전트/병렬 커밋으로 자주 드리프트한다. 재개 직전 반드시 `git log --oneline -8` + `git status --short` 를 **다시 측정**하라 (이 문서의 SHA/상태는 작성 시점 스냅샷).

---

## State of the world (재측정 — 2026-06-02)

```
브랜치: game/module/ingame/temp   (PR 대상은 보통 game/main)
HEAD:   e4f79e4
```

최근 커밋 (이번 세션 관련, 신→구):

| SHA | 제목 | 이번 세션? | 비고 |
|---|---|---|---|
| `e4f79e4` | World Text — 데모 트리거(마우스 클릭 데미지 텍스트) | ❌ 병렬(World Text) | SJH::text 트랙 |
| `a16eef0` | **[dev] : Playable 4방향** | ✅ **내 작업 3단계** | directional facing 6파일 |
| `39f8bc0` | World Text — SpawnWorldText(트윈 상승·페이드) | ❌ 병렬 | |
| `58c1ba7` | World Text — MyApp::Text + Manager 배선 **(Init 무인자로 GL 헤더 충돌 회피)** | ❌ 병렬 | ★ 과거 차단 `Manager.cpp` gl3w 에러를 **여기서 수정** |
| `6adaf02` `e3c27df` `d173298` `10825cc` | World Text 모듈 (SJH::text 16→17) | ❌ 병렬 | |
| `aa684f7` | **[dev] : PlayableDirector 연출 통합 (Player/Enemy 빌더 배선) + sprite/PostFX 엔진 지원** | ✅ **내 작업 1·2단계** | foundation + hit-FX + GL_BOOL + P4 |
| `8f276a9` | [dev] : 연출 디렉터 (MyApp::Playable 신규 lib) | ✅ 내 작업 1단계 | Playable lib 신설 |

**Uncommitted (`git status --short`):**

```
 M doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-resume-handoff.md   ← 🔴 이 문서가 supersede (배너 추가됨)
 M doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-task4-done-resume-handoff.md ← 병렬 트랙(PlayerBehavior 분해) 소유
 m extern/Catch2                                                       ← 서브모듈 포인터(건드리지 말 것)
 M src/material/pass.h                                                 ← ★ 내 cull fix (유일한 미커밋 코드)
?? doc/RadialBarShader.shadergraph                                     ← 사용자/병렬 (HP 바 셰이더 작업)
?? doc/RadisalSegmentedHealthBarBuiltIn.shader                         ← 사용자/병렬
?? doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-foundation-done-resume-handoff.md ← 병렬 트랙
```

**핵심:** 이번 세션 코드 중 **미커밋은 `src/material/pass.h` 단 하나** (cull fix). 나머지 directional 6파일 + foundation/hit-FX 는 전부 커밋 완료. `main.cpp` 는 clean(committed) — 내용 변경 없음.

**빌드 상태: GREEN ✅** (cull fix 적용 상태에서 clean configure + `cmake --build` → `[7/7] Linking CXX executable apps/_MyApp_/_MyApp_`. 유일 출력은 sb7 의 허용된 `#warning gl.h and gl3.h both included`).

> 📌 **사전-압축(pre-compaction) 믿음 정정:** "빌드가 World Text `Manager.cpp` gl3w(`PFNGLGETPOINTERVPROC`) 에러로 차단됨" 은 **이제 stale**. 병렬 트랙이 `58c1ba7` 에서 `Manager` Init 을 무인자로 바꿔 GL 헤더 충돌을 해소했다. 현재 빌드는 그린이고 cull fix 는 GUI 검증만 남았다.

---

## 무엇을 만들었나 / 무엇을 바꿨나 (단계별)

### 1단계 — PlayableDirector + PostFXRegistry foundation ✅ committed (`8f276a9`,`aa684f7`)

신규 라이브러리 **`MyApp::Playable`** (`apps/_MyApp_/src/Playable/`):

- **`PlayableDirector.{h,cpp}`** — `: SJH::Scene::Component, public Entity::IActorPresentation`.
  - `Register(key, std::unique_ptr<PlayableBase>)` / `Play(key)`(=Stop()+Play()+playing 플래그) / `Stop(key)` / `Has(key)` / `Update`.
  - **역할 분리(3축):** `[C]` 트리거 이벤트=PlayableDirector / `[A]` 상시 루프=scene Component / `[B]` 월드점 fire-and-forget=Spawns+delegate.
- **`PostFXRegistry.{h,cpp}`** — Meyer's 싱글톤, passName→`SJH::Material::Material*` 맵. PostFX 패스 머티리얼을 이름으로 찾아 uniform 주입.
- **`PostFXTweenPlayable.{h,cpp}`** — 레지스트리 경유 PostFX 머티리얼의 float uniform 을 Tweeny 로 트윈(one-shot).
- **포트&어댑터:** `PlayerController`(driver) → `Entity::IActorPresentation*`(port) → `PlayableDirector`(adapter). InputHandler 는 `MyApp::Playable` 를 link 하지 않는다(인터페이스-only 강제). 그룹 빌드는 `PlayerActor` 가 아닌 **`PlayerBuilder`** 에서(Entity→Playable link 순환 회피).
- **onFire/onDamage 마이그레이션:** 좌클릭 발사/피격 연출을 `director.Play("fire")` / `ReactDamaged` 경로로 회귀 0 이관.

### 2단계 — hit-FX 트랙 + 스프라이트 연출 + 코어 GL_BOOL 갭 + HP-grayscale + P4 적 포트 ✅ committed (`aa684f7`)

- **`SpriteFxPlayable.{h,cpp}`** —
  - `SpriteHitFlashPlayable` : `enableHit` bool uniform 0.18s on (셰이더 `uTime` 애니로 흰 플래시).
  - `SpriteDissolvePlayable` : `enableDissolve` + `dissolveThreshold` 0→1 트윈. **`dissolve.png`**(`apps/_MyApp_/resources/texture/dissolve.png`) 를 `ResourceRegistry` 로 로드해 `dissolveTex` 에 주입(미지정 시 atlas crude erode fallback).
  - `ForEachSpriteRenderer(root, fn)` : root + 직계 child 의 SpriteRenderer 순회.
- **`HpGrayscalePostFX.{h,cpp}`** — **사용자 명시 요구**: CurHp/MaxHp 비율 → `uGrayscaleAmount` 연속 바인더(`[A]` 상시, director 무관). OnEnter 에서 `ILivable` 캐시, Update 에서 `clamp(CurHp/MaxHp)` 기록. HP0=완전 무채색.
- **`Constants.h`** — `EntityTextureConfig`(TexturePath/ColCount/RowCount/Flip/DrawOrder) + `PLAYER_*_IDLE`/`PLAYER_*_MOVE` 레이어 배열들.
- **PlayerBuilder/EnemyBuilder** 배선: director attach + "fire"/"hit"/"death" Register + HpGrayscalePostFX attach. **Life death delay**(`SetDeathDelaySeconds`: player 1.5s / enemy 0.6s) — dissolve 가 보이도록 즉시 비활성 방지.
- **★ 코어 버그 수정 — `src/render/property_block_setter.cpp`**: `PropertyBlockSetter` 에 **`GL_BOOL` 디스패치 누락** → `uEnableHit`/`uEnableDissolve` bool uniform 이 영영 업로드 안 됨(sprite FX 무반응 근본원인). FIX = `case GL_BOOL:` 를 `case GL_INT:` 로 fall-through(`block.Ints` 읽음). **이게 hit-flash/dissolve 가 작동하게 된 결정타.** (메모리 `propertyblock_gl_bool_gap`)
- **`SimplePursueAI.cpp`**: 사망 시 `SetActive(false)` 제거(`SetLinearVelocity(0)` 만) — Life death delay 가 despawn 소유. (제거 안 하면 dissolve 와 레이스 → 적 dissolve 무발현. 사용자 확인 "Dissolve 작동 성공".)
- **P4 적 IActorPresentation 포트**: 적도 PlayableDirector + Register("hit"/"death") 로 SpriteFx 재사용.

### 3단계 — Directional Facing/Pose ✅ committed (`a16eef0 "[dev] : Playable 4방향"`)

6파일(아래 전부 committed):

| 파일 | 변경 |
|---|---|
| `Playable/Constants.h` | `FacingThresholdConfig{vec2 Back,Front,Left,Right}` + `PLAYER_FACING_THRESHOLD` (튜닝 데이터) |
| `Playable/PlayableDirector.{h,cpp}` | `DirGroup{array<SpriteRenderer*,4> layers}` + `RegisterGroup` + `RefreshDirectional()` + `SetFacing/SetPose`(early-out→Apply) + private `idx()/Apply()/mGroups[4][2]/mFacing/mPose` |
| `Bootstrap/PlayerBuilder.cpp` | 8그룹(4방향×2포즈) child 빌드 루프 + RegisterGroup + AddChild 후 RefreshDirectional |
| `InputHandler/PlayerController.{h,cpp}` | `mSink`(IActorPresentation*) + `mLastFacing` + attack window(0.15s) + `QuantizeByThreshold` + RD5 push. 구 `mCachedSprite`/flipX 핵 제거 |

**핵심 알고리즘 (인라인 — spec gitignored):**
- **EFacing/EPose enum** — `apps/_MyApp_/src/Entity/Components/Components.Interfaces.h:85-86`:
  `enum class EFacing : int { Front = 0, Back, Left, Right };` / `enum class EPose : int { Idle = 0, Move };`
  → `idx(EFacing)`=Front0/Back1/Left2/Right3, `idx(EPose)`=Idle0/Move1. `mGroups[4][2]`.
- **단일 작성자(RD5):** `PlayerController` 만 facing/pose 를 계산하고 director 는 적용(SetActive 토글)만.
- **하이브리드 facing:** `attacking ? Quantize(aim) : moving ? Quantize(vel) : lastFacing`. attack window 는 `OnFirePressed` 에서 `mAttackTimer = 0.15f` 로 arm.
- **각도 양자화(튜닝 가능, 사용자 요구):** `QuantizeByThreshold(dir, cfg, fallback)`: `θ = atan2(-dir.z, dir.x)·(180/π)` → `[0,360)` 정규화 → `PLAYER_FACING_THRESHOLD` 의 4범위 중 wrap-aware 매칭(`start>end` 면 0° wrap=Right). no-match=fallback.
  - `PLAYER_FACING_THRESHOLD` = Back`{45,135}`(Up) / Front`{225,315}`(Down) / Left`{135,225}` / Right`{315,45}`(0° wrap). 0°=+X(오른쪽), 반시계 +, 화면 위=-Z. **사용자가 언제든 이 배열만 고쳐 각도 튜닝** (Constants.h).
- **가시성:** `SetActive(false)`(사용자 선택, Visible 아님) — `(mFacing,mPose)` 그룹만 active. `RefreshDirectional()` 은 **AddChild(enter) 후** 호출(inactive-at-enter 회피, PlayerBuilder.cpp:219).

### 4단계 — Cull fix (D=오른쪽 투명 버그) ⚠️ **UNCOMMITTED / GUI 미검증**

**증상:** D키(오른쪽)·우향 발사 시 플레이어 스프라이트가 **투명**해짐. (WASD 전환·발사 조준 응시는 정상.)
**근본 원인:** `billboard_atlas.vs` 의 geometry flip `vec2 p = vec2(aPos.x * uFlipX, aPos.y)` (RIGHT 등 좌우반전 시 `uFlipX=-1`)이 quad **winding 을 뒤집고**, AlphaTest 패스의 `GL_CULL_FACE(GL_BACK)` 가 그 뒤집힌 면을 컬링 → 투명.
**사용자 선택 = 대안 B (컬링 끄기):** `src/material/pass.h` 의 `DefaultPipelineStateOf(Kind::AlphaTest)` 에서 `CullMode = GL_BACK → 0`(0=face culling 비활성). AlphaTest 는 현재 sprite 전용이라 blast radius=스프라이트만.

→ 이 변경은 §"Verbatim recovery" 에 그대로 보존. **빌드 GREEN 확인됨. GUI 검증만 남음.**

---

## Task/Step 상태표

| 단계 | 상태 | SHA / 위치 |
|---|---|---|
| 1. PlayableDirector + PostFXRegistry foundation | ✅ DONE+committed | `8f276a9`, `aa684f7` |
| 2. hit-FX + sprite FX + GL_BOOL + HP-grayscale + P4 적 | ✅ DONE+committed | `aa684f7` |
| 3. Directional facing/pose (Task1~5 plan) | ✅ DONE+committed | `a16eef0` |
| 4. Cull fix (pass.h AlphaTest CullMode=0) | ⚠️ APPLIED, 빌드 GREEN, **미커밋·GUI미검증** | `src/material/pass.h` (uncommitted) |
| 4-검증. D=오른쪽 투명 해소 육안 확인 | ⏳ PENDING (다음 행동) | — |

---

## Locked decisions (재논쟁 금지)

- **포트&어댑터:** PlayerController→`IActorPresentation*`→PlayableDirector. InputHandler 는 Playable link 안 함. 그룹 빌드는 PlayerBuilder(순환 회피).
- **단일 작성자(RD5):** facing/pose 계산은 PlayerController 단독. director 는 SetActive 적용만.
- **가시성 = `SetActive(false)`** (Visible 아님). `RefreshDirectional()` 은 AddChild 후 1회.
- **하이브리드 facing**, attack window **0.15s**.
- **각도 순서 = Back/Front/Left/Right (=Up/Down/Left/Right)** — 사용자 명시. 튜닝은 `Constants.h::PLAYER_FACING_THRESHOLD` 만.
- **Cull fix = 대안 B (컬링 끄기)** — 사용자 선택. (대안 A=셰이더 UV-flip 은 백업.)
- **death 컴포지트는 dissolve 만** (grayscale 은 HpGrayscalePostFX 가 HP비율로 상시 구동 — 둘이 같은 `uGrayscaleAmount` 쓰면 충돌).
- **EnemyBuilder idle-tween(~L60-105) 은 의도된 것** — Tweeny 도입 동기와 일치, 제거 대상 아님(사용자 명시).
- **`Quantize4`(Components.Interfaces.h:90) 는 읽기 전용** — directional 은 별도 `QuantizeByThreshold`(PlayerController.cpp) 사용. Quantize4 본체 미수정.

---

## 병렬 트랙 충돌 매트릭스

이 브랜치는 **다중 에이전트**가 동시에 커밋한다. 파일 소유권:

| 파일/디렉토리 | 소유 트랙 | 재개 에이전트 행동 |
|---|---|---|
| `apps/_MyApp_/src/Playable/*` | **이 세션(연출)** | ✅ 내 소유 |
| `src/material/pass.h` | **이 세션(cull fix, 미커밋)** | ✅ 커밋/검증 대상 |
| `src/render/property_block_setter.cpp` | 이 세션(committed) | 코어 — 추가 수정 신중 |
| `apps/_MyApp_/src/Text/*`, `Manager.{h,cpp}` | **병렬: World Text (SJH::text)** | ⛔ 건드리지 말 것 (`58c1ba7`~`e4f79e4`) |
| `apps/_MyApp_/src/Entity/...`(PlayerBehavior 분해) | **병렬: PB 분해 트랙** | ⛔ `enemy_factory`/Life 코어/Timer 미접근 |
| `doc/RadialBarShader.shadergraph`, `*HealthBar*.shader` | 사용자/병렬 (HP 바) | ⛔ 미접근 |
| `extern/Catch2`(서브모듈 포인터) | — | ⛔ 건드리지 말 것 |

**안전 수칙:** `git add -A` 금지 → **path-scoped** 만 (`git add src/material/pass.h` 처럼). 다른 트랙 작업을 휩쓸지 않기 위함.

---

## Guardrails & conventions (Step 0 — 반드시 준수)

- **커밋 안 함** (사용자가 커밋 관리). 구현+보고만. 미커밋 코드는 §Verbatim recovery 에 보존.
- **`Co-Authored-By` 트레일러 미사용** (이 프로젝트 컨벤션).
- **`git add -A` 금지** — 다중 에이전트 브랜치라 path-scoped 만.
- **테스트 자동 추가 금지** (`no_auto_tests`) — 사용자 요청 시에만. 검증 = 빌드 exit0 + GUI 육안.
- **주석 한국어**, 사용자 소통 한국어.
- **헤더 가드 `__XXX_H__`** (대문자+언더스코어). `#pragma once` 미사용.
- **`long` 금지** → `int32_t`/`uint64_t` 등 고정폭. 파일 경로 슬래시(`/`). I/O 바이너리 모드.
- **미접근:** `PlayerActor.{h,cpp}`(Entity→Playable 순환), 코어 `src/*`(GL_BOOL fix 외), 셰이더, `Spawns`, `EnemyFactory`, `Life` 코어, `main.cpp` PostFX 라인, `Quantize4`, World Text/PB분해 트랙 파일.
- **빌드:** `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` (sb7 `#warning gl.h+gl3.h` 1건은 허용).
- **실행:** `cd build_ninja/apps/_MyApp_ && ./_MyApp_` (리소스 상대경로라 cd 필수).
- **확신 부족 시 사용자에게 질의** — spec/plan 단락별 질의가 사용자 선호(메모리 `spec_phase_by_phase_inquiry`).

---

## Pointers (참조 깊이 — 선택적)

- **spec (🔴 gitignored — 다른 머신에 안 따라감):** `doc/superpowers/specs/2026-06-02-directional-facing-pose-design.md` (§1-6).
- **plan (🔴 gitignored):** `doc/superpowers/plans/2026-06-02-directional-facing-pose.md` (Task1-5, 완전 코드). → 위 §3단계 핵심 알고리즘에 임계 사실 인라인 완료.
- 정본 spec(tracked): `doc/superpowers/specs/2026-05-26-playable-component-interface-design.md`(IPlayable), `2026-05-26-m5-leaf-playables-design.md`.
- 진행 보고서: `doc/topdown-shooter-progress.md`.
- **이 문서가 supersede:** `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-resume-handoff.md`(directional 착수 전 상태) + `...-directional-facing-pose-agent-prompt.md`(실행 완료된 프롬프트).
- 관련 earlier-session 핸드오프(전부 committed at `aa684f7`): `playable-director-foundation-DONE-resume-handoff.md`, `player-hit-fx-decision-response.md`, `player-hit-fx-playable-design-handoff.md`, `enemy-hit-death-fx-wiring_m6task9.md`, `p4-enemy-presentation-port-p5-reconcile-agent-prompt.md`.
- 메모리: `playable_director_foundation`, `propertyblock_gl_bool_gap`, `camera_depth_postfx_misuse`, `pass_component_postfx_pattern`, `efk_texture_basepath_trap`.

---

## Verbatim recovery — 미커밋 코드 (reset 으로도 안 잃도록)

**파일: `src/material/pass.h`** — `DefaultPipelineStateOf(Kind::AlphaTest)` 내부. 적용된 diff:

```diff
 		case Kind::AlphaTest:
+			// CullMode 0 = face culling 비활성 — sprite(빌보드)는 카메라-정면 2D quad 라 컬링이 무의미하고,
+			// flipX(=-1, RIGHT 등 좌우반전)가 quad winding 을 뒤집어 GL_BACK 컬링 시 투명해지는 버그를 막는다.
+			// (AlphaTest 는 현재 sprite 전용 — blast radius = 스프라이트만.)
 			return PipelineState{
 			    /*DepthTest*/ true, /*DepthWrite*/ true,
-			    /*DepthFunc*/ GL_LEQUAL, /*CullMode*/ GL_BACK,
+			    /*DepthFunc*/ GL_LEQUAL, /*CullMode*/ 0,
 			    /*BlendEnable*/ false, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
 			    /*QueueLayer*/ QueueOf(Kind::AlphaTest)};
```

재적용 시: `src/material/pass.h` 에서 `Kind::AlphaTest` 케이스의 `/*CullMode*/ GL_BACK` 을 `/*CullMode*/ 0` 으로 바꾸고 위 주석 3줄 추가.

**대안 A (B 가 부작용 보이면 — 백업):** 컬링 유지 + 셰이더에서 UV 만 flip(geometry flip 제거). `billboard_atlas.vs` 의 `vec2 p = vec2(aPos.x * uFlipX, aPos.y)` 를 `vec2 p = aPos.xy` 로 되돌리고, fragment 의 UV 샘플링에서 `uFlipX<0` 일 때 `uv.x = 1.0 - uv.x`. ⚠️ 셰이더는 "미접근" 가드 대상 — 대안 A 채택 시 사용자 승인 필요.

---

## Change log (append-only)

- **2026-06-02 (이 문서 작성):** 세션 전체(foundation→hit-FX→directional→cull fix) 무손실 기록. 재측정 결과 — directional 6파일+foundation/hit-FX 는 `a16eef0`/`aa684f7` 로 **committed**, cull fix(`pass.h`)만 미커밋·빌드 GREEN·GUI 미검증. 사전-압축 믿음("Manager.cpp gl3w 빌드 차단")은 `58c1ba7` 에서 병렬 트랙이 해소 → **정정**. 다음 행동 = D=오른쪽 투명 GUI 검증 → 사용자 커밋.
