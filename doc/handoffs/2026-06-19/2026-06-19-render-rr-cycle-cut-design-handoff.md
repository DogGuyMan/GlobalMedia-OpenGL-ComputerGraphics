# Handoff — `render→rr→sprite→render` 3-사이클 절단 설계 재개 (2026-06-19)

> ⚠ 시점 문서 (archival) — 코드 경로는 작성 당시 기준. 소멸/이동 경로는 `<세그먼트>` placeholder 표기.

> 🔴 **확장됨 (2026-06-19) → [2026-06-19-init-scheduler-architecture-resume-handoff.md](2026-06-19-init-scheduler-architecture-resume-handoff.md)** — 이 절단은 **InitScheduler T2 init-아키텍처**로 진화/흡수되었습니다(지점① push 확정 / 지점② A=`src/render_bootstrap/` 확정 / Section 1 합의). 본 문서의 *3-사이클 분석·10 mutual 완료 기록은 여전히 유효 reference*이나, "다음 단계"는 신규 핸드오프가 정본 진입점.

> **이 문서의 목적.** 의존 사이클 리팩토링(엔진 ①②③ + 클라 ④)으로 **10 mutual 사이클 → 0** 을 달성한 직후,
> 전수 검출에서 드러난 **사전 존재 3-사이클 `render → rr → sprite → render`** 을 절단해 *완전 DAG* 로 만드는
> 작업을 재개하기 위한 self-handoff. `/compact` 직후 같은 세션이 (1) Graphviz 의존 방향 시각화 → (2) 절단 설계
> brainstorm 을 무손실로 이어가도록 한다. 받는 사람은 이 문서 외 아무것도 안 읽어도 시작할 수 있어야 한다.

---

## 0. TL;DR + 다음 행동

- **어디까지 됐나:** 의존 사이클 리팩토링 **전부 커밋 완료** (10 mutual → 0). working tree **clean**.
- **무엇을 발견했나:** 빌드·기능 무해하지만 **사전 존재 3-사이클 1건**이 남음 — `render → rr → sprite → render`.
  세 엣지 중 **부자연스러운 엣지 = `render → rr`** (중간층 render 가 최상위 캐시 파사드 rr 에 역의존).
- **사용자 결정 (2026-06-19):** "**설계 리런(brainstorm)부터**" — 즉 즉흥 구현 금지, 설계부터.
- **사용자가 추가 지시한 순서:**
  1. (이 문서) 3-사이클 분석 handoff 작성 → `doc/handoffs/`
  2. `/compact`
  3. **절단 설계 전에 먼저 Graphviz 로 의존 방향을 그림으로 확인** + **나쁜 엣지(`render→rr`)에 빨간 화살표**
  4. 그 다음 `render↛rr` 절단 **brainstorm 설계** (superpowers:brainstorming HARD-GATE — 설계 승인 전 구현 금지)
- **바로 다음 한 걸음:** `/compact` 후 → **Graphviz 의존 그래프 생성** (전 18 모듈 include 엣지, `render→rr` 빨강).
  그 다음 절단 brainstorm.

---

## 1. 상태 — 재측정 (2026-06-19)

- 브랜치: `game/main`. working tree **clean** (`git status` = extern 서브모듈 외 0).
- **커밋 체인 (의존 사이클 리팩토링 전체):**

| 커밋 | 내용 |
|---|---|
| `f98fc81` | 클라 Task 1 (C4) — Entity 시임 → `Contracts/`, 인터페이스 의존 역전 |
| `414cd9d` | 클라 Task 2~4 (C1/C2/C3) — Carrier→ILivable / PhysicsImpulse→ITimerOwner / PlayerController→4 인터페이스 |
| `385c932` | 엔진 Slice 1 (E1+E4) — Light→`scene/light`, RenderTarget→`buffer/render_target`, D6 광원 setter→dispatcher |
| `7029625` | 엔진 Slice 2 (E2) — Skybox/ScreenCamera→`render/actor_factory`, `model_spawner`→render |
| `cadf80b` | 엔진 Slice 3 (E3/E5/E6) — `src/texture/` 18모듈 신설, rr 캐시 파사드화, **D8 SpriteRenderer DI** |

