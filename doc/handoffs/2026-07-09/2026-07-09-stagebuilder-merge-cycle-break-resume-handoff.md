# Resume Handoff — StageBuilder → WorldSceneBuilder 흡수 + Bootstrap↔Stage 사이클 절단

> ⚠ **이 문서가 있는 `doc/` 는 gitignored(로컬 전용)** — 다른 머신엔 안 감. same-machine 재개 전용.
> 다른 머신 인계 시 이 내용을 복사해 전달할 것. 미커밋 코드 백업 = `doc/handoffs/2026-07-09/cycle-break-uncommitted.patch`.

## TL;DR + 다음 액션

`_MyApp_`(탑다운 슈터)의 **씬 조립 빌더 일원화 + 모듈 사이클 절단** 작업. 3단계로 진화:
1. **PCB 장식 모델** StageBuilder→WorldSceneBuilder 이관 (✅ 커밋 `0b85943`)
2. **StageBuilder 전체** WorldSceneBuilder 흡수 (✅ 커밋 `4ec8804`)
3. **Bootstrap↔Stage 사이클 절단** (🟡 **미커밋** — 빌드/골든/ctest 전부 GREEN, 커밋만 대기)

**다음 액션 = 아래 "loose end 1~3" 3개 결정 → cycle-break 커밋(path-scoped, `InitTaskId.h` 제외) → 브랜치 마무리(merge/PR) 또는 대기 중인 ScreenPipeline+Pass 브레인스토밍으로.**

---

## State of the world (2026-07-09 실측)

- **브랜치**: `refactor/pcb-to-worldscene` (main branch = `game/main`)
- **최근 커밋** (`git log --oneline`):
  | SHA | 내용 | 검증 |
  |---|---|---|
  | `4ec8804` | StageBuilder 전체 → WorldSceneBuilder 흡수 | 골든 14/14 bit-동일, ctest 120/120 |
  | `6f9a246` | (사용자) clangd 재적용 | — |
  | `0b85943` | PCB 장식 모델 이관 (물리/렌더 경계) | 골든 14/14 bit-동일 |
  | `9cffba9` | (이전) 골든 이미지 확장 = 골든 REF 기반 | — |

- **미커밋 변경** (`git status --short`) — **cycle-break 작업 + 사용자 병렬 편집이 섞임**:
  ```
   M apps/_MyApp_/main.cpp                              ← cycle-break (내 수정)
   M apps/_MyApp_/src/Bootstrap/Constants.h             ← cycle-break (사용자: ARENA/WALL 추가)
   M apps/_MyApp_/src/Bootstrap/InitTaskId.h            ← ⚠ 사용자 병렬(ScreenPipeline/Pass) — 별개, 커밋 제외
   M apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp   ← cycle-break (인라인 wall_factory + BuildStage)
   M apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.h     ← cycle-break (StageActor 반환 필드 + b2World deps)
   D apps/_MyApp_/src/Stage/Components/MaterialTimeComponent.h  ← cycle-break (컴포넌트 삭제)
   M apps/_MyApp_/src/Stage/Constants.h                 ← cycle-break (ARENA/WALL 제거)
   D apps/_MyApp_/src/Stage/Factories/pickup_factory.h  ← cycle-break (Bootstrap/로 이동)
   D apps/_MyApp_/src/Stage/Factories/wall_factory.h    ← cycle-break (WorldSceneBuilder.cpp 로 인라인)
  ?? apps/_MyApp_/src/Bootstrap/pickup_factory.h        ← cycle-break (신규, 이동됨) — 🟡 loose end 1
  ```
- **현재 검증 상태 (실측)**: 빌드 `EXIT=0` GREEN · 골든 14/14 bit-동일 · ctest 120/120 pass.

---

## 왜 이렇게 됐나 (배경 — 제로컨텍스트 대비)

원래 PCB 장식만 옮기는 작은 plan이었으나, 논의 중 사용자가 **"물리 결합 vs 렌더-only"라는 경계 기준을 기각**했다.
근거(사용자): *"SceneGraph의 Actor를 초기화하고 Root에 붙인다는 점에서 WorldCamera/Light/Skybox/PCB/벽/StageState가
전부 동일 관심사다. 물리냐 비주얼이냐로 나누는 건 이상하다."* → StageBuilder 전체를 WorldSceneBuilder로 흡수 결정.

흡수 후 **모듈 양방향 의존**이 드러남:
- `Bootstrap(WorldSceneBuilder) → Stage` (흡수로 신규: Stage 컴포넌트/팩토리 include)
- `Stage(WaveController) → Bootstrap::BuildEnemy` (기존, 자연스러운 팩토리 의존)

링크 그래프는 header-only 덕에 acyclic이었지만 소스-include는 양방향. 사용자 진단:
*"팩토리가 잘못된 모듈에 있었다"* → 아래 방식으로 **Bootstrap→Stage를 완전 절단**.

