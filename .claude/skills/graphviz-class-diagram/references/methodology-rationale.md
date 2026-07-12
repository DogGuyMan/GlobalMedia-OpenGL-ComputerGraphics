# Graphviz 클래스 다이어그램 작성 방법론 보고서
### (2026-06-24, `2026-06-24-render-main-class.dot` 사례 + 동료 8종 적대적 교차검증)

> 본 보고서는 향후 **skill** 로 고착화할 방법론의 원천이다. 작성 방식: 비교 매트릭스(8 차원) + **적대적 반증** + 방법론 증류를 병렬 분석한 뒤, 검증 가능한 속성(rankdir/splines/constraint/legend/red-edge)을 원본 `.dot` 에서 직접 대조해 판정했다.

---

## 0. 핵심 철학 (사용자 확정 — 최우선 가중치)

이 양식의 **비협상 핵심 원칙 6가지**. 사용자가 가치를 두거나 *명시 채택* 한 것으로, 본 방법론의 *정체성*이다. 어떤 보조 규율도 이 6가지를 해치면 안 된다.

| 우선 | 원칙 | 무엇 | 상태 | 상세 |
|---|---|---|---|---|
| **P1** | **모듈별 Box 계층 묶기** | `subgraph cluster` 로 소스 디렉토리/레이어를 시각적 계층으로 분리 | 핵심 가치 | §2-4 |
| **P2** | **의존 간선의 명확한 표현** | 상속/실현/합성/집약/의존 5종을 화살표·색·라벨로 구분 — "무엇이 무엇에 *어떻게* 의존" 을 한눈에 | 핵심 가치 | §2-3 |
| **P3** | **멤버 + 멤버함수 포함 (UML 3구획)** | 클래스가 *대강 무슨 일을 하는지* 노드만 봐도 읽힘 | 핵심 가치 | §2-2 |
| **P4** | **배치 의미축 (`rankdir=BT`)** | **최다 피참조 = 최상단 / 최다 의존 = 최하단** | ★ **강력 요구 (강제)** | §2-5 |
| **P5** | **직선 라우팅 (`splines=line`)** | 곡선(spline) 뭉침 제거 → 간선 명료성 확보 | ✅ **확정 채택** | §2-5 |
| **P6** | **아키텍처 경계 시각 게이트** | "허용된 누수 vs 위반" 을 빨강-굵음 간선으로 마킹 | ✅ **확정 채택** | §2-3 |

→ P1~P3 은 사용자가 *원래 가치를 둔* 정체성, P4 는 *강력 요구*, P5·P6 은 *검토 후 확정 채택*. 6가지 모두 동급 핵심.

### 필수 전제 — A1: `constraint=true/false` 분리 (P4 작동 조건)

**P4(BT, 강력 요구) 는 A1 없이는 작동하지 않는다.** BT + P1(cluster) + P2(많은 의존 간선)을 constraint 규율 없이 그리면 *모든* 간선이 레이아웃을 잡아당겨 BT 의미축이 붕괴하고 cluster 가 흩어진다(대상 PNG sprawl 의 근본 원인).

- **규칙**: 구조 간선(상속/합성/집약) = `constraint=true`(레이아웃 골격), 의존 간선 = `constraint=false`(cross-cut, 비간섭).
- **출처**: `refrender` 단독(12회). → **P4 를 원하는 한 A1 은 선택이 아니라 강제 전제.** (§2-5)

(차순위 보조: 스코프 출처/제외 사유 헤더 주석 — 추적성엔 좋으나 위만큼 필수는 아님.)

> ⚠️ **정직성 + 검증 정정**: "대상이 동료보다 *전면* 우월" 은 거짓(§1 매트릭스). 대상은 **P1·P2·P3 + 렌더된 범례** 에서 우월하나 **배치규율(P4 미적용·A1·P5)·경계마킹(P6)** 에서 열위였다 → 본 양식이 그 격차를 흡수. 또한 1차 분석의 "engine-migration 도 constraint 분리" 주장은 **날조**(engine constraint = 0개) — `constraint` 분리는 **`refrender` 단독**.

---

## 1. 비교 매트릭스 요약 (CLASS 다이어그램 한정)

비교 대상: 대상 + `engine-migration-class` / `refrender-class-dependency` / `render-class-dependency` / `render-migration-plan-v5` / `2026-06-20-render-class-deps` / `physics-class-deps` / `playable-class` / `stat-class-deps`. (sequence/state/tree/module 다이어그램은 비교 제외.)

