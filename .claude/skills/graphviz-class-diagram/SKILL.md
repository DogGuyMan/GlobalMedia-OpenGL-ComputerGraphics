---
name: graphviz-class-diagram
description: Create publication-quality UML-style class & dependency diagrams of a codebase in Graphviz (`.dot` → SVG/PNG). Use this whenever the user wants to visualize code structure or dependencies — "draw a class diagram", "diagram how module X depends on Y", "show the dependency graph", "Graphviz/dot diagram of these classes", "visualize this architecture", "map the relationships between these files", "before/after change pair for specs" — even if they don't say "Graphviz" or "UML". Also reach for it during review/refactoring when a structural picture would expose coupling, layering, or cycles. Encodes a grounding-first method (read headers + .cpp bodies + build files), 5 distinguished UML edge kinds, module-box clustering, and a bottom-up dependency-layer layout.
---

# Graphviz Class / Dependency Diagram

코드의 UML 스타일 클래스/의존성 다이어그램 — 모듈 하나 ↔ 그 소비자, 서브시스템, 또는 레이어드 아키텍처 — 을 Graphviz `.dot` 로 그려 SVG + PNG 로 렌더링하는 작업 방법론.

좋은 다이어그램은 **구조에 관한 하나의 논증**이지, 모든 클래스를 쏟아붓는 덤프가 아니다. 그 가치는 무엇을 *생략하는지*, 그리고 *누가 누구에게 어떻게 의존하는지*를 얼마나 명확히 보여주는지에 있다. 이 방법론은 6개 원칙을 4개 Phase 에 걸쳐 적용한다.

## The six principles (the identity of this style)

| P | 원칙 | 의미 |
|---|---|---|
| **P1** | **모듈박스 클러스터링** | 소스 디렉토리/레이어별 `subgraph cluster` — 레이어를 가로지르는 의존성이 한눈에 보임 |
| **P2** | **명시적 의존 엣지** | 5가지 관계 종류(inherit(상속) / realize(실현) / compose(합성 소유) / aggregate(집약 참조) / depend(의존))를 arrowhead + 색상 + 라벨로 구분 — "무엇이 무엇에 *어떻게* 의존하는지" |
| **P3** | **멤버 + 메서드 (UML 3분할)** | 각 핵심 노드가 필드 + 메서드를 표시해, 읽는 이가 노드 하나만 보고도 *그 클래스가 무엇을 하는지* 추론 가능 |
| **P4** | **의존성-레이어 레이아웃 (`rankdir=BT`)** | 가장 많이 의존받는(기반) 것은 위에, 가장 많이 의존하는(오케스트레이터) 것은 아래에 |
| **P5** | **직선 라우팅 (`splines=line`)** | 직교/직선 엣지 — 복잡한 그래프를 가리는 스플라인 스파게티를 제거 |
| **P6** | **아키텍처 경계 시각적 게이트** | "허용된 누출 vs 위반"을 굵은 빨강 엣지로 표시 — 그 외 어디든 빨강이 나오면 즉시 위반으로 읽힘 |

**전제조건 A1 — `constraint=true/false` 분리 (이게 P4를 작동시키는 핵심).** 이게 없으면 P4는 무너진다: BT + 클러스터 + 많은 의존 엣지가 있으면, *모든* 엣지가 랭크 배정을 잡아당겨 레이어 축이 흩어진다(클러스터가 퍼지고 엣지가 교차). 그러니 분리하라: 구조 엣지(inherit / compose / aggregate)는 `constraint=true` — 레이아웃 골격을 형성; 의존 엣지는 `constraint=false` — 레이어를 왜곡하지 않고 가로지른다. **P4를 원한다는 것은 A1이 선택이 아니라 필수라는 뜻이다.**

아래 4개 Phase는 체크리스트이기도 하다. 그라운딩이 먼저인 이유: 의존성 사실은 서로 다른 세 파일에 흩어져 있어서, 하나만 읽으면 엣지가 누락되거나 틀리게 된다.

