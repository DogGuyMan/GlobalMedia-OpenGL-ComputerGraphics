# Architecture Design Agent — Playbook (프로젝트 종속분)

> 다른 Agent (또는 미래의 자기 자신) 가 *이 프로젝트의 디자인 작업* 을 재현하기 위한 문서.
>
> 🔵 **범용 방법론은 전역 Skill 로 이관됨 (2026-06-18)** — 이 문서는 이제 *이 프로젝트 고유의 도구 설정 + 누적 합의 + 케이스 스터디* 만 보유한다. 일반 방법론이 필요하면 아래 Skill 을 먼저 켤 것:
> - **`architecture-design-workflow`** — 4-Phase 체인(brainstorm→평가→plan→dispatch) + Decision Framework(옵션표+추천) + Decision Log + spec/plan/commit 템플릿 + 사용자 상호작용 원칙. (구 §1·§3·§5·§6.1~6.3·§8)
> - **`benchmark-research-method`** — 모범 구현 N종 조회 → must-have/common/optional 분류 + 객관/주관 분리. (구 §4.1 쿼리 패턴)
> - **`agent-orchestration-anti-gaming`** — subagent 역할 권한 분리 + 안티게이밍.
>
> 아래 본문은 *위 Skill 을 이 프로젝트에 적용할 때의 구체 설정/합의* 다.

---

## 1. 정체성 + 적용 범위 (요약)

복합 역할 4가지를 한 명이 수행: **Design Steward / Architecture Auditor (ddd) / Implementation Orchestrator (subagent) / Brainstorming Collaborator**. 상세 워크플로우는 `architecture-design-workflow` Skill.

- **적합**: 다단계 아키텍처 재설계, 모범 엔진 벤치마크 후 도입, 레거시 점진 마이그레이션, 결정 명문화.
- **부적합**: 단순 버그픽스, 1파일 단발 변경, 탐색적 prototype.

---

## 2. 필수 도구 스택 (이 프로젝트 고정 설정)

### 2.1 Superpowers 플러그인 — 워크플로우 체인
```
brainstorming → writing-plans → subagent-driven-development → finishing-a-development-branch
```
각 단계 진입·종료 조건 명확 — 건너뛰지 말 것. (체인 운용 상세는 `architecture-design-workflow` Skill)

### 2.2 Context7 MCP — 모범 엔진 4종 조회 ID (이 프로젝트가 쓰는 정본 라이브러리 ID)
```yaml
Unity:    /websites/dev_epicgames_en-us_unreal-engine  # (Unreal 도 검색은 이 ID 부터)
Unreal:   /websites/dev_epicgames_en-us_unreal-engine
Cocos2D:  /cocos2d/cocos2d-x  +  /websites/cocos2d-x_api-ref_js_v3_8
Godot:    /godotengine/godot-docs
```

### 2.3 ddd 플러그인 (NeoLabHQ/context-engineering-kit) — 14 Rules
VSCode 환경에서 `/plugin install` 미동작 시 *수동 clone*:
```bash
git clone --depth 1 https://github.com/NeoLabHQ/context-engineering-kit.git /tmp/cek-ddd
ls /tmp/cek-ddd/plugins/ddd/rules/
```
14 rules: `clean-architecture-ddd`(HIGH) / `separation-of-concerns`(HIGH) / `command-query-separation`(HIGH) / `functional-core-imperative-shell`(HIGH) / `explicit-control-flow`(HIGH) / `explicit-data-flow`(HIGH) / `explicit-side-effects`(HIGH) / `error-handling`(HIGH) / `principle-of-least-astonishment`(HIGH) / `domain-specific-naming`(HIGH) / `library-first-approach`(HIGH) / `early-return-pattern`(MED) / `function-file-size-limits`(MED) / `call-site-honesty`(MED).

---

## 3~6. 워크플로우 / 벤치마크 / Decision Framework / 문서 템플릿 → Skill 로 이관

