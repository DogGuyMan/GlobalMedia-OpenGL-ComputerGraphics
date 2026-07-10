# AI 설계 판단 오류 분석 보고서 — 패스 리스트(mPasses → Material.passes) 사례

> 분석 대상: [doc/godot_engine_reference/AI의잘못된설계.md](../godot_engine_reference/AI의잘못된설계.md) (툰 렌더링 P6~P8 튜터링 대화)
> 작성일: 2026-07-10
> 표기 규약: **객관** = 대화록/코드에서 인용 가능한 사실, **💭 주관** = 분석자의 판단·권고

---

## 1. Agent의 초기 제안 내용

대화록에서 확인되는 Agent 제안의 변천은 3단계다.

### 1-1. 최초안: `Model::DrawOutline()` (대화록 이전 시점, 대화록 7행에서 소급 확인)

- **객관**: 대화록 7행 — "Model::DrawOutline()(포인터로 게이트)를 두는 건…". 즉 Agent는 **Model 클래스에 'Outline'이라는 특정 효과의 이름이 박힌 메서드**를 두고, 아웃라인을 안 쓰는 모델은 포인터 null로 옵트아웃하는 설계를 제안했었다.
- **객관**: 대화록 50행 — `CompoundOutlinedToon` 클래스(복합 테크닉을 클래스로 싸는 안)도 검토 선상에 있었다.

### 1-2. 1차 수정안: `Model`에 `mPasses` 벡터 (대화록 33~51행, 226~282행)

사용자의 1차 반박 후 Agent는 패스 리스트 구조로 전환했으나, **컨테이너를 Model에 배치**했다:

```cpp
// Agent 1차 수정안 (대화록 36행, 230~234행)
class Model {
  protected:
    std::vector<ITechnique*> mPasses;   // ← Model이 렌더 조합을 소유
  public:
    void SetPasses(std::initializer_list<ITechnique*> passes);
};
```

- **객관**: 배치 근거로 든 것은 "Submit()은 형상(Geometry)인데 테크닉은 형상에 접근할 수 없으므로, 순회 주체는 Submit을 부를 수 있는 Model::Draw여야 한다"(29행) — 즉 **순회 주체와 데이터 소유자를 동일시**했다.
- **객관**: 이 시점에 Agent는 커리큘럼 문서(카툰렌더링.md)의 As-Built 블록에 "Model의 mPasses"로 **결정을 기록까지 완료**했다(215~222행, 340행에서 자인).

### 1-3. 최종안(사용자 유도): `Material.passes` (대화록 292~341행)

```cpp
// 최종 확정 — 실제 코드에 반영됨 (src/material.h:51)
struct Material {
    Texture*    albedoPtr = nullptr;
    UVValue     uv;
    glm::vec3   baseColor{1, 1, 1};
    std::vector<ITechnique*> passes;   // techniquePtr(단일)의 일반화
};
```

