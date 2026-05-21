# AGENTS_GUIDE — 설계 의도와 진화 가이드

> 이 문서는 사용자께서 **"리서치 결과를 추후 지침으로 만들 것"**이라고 하신 마지막 문장에 직접 응답한다.
>
> `.claude/agents/`의 5개 파일은 완성품이 아니라 **사용자가 자기 코드베이스에 맞춰 직접 편집·진화시킬 출발점**이다. 이 문서는 "왜 이렇게 생겼는가"와 "어디를 어떻게 바꿔야 하는가"를 설명한다.

---

## 1. 가장 중요한 설계 결정 5가지와 근거

### 결정 1: 5개 에이전트 (3개나 7개가 아님)

**근거**: D 논문 (인하대, 김준영, 2026-02)의 Aider Refactoring Benchmark 89개 Python 문제 정량 결과:

| 구조 | GPT-4 | GPT-4o |
|---|---|---|
| 단일 모델 | 44.9% | 59.6% |
| 7-에이전트 (직무 모사) | 62.9% | 73.0% |
| **5-에이전트 (운영 흐름)** | **70.8%** | **77.5%** |

**핵심 함의**: "에이전트 수가 많을수록 안전"하다는 직관은 **틀렸다**. 운영 흐름과 정렬된 5분해가 직무 모사 7분해를 이긴다.

**진화 시 주의사항**:
- 6번째 에이전트를 추가하기 전에 **기존 5개의 도구를 강화**하라
- 예: "셰이더 검증" 단계가 더 필요하다 → 새 `render-shader-validator` 만들지 말고, `render-quality-gate`에 셰이더 검증 도구를 추가

### 결정 2: 운영 흐름 = 분석 → 수정 → 검증 → 품질 게이트 → 통합

**근거**: D §3.4의 5-Agent 운영 기반 분리 구조 그대로:
1. **테크 아키텍트 (분석)** = `render-architect`
2. **리팩터러 (수정)** = `render-refactorer`
3. **테스트 및 디버깅 (검증)** = `render-test-debug`
4. **품질 게이트** = `render-quality-gate`
5. **프로젝트 매니저 (통합)** = `render-pm`

**왜 이 5단계인가**: 실제 소프트웨어 운영 워크플로우의 자연스러운 분해. 직무 모사("프론트엔드 개발자 / 백엔드 개발자")가 아니라 **공정 단계** 분해.

**진화 시 주의사항**:
- 단계 순서를 바꾸지 마라 — 분석 없이 수정하면 D §1.1의 "맥락 손실" 문제 재발
- 단계를 합치지 마라 — 검증과 품질 게이트를 하나로 합치면 A §7의 "anti-gaming" 격리가 깨진다

### 결정 3: render-architect는 코드 수정 권한이 절대 없다

**근거**: A 논문 (Rehan, Fiverr Labs, TDAD, 2026-03) §7:

> "anti-gaming requires separate invocations with restricted artifact access; a single continuous session should not be used when hidden tests are used for evaluation."

**핵심 함의**: 같은 세션에서 분석·수정·테스트를 모두 하면 같은 환각이 모든 산출물에 인쇄된다. 분석은 분석만, 수정은 수정만.

**진화 시 주의사항**:
- 편의를 위해 render-architect에 Edit를 추가하지 마라 — A 논문의 24 trial 결과(MS 86-100%)는 격리 덕분
- hooks의 PreToolUse가 이를 시스템 레벨에서 강제

### 결정 4: 같은 세션에서 테스트와 코드 함께 만들기 금지

**근거**: A §7 직접 인용 + B 논문 Table 9의 1-2 라운드 피드백 25% 악화 데이터.

**구현**:
- `render-refactorer`는 코드만, `render-test-debug`는 검증만
- 테스트 코드 자체는 사람이 작성 (또는 별도 세션에서 별도 에이전트가 작성 후 사람 검수)

**진화 시 주의사항**:
- "Test-Writer 에이전트"를 추가하고 싶다면 **반드시 별도 invocation** + tests_visible/tests_hidden 디렉터리 분리 (A 논문 패턴)
- 단순히 "render-refactorer가 테스트도 함께 만든다"로 합치지 마라