- **10 mutual → 0 달성 증명 (커밋된 상태에서 재현 가능):**
  ```
  object->scene   : 0   (E1)
  scene->render   : 0   (E2)
  buffer->render  : 0   (E4)
  buffer->rr      : 0   (E3)
  object->rr      : 0   (E5)
  sprite->rr      : 0   (E6)
  texture->rr     : 0
  program->object/light : 0   (D6)
  + 클라 C1~C4 (Physics/InputHandler/Spawns/Playable -> Entity = const-noise 외 0)
  ```
  검증 명령(예): `grep -rl '#include "resource_registry/' src/buffer src/object src/sprite` → 0.
- GUI 육안 검증 **정상** (사용자 확인 2026-06-19) — D8 DI 후 스프라이트/월드텍스트/손/적/스카이박스 정상.

---

## 2. 핵심 — 3-사이클 분석 (이 작업의 출발점, 전부 grounded)

### 2.1 사이클과 세 엣지 (file:line 재측정 2026-06-19)

`render → rr → sprite → render` (rr = `resource_registry`)

| 엣지 | 근거 (file:line) | 자연스러움 |
|---|---|---|
| **rr → sprite** | `src/resource_registry/resource_registry.h:41` `#include "sprite/uniform_atlas.h"` (rr 이 UniformAtlas 를 9종 자원 중 하나로 캐시: `mAtlas` map, `CreateUniformAtlas`/`FindUniformAtlas`) + `src/resource_registry/sprite_resources.cpp:19` (D8 로 이주한 sprite 자원 해결) | ✅ **자연** — rr 은 최상위 캐시 파사드, 모든 자원을 캐시하는 게 본업 |
| **sprite → render** | `src/sprite/sprite_component.h:23` `#include "render/mesh_renderer.h"` (`SpriteRenderer : SJH::Scene::MeshRenderer` — Unity SpriteRenderer is-a MeshRenderer 정통) | ✅ **자연** — sprite 는 render 위 상위 레이어 |
| **render → rr** | `<src>/render/scene_renderer.cpp:32` include + **`:123` `ResourceRegistry::Get().GetAllPrograms()`** (라이트 uniform 송신용 program 목록 *pull*) · `<src>/render/render_pipeline.cpp:26` include + `SetupDefaultPipeline(ResourceRegistry& reg, …)` (파이프라인 자원을 rr 에 *등록*) | ❌ **부자연** — 중간층 render 가 최상위 캐시 파사드 rr 에 역의존 |

> **판정:** `render → rr` 이 유일한 부자연 엣지. 이를 끊으면 **엔진 spec 의 target 다이어그램(`rr → render`)과 정확히 일치** —
> rr 이 깨끗한 최상위 파사드가 되고 그래프가 완전 DAG 가 된다.

### 2.2 사전 존재 확인 (이건 본 리팩토링의 회귀가 아니다)

세 엣지 모두 Slice 3 *이전부터* 존재했다 (render 가 늘 rr 캐시를 pull, rr.h 가 늘 uniform_atlas 캐시, SpriteRenderer 가 늘 MeshRenderer 상속).
spec 은 "3-cycle 은 2-cycle 절단으로 동반 소멸"이라 가정했으나, 이 3-사이클은 절단된 mutual 엣지(`sprite→rr`)가 아니라
`sprite→render` 를 경유하므로 **살아남았다**. 메모리 `dependency_cycles_survey.md` 의 *"ModuleDeps mutual 3개 과소집계"* 와 일치.

### 2.3 왜 `render → rr` 이 부자연인가 (레이어링 직관)

leaf→top 레이어링: `texture ← buffer ← object ← scene ← render ← resource_registry(최상위 캐시 파사드)`.
- sprite 는 render 보다 위 (SpriteRenderer is-a MeshRenderer) → `sprite → render` 정상.
- rr 은 모든 것 위 (모든 자원 캐시) → `rr → sprite` 정상.
- 그런데 render 가 rr(최상위)을 pull → `render → rr` = 레이어 역행. **이게 끊어야 할 엣지.**

---

## 3. 절단 후보 + 비용 (설계 brainstorm 의 입력 — 아직 미결정)

> 목표: render 가 `resource_registry.h` 를 include 하지 않게 (모듈 의존 절단). 한 엣지만 끊으면 DAG.

**채택 방향(부자연 엣지) = `render ↛ rr`.** 두 지점을 처리해야 함:

### 지점 ① `scene_renderer.cpp:123` `GetAllPrograms()` pull
- 현재: SceneRenderer 가 rr 에서 전 program 목록을 *직접 pull* 해 LightUniformDispatcher 로 넘김.
- 절단안: program 목록을 **주입(inject)** — 호출자(rr 를 아는 상위/클라)가 `vector<Program*>` 를 SceneRenderer/dispatcher 에 전달.
- 규모: *중간* (SceneRenderer 의 라이트 송신 경로 시그니처 + 호출처 변경). LightUniformDispatcher::Dispatch 는 *이미* `vector<Program*>` 를 인자로 받음(`<render>/light_uniform_dispatcher.cpp:32`) — SceneRenderer 내부에서 그걸 rr 에서 채우는 부분만 바깥으로 밀면 됨.

### 지점 ② `render_pipeline.cpp` `SetupDefaultPipeline(ResourceRegistry& reg)` / `BuildPostFXChain(...)`
- 현재: render 모듈 안의 *파이프라인 부트스트랩* 자유 함수가 `reg.CreateProgram`/`RegisterMesh` 등으로 rr 에 자원을 등록. `reg` 를 인자로 받지만 `resource_registry.h` 를 include(완전 타입 필요).
- 절단안: **render 밖(상위 부트스트랩 층)으로 이주** — 이게 진짜 설계 질문(거처 결정). 시그니처: `<src>/render/render_pipeline.h:76` `SetupDefaultPipeline(...)`, `:124` `BuildPostFXChain(...)`.
- 규모: *불확실* — 거처 후보(클라 `Bootstrap/` / 신규 `render_bootstrap` 모듈 / rr 쪽으로) 를 brainstorm 에서 결정.

**대안(비추천):** `rr↛sprite`(rr 의 UniformAtlas 캐시 9종 자원 설계를 깨뜨림 + D8 sprite_resources 를 방금 rr 에 둠) / `sprite↛render`(SpriteRenderer is-a MeshRenderer 정통 파괴). 둘 다 부자연.

---

## 4. 다음 단계 (받는 사람이 순서대로 수행)

1. **Graphviz 의존 방향 시각화** (사용자 지시):
   - 전 18 모듈(`src/<module>/`)의 *모듈 간 include 엣지*를 노드/화살표로. 디노이즈 규칙(과거 그래프 작업과 동일): static 클래스 접근 / namespace free function / Constant·Constexpr·매크로는 의존으로 그리지 말 것(=실인스턴스 의존만).
   - **나쁜 엣지 `render → rr` 을 빨간 화살표**로 강조. (나머지는 검정/회색.)
   - 산출물 위치 후보: `doxygen/pages/` (기존 ModuleDeps 그래프가 `doxygen/pages/00-mainpage.md`) 또는 신규 `.dot`/png. 사용자에게 확인.
   - ⚠ 18모듈×다중 include 라 정확도 위해 **모듈별 include 스캔을 fan-out** 하면 좋음(워크플로 적합). 단 *디노이즈*는 사람/판단 필요 — static-only 헤더 include 를 의존으로 오집계 말 것.
2. **절단 brainstorm** (superpowers:brainstorming): 지점 ①②의 처리 + 특히 ②`render_pipeline` 거처를 옵션표+추천으로.
   설계 승인 전 구현 금지(HARD-GATE). 승인 후 spec → plan → 구현.

---

## 5. 가드레일 + 컨벤션 (반드시 준수)