## Phase 1 — Grounding (before any drawing)

**3계층 소스 규칙** — 가장 중요한 습관 하나:

- **헤더(`.h`)** → *멤버 엣지*(composition / aggregation). `unique_ptr<T>` 나 값 타입 멤버면 compose; `T*` / `T&` / `vector<T*>`(비소유)면 aggregate.
- **소비자 바디(`.cpp`)** → *사용 엣지*(dependency) + *노드 리터럴*. `creates` / `Get()` / `Execute()` 호출과 반환 리터럴(예: `"World"` 를 반환하는 `GetKey()`)은 생성자 호출과 메서드 바디에만 나타난다 — **헤더에는 절대 없음.** 엣지를 *발생시키는* `.cpp` 를 읽어라: 소비자 중심 다이어그램이라면 그 소비하는 `main.cpp` / `*.impls.cpp`. **라이브러리/기반 모듈(단일 소비자가 없는 경우)이라면 그 모듈 *자신*의 `.cpp` 를 읽어라 — 그것이 *자신의* 의존성의 소비자이고, 그 모듈들을 향한 사용 엣지가 거기 있다.**
- **빌드 파일(`CMakeLists.txt` 의 `target_link_libraries`, 또는 `BUILD`, `package.json` 등)** → *모듈 의존 방향*을 정본으로 알려준다. 헤더의 전방선언은 방향에 대해 거짓말할 수 있지만, 링크 그래프는 그럴 수 없다. "X는 Y에 의존하지만 Y는 절대 X에 의존하지 않는다"를 단언하는 방법이 이것이다.

Checklist:
- [ ] 목표 + 범위를 한 줄로 명시("module X ↔ consumer Y"); 파일 헤더 주석에 적어둔다.
- [ ] 출력 디렉토리(예: `doc/diagrams`)를 둘러보고 날짜 명명 컨벤션과 형제 다이어그램을 찾아 스타일을 맞추고 교차검증한다.
- [ ] 핵심 클래스 **헤더**를 읽는다 → 멤버, public 메서드, 상속(`class X : public A, B`).
- [ ] 소비자 **`.cpp` 바디**를 읽는다 → 헤더엔 안 보이는 사용 엣지 + 노드 리터럴.
- [ ] 각 멤버의 **소유권**을 분류한다(위 규칙) — compose vs aggregate vs depend를 결정하며, 다이어그램 정확도의 핵심 동인이다.
- [ ] 빌드 파일의 링크 그래프에서 모듈 경계 **와 방향**을 정한다.
- [ ] 양방향성/사이클을 스캔한다(`A→B` 와 `B→A`; `A→B→C→A`). 발견되면 → 빨강 `dir=both` 로 표시하거나 레이어를 재정의. 발견 안 되면 → 그 부재 자체가 노트 노드로 단언할 가치가 있는 결과다.
- [ ] 범위 출처 + 제외 사유(어느 spec/plan인지; 왜 노드가 생략됐는지)를 헤더 주석에 기록한다.

## Phase 2 — Nodes (render vs omit)

디테일은 역할별로 등급을 매긴다 — 너무 많으면 가독성을 죽이고, 너무 적으면 구조를 숨긴다.

| 종류 | 렌더 수준 | 표기법 |
|---|---|---|
| **핵심 클래스**(범위 내) | UML 3분할: 이름 + 스테레오타입 / 멤버 + 소유권 노트 / 메서드 | HTML `<table>` |
| **외부/전방선언**(다른 모듈) | 박스 + 이름만 | `shape=box`, gray |
| **struct / enum (데이터)** | 노드 있음, 필드는 "field1, field2…" 로 접어서 표시 | 1~2행 HTML |
| **public 자유 함수 / 네임스페이스 API**(예: private 헬퍼가 아닌 자유 함수 `ComputeX`) | `«free fn»` 스테레오타입을 붙인 노드, 시그니처만 — 실제 엣지를 발생시키므로 생략하지 말 것 | 1행 HTML |
| **static / 매크로 / 익명 네임스페이스 / private 헬퍼** | 완전히 생략 | — |
| **전이 중(제거 예정)** | 노드는 유지, 엣지를 `style=dashed` + "transitional" 로 표시 | dashed |