### 결정 5: Mesa llvmpipe = 진실의 원천

**근거**:
- A 논문이 지적한 "stochastic outputs" 문제 → 그래픽스에서는 드라이버 차이가 주범
- llvmpipe는 CPU에서 결정론적으로 실행되는 OpenGL 소프트웨어 렌더러 (Mesa 공식 문서: "uses LLVM as a code-generator")
- 5회 동일 PNG 재현성 확보 가능 → 골든 이미지의 결정성 보장

**진화 시 주의사항**:
- 실 GPU에서 캡처한 이미지를 골든으로 쓰지 마라
- `LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe` 환경변수 강제

---

## 2. 어디를 어떻게 수정해야 하는가 (사용자 코드베이스에 맞춰)

### 2.1 빌드 시스템이 다르면

**현재 가정**: CMake + Ninja + GoogleTest

| 사용자 환경 | 수정 위치 |
|---|---|
| MSBuild | `render-refactorer.md`의 빌드 섹션 + `hooks.json`의 PostToolUse |
| Catch2 / doctest | `render-quality-gate.md`의 "단위 테스트" 섹션 |
| 사내 빌드 | 모든 에이전트의 Bash 명령 부분 |

**최소 수정 원칙**: 명령어만 바꾸고 절차는 그대로 유지.

### 2.2 OpenGL 외 다른 API 추가하면

**현재 가정**: OpenGL 단일 백엔드

추가 시 수정 위치:
- `CLAUDE.md` §A.3: 백엔드 매트릭스에 새 API 추가
- `golden-capture.md`: API별 헤드리스 백엔드 추가
  - DirectX 11/12 → WARP (Windows ICD)
  - Vulkan → SwiftShader 또는 lavapipe
  - Metal → 결정적 백엔드 부재, 별도 전략 필요

**주의**: 멀티 백엔드는 **각 백엔드별로 골든 이미지 별도 관리**. 한 골든을 모든 백엔드에 사용하면 드라이버 차이로 즉시 깨진다.

### 2.3 사내 RAG knowledge base가 있다면

**현재 가정**: RAG 없음 (render-architect가 OpenGL 일반 지식만 사용)

추가 시:
- `render-architect.md` 의 "Step 5"에 `mcp__rag__search` 도구 호출 추가
- C 논문(Yonsei, 김연수)의 IEEE Std 2800 RAG 패턴 참조: 표준 문서 → vector embedding → 동적 retrieval
- MCP 서버 별도 구축 필요 (Anthropic MCP spec 따름)

### 2.4 Mutation Testing 도구가 다르면

**현재 가정**: mull (LLVM 기반)

대안:
- Dextool mutate (소스 단계)
- 사내 도구

**수정 위치**: `render-quality-gate.md`의 "Gate 5" 섹션. 명령어와 출력 파싱만 바꾸면 됨.

### 2.5 CrewAI 등 외부 오케스트레이터를 쓴다면

**현재 가정**: Claude Code의 서브에이전트 시스템 단독 사용

D 논문은 CrewAI 0.95.0 + Process.sequential을 사용했다. CrewAI를 외부 오케스트레이터로 쓰고 싶다면:

- Claude Code의 5개 에이전트를 **CrewAI Agent**로 그대로 매핑 가능
- Process.sequential + depends_on 매개변수로 같은 흐름 재현
- 단, **격리 메커니즘은 직접 구현 필요** — CrewAI 자체에는 chmod 같은 시스템 강제가 없음

이 경우 hooks.json은 무용. 대신 CrewAI의 `before_kickoff` / `after_kickoff` 콜백으로 격리.

---

## 3. 사용자가 PoC 실행 후 회고할 6가지 질문

Phase 1 (1주) 끝나면 다음 질문에 답해보고 에이전트 정의를 진화시킨다.

### Q1. 5-에이전트 중 어느 것이 가장 자주 실패했나?