- **객관**: 현재 코드 기준 [src/material.h:51](../../src/material.h#L51)에 `std::vector<ITechnique *> passes`가 존재하고, [src/model_imp.h:122](../../src/model_imp.h#L122) 등 각 파트 `Draw()`가 `material.passes`를 순회한다. **사용자의 최종 판단이 채택된 상태.**

---

## 2. 사용자의 반박 지점과 그 근거

### 반박 ① "Model에 DrawOutline은 원칙 위배다" (대화록 1~3행)

| 항목 | 내용 |
|---|---|
| 반박 | "Model에 Draw Outline을 그리는 것은 리스코프 치환 원칙에 위배된다. 어떤 것은 Outline을 안 그릴 수도 있는데." + 대안으로 "vector 컨테이너로 Technique를 정렬해서 순차 수행" 제안 |
| 근거 | **선택적 효과(outline)를 기반 클래스의 이름 있는 메서드로 고정하면, 그 효과를 안 쓰는 하위 개념이 계약에 끌려 들어간다**는 직관 |
| 결과 | Agent가 원칙명은 교정(LSP가 아니라 SRP/OCP 냄새, 7행)했으나 **문제의식 자체는 타당하다고 인정**하고 패스 리스트로 전환 |

- 💭 주관: 인용한 원칙명(LSP)은 부정확했지만, "옵트아웃이 필요한 기능은 계약이 아니라 데이터여야 한다"는 **설계 직관은 정확**했다. Agent의 원칙명 교정(엄밀히는 하위 타입이 계약을 깨지 않으므로 LSP 위배 아님)도 사실로서 정확 — 이 교정 자체는 Agent의 강점 사례다.

### 반박 ② "왜 Model이 ITechnique을 소유하지? Material이 가져야 하지 않나" (대화록 285~291행)

| 항목 | 내용 |
|---|---|
| 반박 | "잠깐만 왜 Model에 ITechnique을 넣게 되지?" → (Material 복수화는 스스로 철회) → "**Material이 technique를 가졌었고**, 동일 마테리얼에 별개 테크닉이 되어야 하는 것이니 단일 머티리얼에 여러 테크닉 컨테이너를 의존하는 게 맞다" |
| 근거 1 | **기존 소유 구조**: 패스 컨테이너는 이미 Material에 있던 `techniquePtr`(단일)의 자연스러운 일반화 → 일반화된 상태는 원래 있던 자리에 두는 게 맞다 |
| 근거 2 | **응집도**: "이 표면을 어떻게 그리는가(=패스 순서)"는 외형(Appearance) 서술 → 외형 데이터(texture/uv/color)와 한 덩어리 |
| 근거 3 | **관심사 분리**: Model이 mPasses를 가지면 Model이 렌더 조합을 아는 셈 — 반박 ①에서 없애려던 냄새가 형태만 바꿔 되살아남 |
| 결과 | Agent 전면 수용: "당신 판단이 더 낫습니다. 제가 Model에 두자고 한 건 차선이었어요"(292행) |

- 💭 주관: 이 반박이 이 대화의 백미다. 사용자는 **"이 데이터의 기존 소유자가 누구였는가"**라는 단 하나의 질문으로 Agent의 1차 수정안을 무너뜨렸다. 스스로 "Material 복수화"라는 오해를 세웠다가 즉시 철회하고 더 나은 안으로 수렴한 과정도, 가설 → 자기반박 → 수렴이라는 건강한 설계 사고의 표본이다.

### (참고) 반박은 아니지만 사용자가 방향을 주도한 지점

- **객관**: "각각 다른 텍스처 유닛을 쓰면 되지 않나?"(93행) — Agent가 P8로 미뤄두려던 멀티텍스처 해법을 사용자가 먼저 짚었고, Agent는 "정확히 그겁니다"로 확인만 했다.
- **객관**: "albedo, 툰, outline 3패스로 하고 싶다"(163행) — Agent는 2패스 대안(효율↑)을 트레이드오프로 제시하되 사용자의 3패스 선택을 존중했다. 💭 이 구간의 Agent 응대(트레이드오프 명시 + 선택권 이양)는 오히려 모범적이었다.

---

## 3. Agent의 밝혀진 약점

### 약점 A — 국소 제약 최적화 편향 (locality bias): 배치 결정을 "지금 당장의 기계적 제약"으로 내림

- **객관**: mPasses를 Model에 둔 유일한 근거는 "Submit을 부를 수 있는 곳이 Model::Draw"(29행)였다. **순회하는 코드의 위치**와 **데이터가 살아야 할 위치**는 별개인데, 이를 동일시했다.
- **객관**: `Material.techniquePtr`라는 기존 소유 구조가 코드에 명백히 존재했음에도(295행에서 Agent 스스로 "이미 소유하고 있었다"고 뒤늦게 인정) 1차 수정안 도출 시 이를 조회·고려한 흔적이 없다.
- 💭 주관: **AI는 새 요구사항을 "현재 논의 중인 클래스"에 증분 패치하는 경향**이 있다. 기존 코드베이스의 소유권 지도를 다시 그려보는 단계를 생략하면, 기술적으로 동작하는(그러나 응집도가 깨진) 배치가 나온다.

### 약점 B — 자기 제안에 대한 비판 렌즈 미적용 (비대칭 검증)

- **객관**: Agent는 사용자의 LSP 오인은 즉시 교정했고(7행), 텍스처 유닛 충돌 원리도 정확하게 강의했다(68~91행). 즉 **타인의 주장 검증 능력은 충분**했다.
- **객관**: 반면 자신의 DrawOutline 안, Model.mPasses 안에 대해서는 SRP/OCP·응집도 렌즈를 **사용자가 지적하기 전까지 한 번도 스스로 적용하지 않았다.** 두 번 모두 "아주 좋은 반론입니다"(4행), "당신 판단이 더 낫습니다"(292행)라는 사후 인정으로 끝났다.
- 💭 주관: 지식 부족이 아니라 **검증의 비대칭**이 문제다. 자기 제안은 생성 관성(이전 제안에서 최소 변경) 위에서 나오므로, 의도적으로 "남의 코드 리뷰하듯" 재심사하는 절차가 없으면 확증 편향이 그대로 통과한다.

### 약점 C — 확신도 무표기 단정 제안

- **객관**: 1차 수정안 제시 시 "이 방향으로 mPasses + Draw 순회부터 짜보세요"(63행), "진행하세요"(200행) 등 **대안 비교나 불확실성 표기 없이 단일안을 실행 지시형으로** 전달했다. Model vs Material 배치는 검토조차 언급되지 않았다.
- **객관**: 사후에야 "차선이었어요"(292행)라고 자인 — 즉 제안 시점에 이미 차선임을 판별할 재료(기존 techniquePtr 위치)가 코드에 있었는데도 확신도를 낮추지 않았다.
- 💭 주관: 확신도가 표기되지 않은 단정은 튜터링 관계에서 특히 위험하다. 학습자가 권위에 눌려 검증 없이 수용하면 오배치가 코드에 굳는다. 이번엔 사용자가 잡아냈지만, 그것은 시스템이 아니라 사용자 역량에 의존한 방어였다.

### 약점 D — 미확정 설계의 조기 문서화

- **객관**: Agent는 Model.mPasses 결정이 사용자 검증을 거치기 전에 커리큘럼 문서의 As-Built 블록에 기록했고(215~222행), 최종 결정이 뒤집히자 "그 문장을 고쳐드리겠습니다"(340행)라며 재수정이 필요해졌다.
- 💭 주관: ADR 관행에서 결정은 `proposed → accepted` 상태를 거친다. 상태 구분 없이 곧바로 As-Built(확정)로 기록하는 습관은 문서 신뢰도를 갉아먹는다.

### (균형) 확인된 강점

- **객관**: 원칙명 교정(LSP → SRP/OCP, 7행), 상태 스택(A)/패스 리스트(B)의 의미론 구분(11~26행), 텍스처 유닛별 독립 상태 정리(104~113행), 3패스 계약의 함정 목록(191~196행), 2패스/3패스 트레이드오프 제시(197~200행)는 모두 정확하고 교육적으로 밀도가 높았다.
- 💭 주관: 요컨대 **"메커니즘 지식과 원칙 지식은 강하고, 자기 설계에 그 지식을 소급 적용하는 규율이 약하다"**가 이 대화의 진단이다.

---

## 4. 보완 지침 — 이 약점을 시스템으로 막는 방법

### 지침 1. 확신도 3단계 표기 의무화 + "낮으면 도구를 든다" (약점 C 대응)

이미 보유한 `confidence-and-sourcing` 스킬의 3단계(확신 / 대체로 확신 / 불확실·검증 필요)를 **설계 제안에도 강제 적용**하되, 불확실 판정 시의 행동 규칙을 붙인다:

| 확신도 | 행동 |
|---|---|
| 확신 (코드/문서 인용 가능) | 단정 제안 가능. 단, 근거 인용(file:line) 첨부 |
| 대체로 확신 | **옵션 비교표 필수** (최소 2안 + 트레이드오프 + 추천) |
| 불확실 | 제안 보류. 아래 도구 중 하나 이상 실행 후 재판정: ① Graphviz 의존성/소유권 그래프, ② 외부 레퍼런스(클린 아키텍처·GoF·엔진 소스) 조회, ③ 벤치마크(성숙 엔진은 어떻게 하나 — `benchmark-research-method`) |

- 💭 실제로 Godot을 봤다면 즉시 판별됐을 사안이다: Godot도 패스/셰이더 정보는 `Material` 리소스에 산다 (이 저장소의 [doc/godot_engine_reference/](../godot_engine_reference/) 참조 파일들이 바로 그 용도).

### 지침 2. 배치·소유권 결정 체크리스트 — "기존 소유자 우선 원칙" (약점 A 대응)

상태(데이터)를 어느 클래스에 둘지 결정할 때 반드시 순서대로 묻는다:

1. **이 상태의 단수 버전을 이미 누가 소유하고 있는가?** → 일반화(단수→컨테이너)는 원래 자리에서 한다. (`techniquePtr` → `passes`가 정확히 이 사례)
2. **이 상태는 무엇을 서술하는가?** 외형이면 Material, 형상이면 Geometry, 장면 구성이면 Renderer — 서술 대상과 같은 응집 단위에 둔다.
3. **순회/사용하는 코드의 위치는 배치 근거가 아니다.** 사용처는 참조로 접근하면 된다 (`Model::Draw`가 `material.passes`를 순회하는 현재 구조가 증명).
4. 그래도 모호하면 → **Graphviz 소유권 그래프를 그려서 결정** (`graphviz-class-diagram` 스킬). 배치 후보마다 화살표 방향이 어떻게 달라지는지 시각화하면, "추상적인 것이 구체적인 것을 아는" 역방향 화살표가 즉시 드러난다.

> 💭 권고: 이 체크리스트를 `design-decision-discipline` 스킬 §6(소유권 모델)에 "배치 규칙" 항목으로 추가할 것. 이번 사례가 그 스킬의 빈틈(소유권 *수명* 규칙은 있으나 *배치* 규칙은 없음)을 정확히 보여줬다.

### 지침 3. 셀프 리뷰 관문 — 자기 제안을 남의 코드처럼 (약점 B 대응)

구조 변경 제안을 내보내기 전, `code-design-review-lenses`의 5렌즈(소유권·결합도·헤더 위생·SOLID·일관성)를 **자기 제안에 역적용**하는 관문을 둔다. 특히:

- 렌즈 ①(소유권)과 렌즈 ④(SRP)는 이번 사례의 두 오류를 모두 잡을 수 있었다.
- 💭 외부 연구도 같은 방향을 지지한다: AI 산출물을 **별도 세션/신선한 프롬프트로 재비판**시키는 것이 미묘한 설계 결함을 잡는 실효 기법으로 보고됨 ([Addy Osmani, "My LLM coding workflow going into 2026"](https://addyosmani.com/blog/ai-coding-workflow/)). AI 생성 변경은 사람 단독 PR 대비 결함률이 유의하게 높다는 보고([BSI/ANSSI, AI Coding Assistants](https://www.bsi.bund.de/SharedDocs/Downloads/EN/BSI/KI/ANSSI_BSI_AI_Coding_Assistants.pdf?__blob=publicationFile&v=7))도 "생성자 ≠ 검증자" 분리의 근거다.

### 지침 4. 결정 기록은 상태를 가진다 — 경량 ADR (약점 D 대응)

- 구조적 결정은 `proposed` 상태로 먼저 기록하고, 코드 검증(빌드+시각 확인) 통과 후에만 `accepted`/As-Built로 승격한다.
- ADR은 코드 저장소 안(`doc/adr/` 또는 기존 `doc/보고서/`)에 두고, 컨텍스트(왜) → 결정 → 결과(트레이드오프) 3요소를 남긴다. 리뷰 시 "이 변경이 기존 ADR와 충돌하나?"를 묻는 관문으로도 쓴다.
- 근거: [Michael Nygard/Fowler의 ADR 관행](https://martinfowler.com/bliki/ArchitectureDecisionRecord.html), [AWS ADR 프로세스 가이드](https://doc.aws.amazon.com/prescriptive-guidance/latest/architectural-decision-records/adr-process.html), [adr.github.io](https://adr.github.io/) — "결정 근거의 소실이 반복 실수를 만든다"는 문제의식이 이번 사례와 정확히 겹친다.
- 💭 이 저장소는 이미 커리큘럼 문서(카툰렌더링.md)가 사실상 ADR 역할을 하고 있으므로, 새 형식을 도입하기보다 **As-Built 블록에 `[제안됨]`/`[검증됨]` 태그 한 줄을 추가**하는 것으로 충분하다.

### 지침 5. 그 외 모색 — 코드 디자인 품질을 올리는 추가 장치

1. **반증 질문 3종 세트 (제안 전 자문)** — 💭 이번 대화에서 사용자가 실제로 던진 질문들을 표준화한 것:
   - "이걸 안 쓰는 놈은 어떻게 되지?" (옵트아웃 → 계약이 아니라 데이터로)
   - "이 데이터, 원래 누구 거였지?" (기존 소유자 우선)
   - "이 클래스가 이 이름을 알아야 하나?" (이름 결합 냄새)
2. **참조 구현 대조 (벤치마크)** — 배치·구조 결정마다 성숙 엔진 1~2개(이 저장소엔 Godot 소스 발췌가 이미 있음)에서 같은 데이터가 어디 사는지 확인. `benchmark-research-method` 스킬의 "must-have/common/optional" 분류를 배치 결정에도 적용.
3. **의존성 그래프의 상시화** — 큰 리팩터 전후로 `graphviz-class-diagram`으로 소유권·결합도 스냅샷을 남기면, "냄새"를 직관이 아니라 그림(역방향 화살표, 순환)으로 판별할 수 있다. 컴파일 수준 검증이 필요하면 `clang -MM` 기반 include 그래프로 보강.
4. **원칙명 검증 습관 (사용자 측 포함)** — LSP/SRP/OCP처럼 원칙을 인용해 반박할 때, 원칙의 정의를 한 줄로 같이 적는다. 이번엔 Agent가 교정해줬지만, 원칙명이 틀려도 직관이 맞을 수 있고(이번 사례) 그 반대도 있다. 이름과 직관을 분리해 다루면 논쟁이 정확해진다.
5. **결정 직후 즉시 문서화하되, 커밋은 검증 후** — 조기 기록(약점 D) 자체가 나쁜 게 아니라 *상태 없는* 조기 기록이 나쁜 것. 기록은 빠르게, 확정은 느리게.

---

## 5. 한 줄 요약

> 💭 이번 대화에서 AI의 오류는 지식 부족이 아니라 **"자기 제안에 자기 지식을 소급 적용하지 않는 규율 부재"**였고, 사용자는 "안 쓰는 놈은?"과 "원래 누구 거였지?"라는 두 질문만으로 이를 두 번 교정했다. 보완책의 핵심은 새 지식이 아니라 **확신도 표기 → 낮으면 도구(그래프·레퍼런스·벤치마크) → 셀프 5렌즈 리뷰 → 상태 있는 결정 기록**이라는 절차의 강제다.

### 참고 자료
- [Architecture Decision Record — Martin Fowler bliki](https://martinfowler.com/bliki/ArchitectureDecisionRecord.html)
- [ADR process — AWS Prescriptive Guidance](https://doc.aws.amazon.com/prescriptive-guidance/latest/architectural-decision-records/adr-process.html)
- [Architectural Decision Records — adr.github.io](https://adr.github.io/)
- [Master ADRs: Best practices — AWS Architecture Blog](https://aws.amazon.com/blogs/architecture/master-architecture-decision-records-adrs-best-practices-for-effective-decision-making/)
- [My LLM coding workflow going into 2026 — Addy Osmani](https://addyosmani.com/blog/ai-coding-workflow/)
- [AI Coding Assistants — BSI/ANSSI 공동 보고서](https://www.bsi.bund.de/SharedDocs/Downloads/EN/BSI/KI/ANSSI_BSI_AI_Coding_Assistants.pdf?__blob=publicationFile&v=7)