구 §3 (4-Phase Chain), §5 (Decision Framework + 옵션표/추천/AskUserQuestion/Decision Log), §6.1~6.3 (Spec/Plan/Commit 템플릿) 의 *일반 방법* 은 **`architecture-design-workflow` Skill** 에 있다. 구 §4.1 (벤치마크 쿼리 패턴) 은 **`benchmark-research-method` Skill**.

아래는 *이 프로젝트에 적용된 구체값* 만 남긴다.

### 4.2 비교 표 — 이 프로젝트의 4엔진 매핑 (실제 결과)
```markdown
| 우리 클래스 | Unity | Unreal | Cocos2D | Godot |
|---|---|---|---|---|
| `Actor` | `GameObject` | `AActor` | `Node` | `Node` |
| `Component` | `MonoBehaviour` | `UActorComponent` | `cc.Component` | — |
```
이 표가 spec/plan 의 *Cocos/Unreal/Unity/Godot 정통* 결정 근거.

### 4.3 우선순위 결정 (이 프로젝트)
- 이 프로젝트: **Cocos 1차 + Unreal 2차** (2D 탑다운 도메인).

### 6.4 프로젝트별 컨벤션 (이 프로젝트)
- 주석 / 커밋 메시지 *한국어*
- `Co-Authored-By` 트레일러 *미사용* (사용자가 의도적 제거)
- 헤더 가드 `__<PROJECT>_<MODULE>_<NAME>_H__` 형식
- 자원 핸들 클래스 = `Create()` factory + `UPtr` 반환 + 복사·이동 4종 `= delete` + `static_assert` 4종

각 프로젝트의 `.claude/CLAUDE.md` *반드시 확인*.

---

## 7. Quality Gates — ddd 14 Rules 적용 체크리스트

### 7.1 매 spec 작성 직후
다음 9 rules *최소 적용*:
- [ ] **Separation of Concerns** — 각 클래스/모듈이 단일 책임?
- [ ] **CQS** — Get* 가 pure query? 회색지대(memoization 등)가 honest 한가?
- [ ] **POLA** — 함수 이름이 약속한 것만 하는가?
- [ ] **Explicit Side Effects** — 단일 함수가 여러 GL/IO 호출을 숨기지 않는가?
- [ ] **Domain-Specific Naming** — `Manager`/`Helper`/`Utils` 같은 generic 회피?
- [ ] **Library-First** — 자체 구현 전에 stdlib/외부 라이브러리 확인?
- [ ] **Function/File Size Limits** — 각 파일 < ~200 lines, 각 함수 < ~30 lines?
- [ ] **Early Return** — null guard, miss case 일찍 return?
- [ ] **Functional Core / Imperative Shell** — pure 로직과 side-effect 호출 분리?

### 7.2 코드 구현(subagent) 후 code quality reviewer 가 검증
추가로: **Clean Architecture DDD** / **Error Handling** (typed error + logging) / **Call-Site Honesty** / **Explicit Control Flow** / **Explicit Data Flow**.

### 7.3 발견 시 패치 명문화
```
[docs] : <topic> ddd N차 재검증 패치 — <count> 종

발견 N 종 (M fix + K 결정 문서화):
- <발견 1 요약 + fix>
ddd Rules 정합 강화: <적용된 rule 목록>
```
이 프로젝트 예: `44395f1 [docs] SP3 spec ddd 2차 재검증 패치 — 5 종`, `32ea6f3 [docs] SP3 spec ddd 3차 검증 — namespace/class 충돌 fix`.

---

## 8. 사용자 상호작용 원칙 → `architecture-design-workflow` Skill

1메시지=1결정, 옵션+추천, 방향 전환 시 cascade 영향 명시, 사용자 직관 교정, 중간 업데이트 — 상세는 Skill. 프로젝트 가드: `Co-Authored-By` 미추가, force push/브랜치 reset 금지, `extern/sb7code` 수정 금지.

---

## 9. Anti-Patterns (프로젝트 가드 포함)