- 핵심 클래스에서도 사소한 getter/setter는 제외하고, 책임을 전달하는 메서드만 남긴다. 여러 개는 한 줄로 접는다: `+ A() / B() / C()`.
- **소유권을 이중 인코딩**한다: arrowhead(다이아몬드) *그리고* 색이 있는 멤버-텍스트 노트(`(owns)` / `(observes)` / `(shared)`). 큰 그래프에서는 한 채널만으로는 페이지를 가로질러 추적이 안 될 때가 많다 — 다른 채널이 이를 복구해준다.
- 순수 가상 타입에는 `«interface»` 스테레오타입을; 컬렉션에는 다중성 라벨(`0..*`)을 붙인다.
- **상속 깊이**: 범위 내 클래스의 *직계* base만 보여준다; 외부 조상은 박스로 렌더링하고 거기서 멈춘다 — 전이적으로 확장하지 않는다.
- `return *this` 를 하는 **Fluent Builder** 메서드 → 메서드 시그니처(`: Self&`)로만 표시; self-loop 엣지는 그리지 **않는다**(순전한 노이즈).
- **외부** 타입은 박스+이름만이지만, **그 엣지가 범위 내 클래스의 base를 설명해줄 때**는 `«interface»` 스테레오타입과 realize/inherit 엣지를 여전히 가질 수 있다(예: 범위 내 base 클래스가 외부 인터페이스를 realize하는 경우). 외부 박스 둘 사이의 엣지는 범위 내 노드를 명확히 해줄 때만 존재해야 한다.

## Phase 3 — Edges (5 UML kinds)

각 종류를 `edge[...]` 기본 블록 아래로 묶어서, 스타일이 일관되게 적용되고 `.dot` 이 읽기 쉽게 유지되도록 한다.

| Relationship | Style | Color | Source signal |
|---|---|---|---|
| **Inherit**(is-a 관계) | `arrowhead=onormal, style=solid` | navy `#1f4e79` | `class D : public Base` |
| **Realize**(구현) | `arrowhead=onormal, style=dashed` | black `#333` | `class C : public IFace` |
| **Compose**(소유) | `dir=both, arrowtail=diamond, arrowhead=vee` | purple `#7030a0` | `unique_ptr` / by-value member |
| **Aggregate**(참조) | `dir=both, arrowtail=odiamond, arrowhead=vee` | blue `#2e75b6` | `T*` / `T&` / `vector<T*>` non-owning |
| **Depend**(사용) | `style=dashed, arrowhead=vee` | gray `#777` | method arg / return / `creates` |

- 엣지 라벨 = 멤버 이름 + 다중성(`mPasses 0..*`) 또는 액션(`creates`, `Get()/Execute`).
- **P6 경계 게이트**: "허용된 누출" 의존성(예: `gl*` 를 직접 호출하는 `DeviceContext → GraphicsAPI`)은 **굵은 빨강**(`color="#D50000", penwidth=2.6`)으로 표시한다. 읽는 이가 내면화하는 규칙: *예상 밖 어디든 빨강이면 = 아키텍처 위반.* 신호로 남으려면 아껴서 써야 한다.
- **양방향**: 진짜 상호 `A↔B` 만 빨강 `dir=both` 를 받을 자격이 있다. 없다면 **"단일 방향 / 사이클 없음"을 단언하는 노트 노드**를 추가한다 — 사이클의 *부재*를 문서화하는 것이 읽는 이의 첫 구조적 질문에 선제적으로 답한다.