---

## Locked decisions (재론 금지)

1. **StageBuilder는 WorldSceneBuilder에 완전 흡수** (별도 모듈/파일 아님). 근거 = 씬그래프 Actor 조립 일원화.
2. **Bootstrap→Stage 사이클 절단 방식** (사용자 확정, 이번 세션 구현):
   - `wall_factory.h` → **WorldSceneBuilder.cpp 익명 namespace에 인라인** (`CreateWallActor`). Stage/Factories 삭제.
   - `ARENA_HALF_EXTENT` / `WALL_THICKNESS` → **`Bootstrap/Constants.h`로 이동** (Stage/Constants.h에서 제거).
     → 모든 참조는 이제 `Bootstrap::ARENA_HALF_EXTENT`.
   - `BuildStage`는 **StageActor(`SJH::Scene::Actor*`)를 반환** (Root AddChild는 내부에서, 포인터만 반환).
   - **StageState 부착은 main.cpp로 이전** (main은 이미 Stage 의존이라 정당). `MaterialTime`은 **삭제**.
3. **golden bit-identical = 무회귀 계약**. 위 전 단계가 14/14 bit-동일 유지 (Title 캡처 + Director freeze라
   MaterialTime tick 안 됨 → 삭제해도 골든 불변).

### 정정된 사실 (이전 대화의 오라벨)
- "WaveController→EnemyBuilder가 냄새"는 **틀린 프레이밍**. 그건 자연스러운 팩토리 의존(EnemyBuilder는
  PlayerBuilder의 형제, Bootstrap 빌더 패밀리). 진짜 신규 결합은 흡수가 만든 `Bootstrap→Stage`였고, 그건 절단됨.

---

## 🟡 남은 loose end (다음 세션 = 여기부터. 전부 빌드/골든 GREEN, 결정만 필요)

### 1. `Bootstrap/pickup_factory.h` — dormant 사이클 씨앗 (우선순위 中)
- 사용자가 `Stage/Factories/pickup_factory.h` → `Bootstrap/pickup_factory.h`로 이동(신규 untracked).
- **문제**: 이 파일이 여전히 `#include "Stage/Components/PickupTriggerLogger.h"` + 네임스페이스/가드가
  `TopdownShooter::Stage::Factories` / `__TOPDOWNSHOOTER_STAGE_FACTORIES_...` 그대로.
  즉 Bootstrap/에 살면서 Stage를 include → **Bootstrap→Stage 씨앗**.
- **지금은 무해**: 실측 결과 **아무 TU도 이 파일을 include 안 함(dormant)** → 활성 사이클 아님. 빌드 GREEN.
- **결정 필요**: (a) pickup_factory를 Stage로 되돌리기, 또는 (b) `PickupTriggerLogger`(Stage 컴포넌트)도 함께
  중립 이동, 또는 (c) dormant로 두고 "include 금지" 주석. 근본 = pickup 팩토리가 Stage 컴포넌트에 의존하는 한
  Bootstrap 거주는 latent 사이클.

### 2. `MaterialTime` 삭제 — CombatPlay 벽 스크롤 소실 (우선순위 中)
- `Stage/Components/MaterialTimeComponent.h` **삭제됨**(코드 참조 0, 빌드 GREEN).
- **효과**: 벽 PoliceTape의 `uTime` 스크롤 애니메이션이 **CombatPlay에서 안 흐름**.
- **골든 무영향**: 골든은 Title 캡처 + Title에선 Director freeze(D5)라 uTime이 어차피 0 → bit-동일 유지.
- **결정 필요**: 전투 중 벽 스크롤을 되살릴지. 원하면 main에서
  `mStage->AddComponent<...MaterialTime>(wallMat)` 대신 **다른 방식** 필요(컴포넌트가 삭제됐으므로 재도입 or
  render loop에서 직접 `stage_wall` 머티리얼 uTime 갱신). 의도된 드롭이면 그대로.

### 3. `StageConfig.h` — 깨진 고아 (우선순위 中)
- `apps/_MyApp_/src/Stage/StageConfig.h` = 삭제된 `CreateStageActor`의 입력 PoD.
- 이제 제거된 `ARENA_HALF_EXTENT`/`WALL_THICKNESS`(Stage에서)를 참조 → **컴파일하면 깨짐**.
- **무해**: 아무도 include 안 해서 미컴파일. 하지만 죽은+깨진 코드 → **삭제 후보**.

### 4. stale 주석 (우선순위 低)
- `Stage/Constants.h:8` (파일 doc) + `Entity/Constants.h:55` (`sqrt(2) x Stage::ARENA_HALF_EXTENT` 주석) 이
  아직 옮겨간 ARENA/WALL을 Stage 소속으로 언급. 코드 무영향.