| 차원 | 대상 | refrender | engine-migration | 나머지 |
|---|---|---|---|---|
| (1) 노드 상세(UML 3구획) | ✅ 완전 | ✅ 완전(멤버 충실도 약간 ↑) | ✅ 완전 | record/box (축약) |
| (2) 패키지 클러스터링 | ✅ **6 cluster(최다)** | ✗ (의도적 비클러스터) | △ future 1개만 | render-class-deps 4, 2026-06-20 3 |
| (3) 레이아웃 규율 | ✗ TB+spline+constraint 0 | ✅ **BT+line+constraint 12** | ✅ BT+line(constraint 0) | 대부분 TB 기본 |
| (4) 간선 의미 5종 | ✅ 5종 | ✅ 5종+격리마킹 | ✅ 6종+★gl마킹 | 2~3종 |
| (5) 범례 | ✅ **렌더된 예시 간선(유일)** | 텍스트 표 | 텍스트 표 | 대부분 없음/주석 |
| (6) 방향/사이클 명시 | ✅ **note 단언(유일)** | 헤더 주석 배치규칙 | 배치 암시 | 없음 |
| (7) 색 의미부여 | ✅ cluster+노드 | ✅ +경계 빨강 | ✅ +★빨강 | 다양 |
| (8) 스코프 출처/제외 | ✗ | ✅ (출처+out-of-scope) | △ | physics 명시 |

---

## 2. 방법론 (skill 본문 — 사용자 4질문 + 레이아웃/범례/주석)

### 2-1. 사전 절차 프로세스 (그리기 전 grounding)

**왜:** 다이어그램은 "무엇을 *안* 그릴지"의 예술이다. 사전 grounding 없이는 노이즈/핵심을 못 가른다.

> ★ **의존 정보의 3층 출처** (grounding 핵심 — 한 곳만 읽으면 간선이 빈다):
> - **헤더 `.h`** = *멤버 간선*(합성/집약). `unique_ptr`/`T*`/`vector<T*>` 멤버.
> - **소비처 `.cpp`** = *사용 간선*(의존) + *노드 리터럴*. `creates/Add`·`Get()/Execute` dependency 와 `GetPassKey()="World"` 같은 반환 리터럴은 생성자 호출부·메서드 바디에만 산다 — 헤더엔 안 보임.
> - **빌드파일 `CMakeLists.txt`(`target_link_libraries`)** = *모듈 의존 방향*의 권위 출처. 헤더 forward-decl 은 방향을 속이지만 링크 그래프는 못 속인다.

- [ ] **목표·범위 1줄 확정** — "src/render ↔ main.cpp 클래스 의존" 처럼 *대상 + 경계*. 헤더 주석에 박는다.
- [ ] **출력 디렉토리 정찰** — 산출 dir(`doc/diagrams`) ls: ① 날짜 네이밍 컨벤션(`YYYY-MM-DD-`) ② 교차검증·양식 정합용 동료 다이어그램 확보.
- [ ] **핵심 클래스 헤더 read** — include guard + 멤버 변수 + public 메서드 + 상속(`class X : public A, B`). → *멤버 간선*.
- [ ] **소비처 `.cpp` 본문 read** — main.cpp/impls.cpp 의 생성자 호출·메서드 바디. → 헤더에 없는 *의존 간선*(creates/uses) + 노드 리터럴. (트레이스: main.cpp 543줄 + render_passable.impls.cpp 통독.)
- [ ] **멤버 타입으로 소유권 판정** (★다이어그램 정확도의 핵심):
  - `unique_ptr<T>` / by-value 멤버 → **합성(composition)**
  - `T*` / `T&` / `vector<T*>` (비소유) → **집약(aggregation)**
  - 메서드 인자·반환·지역에만 등장(멤버 X) → **의존(dependency)**
- [ ] **모듈/디렉토리 경계 + 방향 파악** — 소스 dir 별 cluster 후보 그룹 + **CMake `target_link_libraries` 로 모듈 의존 방향 확정**. (트레이스: `src/scene/CMakeLists.txt` 의 "scene → render 절단" 주석이 note_dir 의 "render→main 없음" 단언을 근거지음.)
- [ ] **양방향/사이클 사전조사** — `A→B` + `B→A` 검색, `A→B→C→A` 검색. 발견 시 빨강 `dir=both` 또는 레이어 재정의. (대상은 "render→main 없음 = 단방향" 을 note 로 단언했다.)
- [ ] **[흡수⑧] 스코프 출처·제외 사유 기록** — "spec/plan 경로" + "왜 이 노드는 생략/단순화" 를 헤더 주석에. (refrender 패턴)

### 2-2. 렌더 vs 생략 (추상화 레벨)

**왜:** 너무 상세=가독성 붕괴, 너무 단순=구조 불명. 노드 종류별 차등 상세도.