- **디자인**: 자체 결정 후 통보 ❌(옵션+동의) / Context7 조회 없이 추측 ❌ / 5+ 결정 일괄 ❌ / ddd 생략 커밋 ❌ / Out of Scope 누락 ❌
- **Plan**: "Similar to Task N" ❌ / "Add appropriate error handling" ❌ / test 없이 "Write tests" ❌ / file path·명령·Expected 누락 ❌
- **Subagent**: plan 파일 *읽으라고* 시키기 ❌(full task text 제공) / spec compliance review 생략 ❌ / "Issues found" 무시하고 다음 Task ❌ / 여러 implementer 병렬 ❌
- **커밋**: 무관 변경 묶기 ❌ / `Co-Authored-By` 임의 추가 ❌ / force push·reset ❌

(역할 권한 분리/안티게이밍 일반론은 `agent-orchestration-anti-gaming` Skill)

---

## 10. 신규 Agent 진입 체크리스트 — Fresh Start 30초

```bash
cat .claude/CLAUDE.md | head -50                                   # 1. 어떤 프로젝트
git status && git log --oneline | head -20                         # 2. 브랜치 + 최근 작업
ls doc/superpowers/specs/ && ls doc/superpowers/plans/           # 3. 진행 중 spec/plan
grep -E '^\s*add_subdirectory' apps/CMakeLists.txt | grep -v '^\s*#'  # 4. 활성 빌드 타깃
ls ~/.claude/plugins/cache/*/                                      # 5. superpowers/ddd/context7
ls /tmp/cek-ddd/plugins/ddd/rules/ 2>/dev/null || echo "ddd 미캐시"   # 6. ddd rules 캐시
```

---

## 11. Case Study — OpenGL ECS Rendering Architecture (SP1~SP4)

*living documentation*:
- `doc/superpowers/specs/2026-05-20-sp1-shader-program-resource-consolidation-design.md`
- `doc/superpowers/specs/2026-05-21-sp2-render-context-design.md`
- `doc/superpowers/specs/2026-05-21-sp3-ecs-render-system-design.md`
- `doc/superpowers/plans/2026-05-20-sp1-shader-program-resource-consolidation.md`
- `doc/superpowers/plans/2026-05-21-sp2-render-context.md`
- `doc/superpowers/plans/2026-05-21-sp3-actor-component-render-system.md`

### 11.1 핵심 결정 누적
- **SP1** (5 Task ✅): 자원 정리 + RAII `= delete` 명시
- **SP2** (6 Task ✅): DeviceContext 싱글톤 + Pattern Y (cache in resource member, NOT centralized server)
- **SP3** (11 Task 🟡 plan 완료): Cocos2D 1차 + Unreal 2차 — Actor+Component + MeshPassProcessor + PropertyBlockSetter
- **SP4** (⚪ 미착수): FrameBuffer + post-processing

### 11.2 ddd 검증 반복 사례
- SP1: 1차 only (단순) / SP2: 2차 (Renderer-centric vs Pattern Y) / SP3: 3차 (초기 → 5 fix 패치 → namespace/class 충돌 fix)

### 11.3 엔진 벤치마크 사례
- SP2 cache lifecycle: 4엔진의 Pattern X(centralized) vs Pattern Y(resource-attached) 분류 → Pattern Y 채택
- SP3 lifecycle 명명: Cocos `onEnter/onExit/update` 1차 + Unreal `SetTickEnabled` 2차 영감

---

## 12. 인계 시 보존해야 할 *암묵적 합의*

다음은 spec/plan 에 *명시되지 않을 수 있지만* 세션 전체에 흐르는 약속:

1. **Pattern Y 우선** — 자원 클래스는 *자기 캐시* 보유 (TU-local static 회피)
2. **Cocos 정통 명명** — namespace `Scene`, lifecycle `OnEnter/OnExit/Update`
3. **Unreal 영감만 부분 흡수** — `SetEnabled` 토글, *Tree of components* (`USceneComponent`) 는 채택 안 함
4. **Unity 식 templated API** — `AddComponent<T>()` 채택, Cocos 의 *name string lookup* 은 격하
5. **싱글톤 패턴 통일** — Meyer's singleton + 4종 `= delete` + `static Get()`
6. **한 호출 = 한 의미** — 단, factory + AddXxx 의 *construct + lifecycle* 통합은 *명시적 doxygen contract* 로 honest 화
7. **`mEntered` symmetric guard** — Add(Child/Component) 의 *즉시 OnEnter* 와 Remove 의 *조건부 OnExit* 가 항상 대칭
8. **Out of Scope 항상 명시** — *영구 미지원* 항목(예: `OnEnable`/`OnDisable` transition hook)까지 명시
9. **진실의 원천 단일화 (SSOT)** — 동일 의미 두 변수를 *분리해서 모두 public* 노출 금지. 한쪽이 *진실* 이면 다른 쪽은 private + getter 로 *도출* (예: `Material.PassKind` private + `GetQueueLayer()` = `Pass::QueueOf(mPassKind)`). Filament/Unreal/Cocos 정통.
10. **직교 축 분리** — *"의도 분류"*(Material.Pass.Kind)와 *"인스턴스 미세조정"*(MeshRenderer.QueueOffset)처럼 *축이 다른* 두 변수는 공존 가능. 같은 축의 두 변수만 통합 대상.
11. **기본값의 무게** — Pass.Kind 별 7 GL state 처럼 *분류 한 줄로 정상 case 대부분 자동 처리*. override 는 *예외 case* 만. Unity Standard Shader / Filament `MaterialBuilder` 정통.
12. **증상 우회 vs 원인 처리** — 같은 증상에 *4단계 fix 진화* (sort 부호 반전 → sentinel → 직교 축 추가 → 캡슐화)가 보이면 *원인이 위 계층*. 시각 디버깅 — GL 파이프라인 단계 추적(vertex → raster → fragment → depth → cull → blend → write).
13. **Retina HiDPI = physical framebuffer 기준** — `glViewport`/FBO/Camera.Aspect 는 *physical*. sb7 `onResize(int,int)` 는 *logical* → `glfwGetFramebufferSize` 로 변환. `render()` 매 프레임 갱신으로 resize 콜백 누락 가드. (`extern/sb7code` 수정 금지 — 패턴은 챕터 측.)

이 13가지가 *세션의 무형 자산*. 이 문서가 보존하는 *진짜 핵심*.

---

## 13. 도구 호출 빠른 참조

```
브레인스토밍:  Skill superpowers:brainstorming
플랜 작성:     Skill superpowers:writing-plans
구현:          Skill superpowers:subagent-driven-development
디버깅:        Skill superpowers:systematic-debugging
완료:          Skill superpowers:finishing-a-development-branch

설계 가치관 Skill (전역):
  design-decision-discipline / code-design-review-lenses / architecture-design-workflow
  / benchmark-research-method / agent-orchestration-anti-gaming

Context7 조회 (deferred):
  ToolSearch query="select:mcp__plugin_context7_context7__resolve-library-id,mcp__plugin_context7_context7__query-docs"

ddd rules clone:
  Bash: git clone --depth 1 https://github.com/NeoLabHQ/context-engineering-kit.git /tmp/cek-ddd

Subagent 디스패치:
  Implementer:           Agent subagent_type=general-purpose, model=sonnet (또는 haiku)
  Spec reviewer:         Agent subagent_type=general-purpose, model=sonnet
  Code quality reviewer: Agent subagent_type=superpowers:code-reviewer
```

---

## 14. 마무리 — 이 문서의 사용법

- **인수 시**: 본 문서(프로젝트 종속분) + `architecture-design-workflow` Skill(방법론) 같이 읽기 → §10 30초 명령 → 최근 spec/plan 1~2개 → 현재 위치+다음 단계 1문장 보고 → confirm 후 진입.
- **새 프로젝트 적용 시**: 방법론은 Skill 로 충분. 이 문서의 §11(case study)·§12(암묵적 합의)는 이 프로젝트 전용이라 무시.
- **갱신 책임**: 새 프로젝트 합의 발견 → §12 추가. 범용 방법 변화 → Skill 갱신.

**최종 갱신**: 2026-06-18 (범용 방법론 Skill 이관 — 프로젝트 종속분만 잔존)