---

## 이번 세션에서 고친 컴파일 에러 (참고 — 사용자 병렬 편집이 남긴 것들)

| 파일 | 수정 | 왜 |
|---|---|---|
| WorldSceneBuilder.cpp | `#include "Bootstrap/Constants.h"` 추가 | ARENA/WALL 이동처 |
| WorldSceneBuilder.cpp | `return dir.Root().AddChild(std::move(stage));` | ⚠ **버그였음**: `AddChild(std::move(stage)); return stage.get();`은 moved-from → **nullptr 반환** → main `mStage->AddComponent`가 널 역참조 크래시 |
| main.cpp | `#include` 2종 (StageStateComponent.h, Bootstrap/Constants.h) | StageState 부착 + ARENA |
| main.cpp | `AddComponent<stageState>` → `<Stage::Components::StageState>` | 변수명이 타입 자리 |
| main.cpp | `Stage::ARENA_HALF_EXTENT` → `Bootstrap::ARENA_HALF_EXTENT` | 상수 이동 |

---

## Parallel-track conflict matrix (⚠ 필독 — 다중 편집자)

| 파일/영역 | 소유/편집자 | 다음 세션 규칙 |
|---|---|---|
| `Bootstrap/WorldSceneBuilder.{cpp,h}` | **이 작업 + 사용자 실시간 편집** | cycle-break 소관. 편집 전 `git diff`로 현재형 재확인(사용자가 계속 손댐) |
| `main.cpp` (World task 218~241) | 이 작업 | cycle-break 소관 |
| `Bootstrap/Constants.h`, `Stage/Constants.h`, `Bootstrap/pickup_factory.h` | 이 작업 (cycle-break) | 소관 |
| **`Bootstrap/InitTaskId.h`** | **⚠ 사용자 병렬 = ScreenPipeline/Pass 작업** | **cycle-break 커밋에서 제외**. 절대 이 파일을 cycle-break에 add하지 말 것 |
| `.clangd` | 사용자 | 미접촉 |

---

## Guardrails & 컨벤션 (프로젝트)

- **커밋 path-scoped**: `git add <명시경로>` 만. **`git add -A` 절대 금지** (사용자 병렬 staging 휩쓺).
  cycle-break 커밋 대상 = 위 매트릭스의 cycle-break 파일들, **`InitTaskId.h` 제외**, `.clangd` 제외.
- **Co-Authored-By 미사용**. 커밋 메시지 한국어, 형식 `[refactor] : ...`.
- **주석 한국어**, 특수문자 자제(Doxygen+ASCII 컨벤션).
- **빌드**: `cmake --build --preset ninja --target _MyApp_`  (sb7 `gl.h and gl3.h` 경고 1개는 정상).
- **골든 무회귀**: `cd build_ninja/apps/_MyApp_ && SJH_GOLDEN_CAPTURE=1 ./_MyApp_` 후
  `test/golden/*.png` 14장 vs `build_ninja/apps/_MyApp_/test/golden/` `cmp -s` bit-동일 확인.
- **ctest**: `ctest --test-dir build_ninja --output-on-failure` → 120/120.
- **clangd IDE의 missing/unused-include 진단은 알려진 거짓 양성** (Strict .clangd 정책). 빌드/ctest가 진실.
- **커밋은 사용자 게이트일 수 있음** — 미커밋 상태로 두고 사용자에게 커밋 승인/방식 확인 권장.

---

## Pointers

- **Plan (진화 원본, gitignored 로컬)**: `doc/superpowers/plans/2026-07-08-pcb-decoration-to-worldscenebuilder.md`
  — PCB 이관 Task1~4만 담긴 원본. 상단에 이 핸드오프로의 진화 배너 추가됨.
- **미커밋 무손실 백업**: `doc/handoffs/2026-07-09/cycle-break-uncommitted.patch`
  (git reset 사고 시 `git apply` 로 복구. ⚠ 사용자 병렬 편집분도 일부 포함될 수 있으니 apply 전 diff 검토.)
- **대기 중 별개 작업**: **ScreenPipeline(T2) + Pass(T4) init-task 완전 병합 브레인스토밍**.
  사용자가 원한 것 = 두 task 병합. 제약 = 그대로 병합 시 `ScreenPipeline→World→VfxUi→ScreenPipeline`
  **의존 사이클**(InitScheduler abort). 사이클 회피 위해 간선 재설계 필요 → brainstorming 미착수.
  `InitTaskId.h` 미커밋 편집이 이 작업의 씨앗.

---

## Change log
- 2026-07-09: 최초 작성. PCB 이관(`0b85943`)+StageBuilder 흡수(`4ec8804`) 커밋 완료, cycle-break 미커밋
  (빌드/골든/ctest GREEN). loose end 1~4 + 대기 브레인스토밍 기록.