| 종류 | 렌더 수준 | 표기 |
|---|---|---|
| **핵심 클래스** (Pass/Processor/DeviceContext 등) | UML 3구획(이름+상속 / 멤버+소유권주석 / 메서드) | HTML `<table>` |
| **외부 forward** (Material/Mesh/Program 등 타 모듈) | 박스 + 이름만 | `shape=box` 회색 |
| **struct/enum/free-fn 유틸** | 노드 O, 멤버는 "필드1, 필드2…" 축약 | HTML 1~2행, 황/회색 |
| **static/macro/익명ns 헬퍼** | **완전 생략** | — |
| **전이(폐기 예정) 요소** | 노드 유지, 간선만 `style=dashed` + "전이" 라벨 | 점선 |

- 핵심 클래스도 trivial getter/setter 는 생략, 핵심 메서드만. 여러 메서드는 `+ A() / B() / C()` 한 줄로.
- 멤버에 **소유권 주석**(`(관찰)` / `(소유)` / `(공유)`)을 색(#2e75b6)으로 — unique_ptr↔raw 가독성. (대상 강점④)
- **[흡수④] 소유권 이중 인코딩** — 같은 사실을 *간선 화살촉(diamond/odiamond)* + *멤버텍스트 주석* 두 채널로 중복 표기. 대형 그래프에서 한 채널(간선) 추적이 끊겨도 노드만 봐서 소유관계 복원 (redundant visual channel = 강건성).

### 2-3. 의존 관계 표현 (5종 간선 — UML 표준)

각 종류를 **`edge[...]` 기본값 블록으로 묶어** 일관 적용 (대상 패턴).

| 관계 | 스타일 | 색 | 의미 |
|---|---|---|---|
| **상속(is-a)** | `arrowhead=onormal, style=solid` | 남색 #1f4e79 | `class D : public Base` |
| **실현(implements)** | `arrowhead=onormal, style=dashed` | 검정 #333 | `class C : public IFace` (인터페이스) |
| **합성(owns)** | `dir=both, arrowtail=diamond, arrowhead=vee` | 보라 #7030a0 | unique_ptr/by-value |
| **집약(refs)** | `dir=both, arrowtail=odiamond, arrowhead=vee` | 파랑 #2e75b6 | T*/T&/vector<T*> 비소유 |
| **의존(uses)** | `style=dashed, arrowhead=vee` | 회색 #777 | 메서드 인자/반환/생성 |

- 간선 라벨 = **멤버명 + 다중도**(`mPasses 0..*`) 또는 동작(`creates/Add`, `Get()/Execute`).
- **[흡수⑦] 아키텍처 경계 마킹** — "허용된 누수" 의존(예: `DeviceContext → GraphicsAPI` gl* 직접)은 **빨강 굵음(#D50000, penwidth=2.6)**. → "빨강이 다른 데 보이면 아키텍처 위반" 이라는 *시각 게이트*. (engine-migration `★ gl* 직접` + refrender `★ 유일 API 의존` 패턴.)
- **양방향**: 진짜 mutual(`A↔B`)만 빨강 `dir=both`. 없으면 note 로 "단방향/사이클 없음" 단언. (대상 강점③)

### 2-4. 패키지/모듈 그룹화 (subgraph cluster)

**왜:** 소스 디렉토리=레이어 경계를 시각화 → 의존이 계층을 거스르는지 한눈에.

```dot
subgraph cluster_render {
    label="src/render (core)"; labeljust="l"; fontsize=13;
    style="rounded"; color="#385723"; penwidth=2;
    /* 노드... */
}
```
- **색 컨벤션(고정)**: 응용(apps)=빨강 #c00000 · 엔진 코어=파랑 #1f4e79 / 초록 #385723 · 외부 forward=회색 #aaaaaa `dotted` · 범례/노트=노랑 #fff2cc.
- `labeljust="l"` + cluster `fontsize=13` > 노드 `fontsize=10` (위계).
- **`compound=true` + `lhead=cluster_X`** — cluster 로 들어가는 의존 간선을 "노드→cluster" 로 묶어 교차 감소. (2026-06-20 패턴. 대상은 `compound=true` *선언만 하고 `lhead` 미활용* = 개선점.)

### 2-5. 레이아웃 규율 ★ (대상의 핵심 약점 = 최우선 흡수)

**결정 규칙(취향 아님):**
- **의존-레이어 강조 다이어그램** → **`rankdir=BT` + `splines=line` + `constraint=true/false` 분리** 3종 세트 강제.
  - BT: "위=피참조多(foundation) / 아래=참조多(orchestrator)" 의미축. (refrender/engine)
  - `splines=line`: 직선 직교 라우팅(곡선 난잡 제거).
  - **구조 간선(상속/합성/집약) = `constraint=true`**(골격), **의존 간선 = `constraint=false`**(cross-cut 비간섭). → 의존이 많아도 레이아웃이 안 흔들림. **(refrender 단독 기법, 12회.)**
- **프로세스/제어흐름 하향 다이어그램만** `rankdir=TB` 허용.
- `ranksep`: BT 레이어 강조 시 1.7~2.55 크게, TB 컴팩트 시 0.9.

> 대상이 `TB + splines=spline + constraint 0` 3종을 택한 것이 sprawling(외부 cluster 향 간선 다발 교차)의 근본 원인. → BT/line/constraint 로 전환 시 레이아웃 대폭 개선.

### 2-6. 범례 + 헤더 주석

- **범례 = 렌더된 예시 간선**(대상 강점①, 동료 대비 우월): `cluster_legend` 안에 실제 `L_a→L_b [arrowhead=onormal,...]` 4~5종을 그려, *렌더 결과와 1:1 일치*하는 자기검증 범례. (텍스트 표보다 교육적.)
- **헤더 주석**(파일 최상단): ① 제목+날짜 ② 목표·범위 ③ 간선 5종 1줄 요약 ④ **스코프 출처(spec/plan)+제외 사유** ⑤ 렌더 명령(`dot -Tsvg X.dot -o X.svg`).
- 주석/라벨: **한국어 설명 + ASCII/영문 시그니처** (프로젝트 컨벤션).

---

## 3. 즉시 적용 4-Phase 체크리스트

```
Phase 1 (사전):   [ ] 목표/범위 1줄  [ ] 헤더 read(멤버간선)  [ ] 소비처.cpp read(사용간선+리터럴)  [ ] 소유권 분류(unique_ptr/ptr/인자)
                 [ ] 출력dir 정찰  [ ] 모듈경계+CMake방향  [ ] 사이클 검사  [ ] 스코프 출처+제외
Phase 2 (노드):   [ ] cluster(app/엔진/외부) 색 고정  [ ] 핵심=UML3구획  [ ] 외부=박스  [ ] 유틸=축약
                 [ ] 렌더된-예시 범례 cluster
Phase 3 (간선):   [ ] 5종 edge[] 블록 분리  [ ] 합성/집약 정확 구분  [ ] 라벨=멤버명+다중도
                 [ ] ★아키텍처 경계 빨강-굵음  [ ] 양방향/사이클 note
Phase 4 (배치):   [ ] 의존-레이어면 BT+line+constraint 분리 / 흐름이면 TB
                 [ ] compound=true + lhead  [ ] 헤더 주석(렌더명령)  [ ] dot -Tsvg + -Tpng
```

---

## 4. skill 전환 시 정정·보강 (검증 기반)

- **[정정]** `constraint=true/false` 분리 근거는 **refrender 단독**(라인 146/157/169=true, 178/210=false; 의도는 라인 8 주석). engine-migration 은 constraint **0개** — 인용 금지.
- **[정정]** 아키텍처 경계 빨강 마킹 근거 = engine-migration(`gl* 직접` #D50000 penwidth 2.6) **및** refrender(`★ 유일 API 의존` #cc0000 bold) — **둘 다** 유효.
- **[톤다운]** "대상이 UML 3구획 최상세" → "동급 최상위(refrender 와 대등, 멤버 충실도는 refrender 가 약간 우위)".
- **[추가]** Phase 4 에 "아키텍처 경계 간선 빨강-굵음 마킹 여부" 신설.
- **[추가]** rankdir 을 *결정 규칙*(§2-5)으로 — 대상의 TB+spline+no-constraint 3종이 핵심 약점이므로 규칙화.
- **[추가]** §2-1 에 **의존 정보 3층 출처**(헤더=멤버 / `.cpp`=사용+리터럴 / CMake=모듈방향) 규율 신설 — grounding 을 "헤더 read" 로 뭉뚱그리던 것을 분해. **skill 사전절차의 핵심.**
- **[추가]** §2-2 에 **소유권 이중 인코딩**(간선 화살촉 + 멤버텍스트), §2-1 에 **출력 디렉토리 정찰**(네이밍·동료 양식) 보강. `compound=true` 는 "선언만·`lhead` 미활용" 으로 정정.

---

## 5. 한 줄 평

본 양식의 정체성 = **6원칙**: P1 모듈 Box 계층 · P2 의존 간선 명확 · P3 멤버·멤버함수 역할가독 · **P4 피참조多=상단/의존多=하단(BT, 강력 요구)** · **P5 `splines=line` 직선 명료성(확정)** · **P6 아키텍처 경계 시각 게이트(확정)**. 단 **P4는 A1(`constraint=true/false` 분리)이 없으면 작동하지 않으므로, A1은 P4의 강제 전제**다. 즉 BT(P4)·직선(P5)·게이트(P6)는 사용자 확정, constraint(A1)은 P4를 떠받치는 필수 골격 — 이 셋이 P1~P3의 시각 구조를 *흩어지지 않게* 실현한다.