- **커밋: 사용자 게이트.** 자동 커밋 금지. 사용자가 직접 커밋함(2026-06-19 명시: "커밋은 자동으로 하지 말고"). 커밋 필요 시 path-scoped `git commit <경로>` 명령을 *제안만* 하고 대기. `git add -A` 금지(사용자 병렬 staging 휩쓺). **`Co-Authored-By` 트레일러 미사용**.
- **Task(슬라이스)마다 끝에 질의** (2026-06-19 명시: "Task 끝날때마다 질의해"). AskUserQuestion 으로 다음 단계 확인.
- **사용자 병렬 git 작업** — 받는 사람과 사용자가 같은 working tree에서 병렬. 각 단계 직전 `git status`/`git log` 재측정 필수(커밋 SHA가 위 표와 달라질 수 있음).
- **빌드/검증 명령:** `cmake --preset ninja` → `cmake --build --preset ninja --target _MyApp_` (에러 0). 실행/GUI 는 사용자(`cd build_ninja/apps/_MyApp_ && ./_MyApp_`). ⚠ `-DENABLE_TESTING=ON` configure 는 **사전 breakage**(root `CMakeLists.txt:44` `add_subdirectory(test_smoke)` — 디렉토리 부재)로 막힘. 내 변경과 무관. test 타겟 검증은 그 해소 후.
- **코드 컨벤션:** 주석 한국어 + ASCII/한글만(특수문자 0, 화살표 `->` 만, 유니코드 → 금지). 헤더 가드 `__SJH_<MODULE>_<NAME>_H__`(`#pragma once` 금지, `#endif` 주석 정확히 일치). Tab indent. `long` 금지→고정폭. 멤버 `m`PascalCase / 지역 camelCase / bool `mIs*` / 포인터 `*Ptr`. (전역 Skill `personal-naming-conventions`)
- **불가침:** `extern/sb7code` 수정 금지. stb `STB_IMAGE_IMPLEMENTATION` 단일 owner = `src/texture/image.cpp`(이주됨). FMOD `SJH_HAS_FMOD` 가드 보존, rr `game_deps` PUBLIC 불변. no_auto_tests(요청 시만).
- **모듈 빌드 규율** (Skill `modular-build-discipline`): 헤더 노출 의존=PUBLIC / .cpp 전용=PRIVATE, 결정 *왜*를 CMake 주석에. self-contained.

---

## 6. 로드할 설계 스킬·문서 (사용자가 이 작업용으로 지정)

- 전역 Skill: `architecture-design-workflow`(4-Phase+Decision Log+옵션표/추천) · `design-decision-discipline`(변동성≠다형성/composition>inheritance/소유권) · `code-design-review-lenses`(5렌즈+객관/주관 분리) · `modular-build-discipline` · `confidence-and-sourcing` · `response-quality-calibration` · `personal-naming-conventions` · `agent-orchestration-anti-gaming` · `socratic-tutor` · `improve-codebase-architecture`.
- 프로젝트 문서: `.claude/architecture.md`(§11.1 SJH::Uniforms family / §11.2 dangling 불변식 / §11.3 자원 보유 컨벤션) · `.claude/architecture-design-agent.md`(§12 13가지 암묵적 합의: Pattern Y/Cocos 정통명명/SSOT 등) · `doc/design/EngineDesign.md`(🛑 역사적 스냅샷, 구 `src/engine/*` 경로 — 직교, 추가 발견 없음).

## 7. 포인터 (선택적 depth — 시작에 불필요)

- 마스터 로드맵(단일 인덱스): `doc/superpowers/plans/2026-06-11-dependency-cycle-master-roadmap.md` (①②③④ + Slice 1~3 완료 + 3-사이클 발견 기록).
- 엔진 plan: `doc/superpowers/plans/2026-06-11-engine-cycle-e1-e6-plan.md` (슬라이스 1~3 ✅ + §슬라이스3 끝의 3-사이클 발견 노트).
- 엔진 spec(정본 D1~D8): `doc/superpowers/specs/2026-06-11-engine-dependency-cycle-refactor-design.md` (§0 target 다이어그램 `rr → render` = 이 절단의 목표 상태).
- 클라 plan: `doc/superpowers/plans/2026-06-11-client-cycle-c1-c4-dip-plan.md`.
- 사이클 전수조사 근거: `doc/handoffs/2026-06-11/2026-06-11-dependency-cycle-refactor-handoff.md`.

## 8. 이 문서가 대체하는 것

- 신규 작업(3-사이클 절단)이라 기존 handoff 를 supersede 하지 않음. 본 문서가 **3-사이클 절단의 단일 진입점**.

## Change log
- 2026-06-19 — 최초 작성. 10 mutual→0 완료(5커밋 `f98fc81`~`cadf80b`) 직후. 3-사이클 `render→rr→sprite→render` 발견·분석(부자연 엣지=`render→rr`) + 절단 후보(지점①SceneRenderer program 주입 / 지점②render_pipeline 거처 이주) 기록. 다음 = Graphviz 시각화(render→rr 빨강) → 절단 brainstorm.