- 자주 실패한 에이전트의 시스템 프롬프트가 부정확하다는 신호
- 실패 모드를 분석해 시스템 프롬프트의 "절대 금지" 섹션을 보강

### Q2. iteration budget(outer 6, inner 8)이 적절한가?

- 평균 2-3 iteration에서 끝나면 budget 줄여도 됨 (비용 절감)
- 자주 budget 소진하면 명세 모호성(A §6.3 oscillation) 가능성 → 사람 escalate 빈도 늘리기

### Q3. FLIP weighted median 임계값(0.05)이 게임 시각 품질에 맞는가?

- 0.05가 너무 엄격하면 false positive (회귀 아닌데 회귀로 오판)
- 너무 느슨하면 false negative (회귀를 통과시킴)
- 게임의 시각적 특성(예: 동적 그림자가 많음 → 높은 임계값 필요)에 맞춰 캘리브레이션

### Q4. Mutation Score 60% 임계값이 현실적인가?

- 신규 모듈은 처음부터 60% 가능
- 레거시 모듈은 처음엔 30%부터 시작해 점진적 상향이 현실적
- D 논문도 이 점진성을 한계 §5에서 명시

### Q5. PR당 비용이 견적($5)에 맞는가?

- A 논문 §6.5: $2-3/spec version (Anthropic API 1월 2026)
- C++ 빌드 시간 + 골든 이미지 비교 추가 → $5 추정
- 실측 후 조정

### Q6. 어떤 안티패턴이 실제로 발생했나?

CLAUDE.md §D의 5가지 금지 패턴 중 실제로 발생한 것을 표시:
- [ ] 같은 세션에서 테스트와 코드 함께 생성
- [ ] 1-2 라운드 피드백 (3+로 자동 변환되어야)
- [ ] Mutation Score 100%에 안심
- [ ] 명세 모호성 oscillation
- [ ] 7-에이전트 식 직무 모사 분해 회귀

발생한 패턴마다 hooks 또는 시스템 프롬프트로 추가 차단.

---

## 4. 4편 PDF 인용 빠른 참조

본 가이드의 결정들이 어느 논문의 어느 섹션에 근거하는지 빠른 참조:

| 결정 | 논문 | 위치 |
|---|---|---|
| 5-에이전트 운영 분리 | D | §3.4, Table 4 |
| 같은 세션 테스트+코드 금지 | A | §7 |
| Mutation testing 게이트 | A | §4.2, §6.4 |
| 3+ 라운드 피드백 | B | Table 9 |
| 결정적 시드 시나리오 | A | §3.5 |
| Mock RHI 패턴 | (1차 리포트의 일반론) | — |
| 비용 견적 $2-3/run | A | §6.5 |
| iteration budget (6, 8) | A | §6.1 Algorithm 1 |
| Visible/hidden 분리 | A | §4.1 |
| Activation probe (87%) | A | §4.2 |
| RAG knowledge base | C | §3.3 |
| 75% 시간 단축 비교 | C | §4.9 |
| Visual fidelity 자동 검증 | B | §3.3 |

---

## 5. "이건 안 만들었음" 명시 (anti-hallucination 원칙)

본 PoC가 다음을 **만들지 않았음**을 명시:

- **Mock RHI 인터페이스 자체** — 사용자 게임 엔진의 RHI 구조를 모르므로. PoC Day 1에 사용자가 직접 정의해야 함
- **첫 골든 이미지** — 사용자의 실제 렌더 패스에 의존
- **mull 또는 다른 mutation 도구의 정확한 설치 명령** — 사용자 OS / 빌드 환경 의존
- **MCP 서버 구현** — RAG가 필요하면 사용자가 별도 구축
- **CI/CD 통합** — GitHub Actions / GitLab CI 등 어느 것을 쓰는지 모름
- **사내 코드 컨벤션** — clang-tidy의 정확한 규칙은 사용자 사내 표준에 맞춰야 함

이것들은 **Phase 1 Day 1-2에 사용자가 직접 채워 넣을 항목**이다. PHASE1_RUNBOOK.md 참조.