## Phase 4 — Layout (a decision rule, not taste)

- **의존성/레이어링 다이어그램** → `rankdir=BT` + `splines=line` + `constraint=true/false` 분리(이 셋은 함께 다닌다 — A1 참고). BT는 기반을 위에, 오케스트레이터를 아래에 둔다; constraint 분리가 복잡한 그래프가 흩어지지 않게 잡아준다.
- **프로세스/제어흐름 다이어그램** → `rankdir=TB` 가 맞다 — 하지만 그건 *다른 종류의 다이어그램*이지, 이것이 아니다.
- `ranksep`: 레이어를 강조할 땐 크게(1.7~2.5); 컴팩트할 땐 ~0.9.
- 클러스터로 들어가는 엣지를 묶으려면(교차 감소) `compound=true` **그리고 실제로 `lhead=cluster_X`** 를 함께 써야 한다. `lhead` 없이 `compound=true` 만 선언하면 아무 효과가 없다. **트레이드오프**: 엣지가 클러스터 내부의 *특정* 노드에 도달해야 한다면(예: "*어느* 외부 타입인지"가 핵심인 타입별 소유권 다이어그램) `lhead` 를 생략하라 — 클러스터 head로 묶어버리면 그 정보가 지워진다. 노드가 아니라 클러스터가 의미 있는 종착점일 때만 사용하라.
- **클러스터가 많거나 기반 대상이 많으면 수평으로 퍼진다** — 하지만 해법은 외부를 회색 버킷 하나로 납작하게 만드는 게 *아니다*; 그건 P1을 희생시키고, 외부 모듈도 여전히 제 테두리 박스를 가질 자격이 있다. **중첩 클러스터**로 절충하라: 모듈별 외부 서브클러스터(각각 라벨+테두리)를 하나의 바깥 `external` 밴드 클러스터 안에 감싸고, 최상단 랭크에 고정한다. 너비는 **보이지 않는 `constraint=true` 엣지**로 쌓아서 제한한다 — *모듈 내부*(한 모듈의 타입들을 수직으로 쌓기), 그래도 넓으면 *모듈 간*(모듈들을 2~3개의 짧은 열로 사슬처럼 연결) — `rank=same/source/sink` 대신 쓰는데, 이건 `rankdir=BT` 아래 클러스터 내부에서는 신뢰할 수 없기 때문이다. 이렇게 하면 모듈별 박스(P1)를 유지하면서 *동시에* 너비도 제한된다.

## Legend + header comment

- **범례 = 렌더링된 예시 엣지**여야지, 텍스트 표가 아니다. `cluster_legend` 안에 5가지 종류 각각에 대해 실제로 `L_a -> L_b [arrowhead=onormal, ...]` 를 그려서, 범례가 그래프가 실제로 쓰는 스타일 *그대로* 렌더링되게 한다(자체검증이 되고, 산문보다 더 잘 가르쳐준다). 클러스터가 많은 그래프에서는 연결 안 된 범례 행들이 캔버스 너비만큼 늘어난다 — 행 사이에 보이지 않는 `constraint=true` 엣지로 수직으로 쌓아라.
- **헤더 주석**(`.dot` 맨 위): ① 제목 + 날짜 ② 목표 / 범위 ③ 5가지 엣지 종류 한 줄 요약 ④ 범위 출처(spec/plan) + 제외 사유 ⑤ 렌더 명령어.
- 주석/라벨: 프로젝트 컨벤션을 따른다(이 프로젝트: 한국어 산문 + ASCII/영문 시그니처).

## Render

```bash
dot -Tsvg NAME.dot -o NAME.svg
dot -Tpng -Gdpi=140 NAME.dot -o NAME.png
```

항상 **둘 다** 렌더링하고 SVG를 눈으로 확인하라 — `dot` 이 exit 0이라고 해서 읽기 좋은 다이어그램이라는 뜻은 아니다. 클러스터가 흩어지거나 엣지가 심하게 교차한다면, 대개 Phase-4/A1 규율을 건너뛴 게 원인이다(`TB + splines=spline + no-constraint` 그래프가 전형적인 실패 사례).

## Decision-gate mode — candidate ownership mini-graphs

- **트리거**: 배치나 레이어링 결정이 발생할 때(신규구조물 신고 템플릿의 `종류` 필드가 이를 표시한다), 산문으로 논증하기 *전에* 먼저 그린다.
- **형식**: 후보 배치 **하나당** 미니 `.dot` 하나 — 노드는 3~7개만(관련 클래스들); 엣지 = 소유권(다이아몬드) + 호출(화살표). 후보들을 나란히 놓고 화살표 방향을 비교한다; "추상이 구체를 아는"(역방향 화살표) 후보는 탈락.
- **저장 위치**: 결정을 기록하는 spec/handoff와 **같은** 디렉토리에 `<decision-name>-ownership.dot/.svg` 로 저장하고, 그 문서에서 상대링크를 건다(감사 게이트가 이후 링크 생존을 감시한다).
- 이것이 `confidence-and-sourcing` §1.5의 'Uncertain' 등급을 위한 기본 도구다.

## Before/After mode — structure-change pair for specs

- **시점**: 아키텍처를 변경하는 모든 spec(모듈/클래스/소유권/레이어 변경)에 필수 — 무조건, Decision-gate 의 형제(gate = 결정 전 후보 비교; 이것 = 결정 후 산출물).
- **형식**: 눈으로 diff할 수 있도록 레이아웃/랭크 규율이 **동일한** 두 그래프 — `<spec-name>-before.dot` 와 `<spec-name>-after.dot`; 변경되는 노드/엣지는 하이라이트 색 + 짧은 범례(added / removed / moved)를 받는다; 변경 안 된 맥락은 무채색 회색으로 유지; 보통 5~12개 노드(전체 코드베이스가 아니라 영향 반경만 자른다).
- **저장 위치**: .dot + 렌더링된 .svg 둘 다 spec **바로 옆**에, 거기서 상대링크(문서 감사 게이트가 이후 링크를 감시한다).
- 이 쌍은 spec의 HTML 검토 대시보드(`spec-review-dashboard`)에 나란히 임베드된다.

## Worked example & rationale (bundled)

목표의 *형태*에 맞는 worked example을 골라라:

- **`references/worked-example.dot`** — **소비자-형태**: 렌더 모듈 ↔ 애플리케이션 소비자(클러스터 6개, 양방향/사이클 체크, UML HTML 노드, `edge[]` 블록 5개, 렌더링된 예시 범례, 방향-단언 노트). *"모듈 X ↔ 그 소비자"* 를 위한 템플릿.
- **`references/worked-example-foundation.dot`** — **foundation/library-형태**: leaf 모듈(`SJH::sprite`)과 그 의존성 팬. 소비자 예시엔 없는 패턴들을 보여준다 — `«free fn»` 노드, 하나의 밴드 안에 **모듈별 중첩 외부 서브클러스터**(각 외부 모듈에 테두리) + 수직으로 쌓은 범례(anti-sprawl, P1과 제한된 너비를 절충), 직계-base만 있는 외부 박스, 그리고 라이브러리-모듈의 *".cpp가 엣지를 발생시킨다"* 읽기 방식. *"모듈 자신의 의존성을 다이어그래밍"* 하기 위한 템플릿.
- **`references/methodology-rationale.md`** — 이 스타일이 순진한 auto-layout 덤프보다 나은 *이유*: 동종 다이어그램 8개와의 적대적 비교, 그리고 솔직한 한계(베이스라인이 실제로 더 나았던 지점도 포함). 원칙 뒤의 근거가 궁금하거나, 특이한 그래프에 이 방법론을 적용할 때 읽어라.
