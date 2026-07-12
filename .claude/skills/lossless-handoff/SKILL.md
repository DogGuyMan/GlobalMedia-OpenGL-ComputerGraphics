---
name: lossless-handoff
description: >-
  Use when delegating a unit of work to a separate Claude Code agent/session, or preserving
  conversation context so a fresh session can resume with zero loss. Produces either (A) a
  paste-ready, self-contained agent prompt for one bounded task, or (B) a lossless resume/context
  handoff document for a multi-step effort. Trigger this whenever the user mentions 핸드오프/handoff,
  "다른 (Claude) 에이전트에게 넘겨/맡겨", "프롬프트 만들어/추출", "무손실", "컨텍스트 저장/이주", "재개",
  "병렬로 빼줘/나눠", delegating to a parallel agent, preserving session context before a long task,
  or migrating work to another session — even if they never say the word "handoff". Especially
  important in multi-agent / parallel-branch setups where scope boundaries and conflict-avoidance matter.
---

# Lossless Handoff

## Why this exists

인수하는 에이전트(또는 새로 시작하는 재개 세션)는 이 대화의 맥락을 전혀 갖고 있지 않다. 코드를 조사하고,
옵션을 저울질하고, 사용자의 수정 지시를 받아들이는 과정을 지켜보지 못했다. 그래서 핸드오프는 공유 기억을
전제하는 순간 실패한다 — "우리가 논의했던 접근법" 같은 허공에 뜬 참조, 어긋난 파일 경로, 언급되지 않은
가드레일이 그 예다. 좋은 핸드오프는 **자기완결적(self-sufficient)**이다: 수신자는 핸드오프 하나만으로
작업을 정확하고 안전하게 수행할 수 있어야 한다.

이 스킬은 작업이 에이전트/세션 경계를 넘을 때마다 반복적으로 필요해지는 두 가지 산출물을 정리한다:

- **A — 자기완결형 에이전트 프롬프트**: 다른 Claude Code 에이전트가 **하나의 한정된 작업**을 수행하도록
  그대로 붙여넣을 수 있는 블록. "복사 → 새 세션 → 실행"에 최적화되어 있다.
- **B — 재개/컨텍스트 핸드오프 문서**: 새 세션이 **다단계 작업**을 이어받게 해주는, 추적되는 문서(또는
  세션이 끝나도 아무것도 잃지 않도록 긴 작업 전에 미리 작성하는 문서).

둘 다 산출하는 경우도 많다: B는 작업 전체를 보존하고, A는 그중 일부를 떼어내 위임한다.

## Step 0 — Discover the target project's conventions (do this first, every time)

핸드오프는 수신자가 지켜야 할 규칙을 담고 있을 때만 안전하다. 이 규칙들은 **프로젝트마다 다르므로**
추측으로 채울 수 없다 — 직접 찾아서 핸드오프에 박아 넣어야 한다. 다음을 확인하는 데 잠깐 시간을 들인다:

- **빌드/실행/테스트 명령** — 정확한 검증 명령(예: `cmake --build --preset …`, `npm test`). 수신자는
  이 명령으로 검증한다. "빌드되는지 확인해" 같은 막연한 지시로는 부족하다.
- **커밋 정책** — 사용자가 커밋을 게이트하는가? 커밋 메시지 형식이 있는가? 트레일러 컨벤션(일부
  프로젝트는 `Co-Authored-By`를 의도적으로 생략한다)은? 브랜치 규칙은? 불확실하면 기본값은 "구현 + 보고,
  커밋은 하지 않는다" — 승인되지 않은 커밋은 되돌리기 어렵다.
- **테스트 정책** — TDD를 기대하는가, 아니면 요청 시에만 테스트를 작성하는가? (이를 명시하라. TDD를
  거부하는 프로젝트에 TDD를 강요하지 말 것.)
- **코드 컨벤션** — 주석 언어, 헤더가드 스타일, 명명 규칙, 금지된 구문.
- **병렬 에이전트 지형** — 수신자가 이 브랜치에서 혼자 작업하는가? 다중 에이전트 환경이라면 누가 무엇을
  편집 중인지(최근 커밋, `git status`, 다른 핸드오프 문서)를 파악한다. 이는 아래의 충돌 매트릭스(conflict
  matrix)로 이어진다. 이 단계를 건너뛰는 것이 핸드오프 충돌의 1순위 원인이다.

이런 사실은 `CLAUDE.md` / `AGENTS.md`, 최근 `git log`, 그리고 근처의 다른 핸드오프 문서들을 읽어서
확보한다. 컨벤션을 정말 알 수 없고 사용자에게 물어볼 수 있다면, 지어내지 말고 물어본다.

## The five principles (these are what make a handoff "lossless")

1. **Self-containment(자기완결성).** 수신자에게 필요한 사실은 핸드오프 안에 인라인으로 넣는다 — 시작하기
   위해 다른 문서 세 개를 읽게 만들지 않는다. 참조 스펙은 어디까지나 선택적 심화 자료이지 필수
   전제조건이 아니다. (어떤 사실이 수신자가 열어볼 수 있는 파일에 있다면 포인터만으로 충분하다 — 다만
   핵심 경로만큼은 핸드오프 자체에 있어야 한다.)

2. **Grounding(실측 기반) — 믿지 말고 검증하라.** 모든 파일 경로, 시그니처, 줄 번호, "현재 상태" 주장은
   기억에서 끄집어내거나 이전 핸드오프에서 그대로 베끼지 말고, 핸드오프를 작성하는 시점에 실제 코드를
   다시 대조해야 한다. 저장소는 드리프트한다(이름 변경, 병렬 커밋). 상태 드리프트 = 수신자가 거짓 전제
   위에서 작업하게 된다는 뜻이다. 작성 직전에 `git log`/`git status`를 다시 측정하고, `file:line`을
   인용한다. 수신자에게도 재검증을 지시한다("저장소 이름이 바뀌는 중이니 — include 전에 실제 파일명을
   확인하라").

3. **Scope boundaries(소유 경계) + conflict matrix(충돌 매트릭스).** 수신자가 무엇을 소유하고 무엇을
   건드리면 안 되는지(그리고 왜인지 — "그 파일은 병렬 작업 X가 소유한다") 명시적으로 말한다. 다중
   에이전트 환경에서는 작은 소유권 표(파일 × 담당자)가 산문보다 충돌 방지에 훨씬 효과적이다.
   `git add -A` 대신 경로 범위를 좁힌 커밋을 선호해서, 수신자가 다른 에이전트의 작업을 실수로
   쓸어담지 않게 한다.

4. **Guardrails(가드레일).** 기본적으로 위험한 규칙들을 그대로 전달한다: 승인되지 않은 커밋 금지,
   프로젝트 컨벤션, "동결된 의존성은 수정하지 말 것", 손대야 한다면 최소한으로만 건드려야 할 분쟁
   파일들. 수신자가 기계적 순응이 아니라 판단을 적용할 수 있도록 이유(why)를 함께 담는다.

5. **발신 측 Hygiene(정리 규율).** 어떤 결정이 이전 핸드오프를 대체하게 되면, 서로 모순되는 문서 두 개를
   그대로 남겨두지 말고 옛 문서에 표시("🔴 superseded → see X" 배너)를 남긴다. 메모리/인덱스 포인터도 새
   진입점으로 이전한다. gitignore된 경로에 유의한다(로컬 전용 스펙은 다른 머신으로 옮겨가지 않는다 —
   이를 명시하거나, 지속되어야 할 핸드오프는 추적되는 디렉터리에 둔다).

## Choosing A vs B (or both)

```
지금 당장 실행할 하나의 한정된 작업(ONE bounded task)을 위임하는 것인가?      → A (agent prompt)
다단계 작업/세션 컨텍스트를 보존하는 것인가?                                  → B (resume handoff)
다단계 작업이면서 그중 일부를 떼어내 위임하는 것인가?                          → B for the whole + A for the slice
다른 에이전트가 요청한 결정/답변인가?                                         → a short decision-response doc (A-style: ruling + rationale + boundary)
```

## Artifact A — Self-contained agent prompt

수신자는 펜스 블록 하나를 새 세션에 붙여넣는다. 낯선 사람도 실행할 수 있도록 구조화한다. 전체 주석이
달린 뼈대(skeleton)는 **`references/agent-prompt-template.md`** — 작성할 때 이것을 읽는다. 구성은 다음과
같다:

- `[ROLE]` — 한두 줄로: 에이전트가 누구인지, 프로젝트/경로/브랜치, 단일 목표. 작업이 의도적으로 미완성
  상태로 인도되는 경우(예: "unwired — correct by construction") 앞에서 밝힌다.
- `[Hard rules]` — 커밋/테스트 정책 + 코드 컨벤션(Step 0에서 확보한 것).
- `[Verified facts]` — 수신자에게 필요한, 실측된 `file:line` 사실들(시그니처, 따라야 할 기존 패턴, 현재
  배선 상태). 드리프트 위험이 있는 곳에는 "이 보고를 믿지 말고 재검증하라"를 남긴다.
- `[STEP n]` — 정확한 파일 + 완전한 코드("적절한 X를 추가하라" 식이 아니라). 실제 코드를 보여준다.
- `[Boundaries]` — 이 파일들은 소유함 / 이 파일들은 절대 건드리지 않음(+ 이유). 병렬 에이전트 관련 메모.
- `[Verify]` — 정확한 빌드/실행 명령 + 기대 출력; "완료 & 정확함"이 어떤 모습인지.
- `[Self-review]` — 보고하기 전에 에이전트가 돌리는 짧은 체크리스트.
- `[Report]` — 구조화된 상태를 요청한다: DONE / DONE_WITH_CONCERNS / BLOCKED + 변경된 파일 + 검증 결과 +
  미룬 사항. (구조화된 보고여야 돌아왔을 때 trust-but-verify가 가능하다.) 커밋이 게이트되어 있다면
  "커밋하지 말 것"을 재차 명시한다.

사용자가 깔끔하게 복사할 수 있도록 전체를 ``` 펜스 하나로 감싼다. 오케스트레이터를 위한 메모(조율, 권장
커밋 메시지)는 펜스 바깥, "Notes" 제목 아래에 둔다.

## Artifact B — Resume / context handoff doc

재개를 위한 단일 진입점이 되는, 추적되는 마크다운 문서. 전체 주석이 달린 뼈대는
**`references/resume-handoff-template.md`**. 구성은 다음과 같다:

- **TL;DR + next action** — 현재 위치를 한눈에, 그리고 바로 다음에 할 구체적인 한 걸음.
- **State of the world, re-measured** — 브랜치, `git log`(최근 커밋과 각각이 한 일), 커밋된 것과 안 된
  것. (작성하면서 다시 실행할 것 — 낡은 상태를 그대로 붙여넣지 말 것.)
- **Task / step status table** — done / in-progress / pending, 커밋 SHA와 함께.
- **Locked decisions** — 무엇이 확정되어 다시 논쟁하면 안 되는지(그리고 이전에 확정된 주장을 정정한 경우
  그 이유까지).
- **Parallel-track conflict matrix** — 누가 어느 파일을 소유하는지, 병렬로 안전하게 할 수 있는 것은
  무엇인지, 순서는 어떻게 되는지.
- **Guardrails & conventions** — Step 0에서 그대로 가져온 것.
- **Pointers** — 스펙/플랜 위치(gitignore된 것은 표시), 그리고 이 문서가 어떤 문서를 대체하는지.
- **(유실 가능성이 있다면) verbatim recovery** — 리셋으로도 파괴되지 않도록 커밋되지 않은 핵심 코드를
  그대로 붙여넣는다.
- **Change log** — append-only(추가만 함), 다음 독자가 상태가 어떻게 변해왔는지 볼 수 있도록.

B를 작성할 때는 hygiene(원칙 5)도 함께 수행한다: 대체된 핸드오프에 배너를 달고, 메모리 포인터를 이
문서로 이전한다.

## Anti-patterns to avoid

- **Assumed context(전제된 맥락)** — "우리가 논의한 접근법을 계속해줘." 수신자는 그 자리에 없었다. 다시
  명시하라.
- **Stale grounding(낡은 실측)** — 재확인 없이 이전 핸드오프의 줄 번호/경로/상태를 그대로 재사용. 병렬
  커밋과 이름 변경이 소리 없이 이를 무효화한다.
- **Vague verification(모호한 검증)** — "잘 되는지 확인해." 명령어와 기대 출력을 제시하라.
- **No boundaries in a shared repo(공유 저장소에 경계 없음)** — 두 에이전트가 서로의 작업을 덮어쓰게
  만드는 가장 빠른 길.
- **핸드오프에서 `git add -A`** — 무관하거나 병렬로 진행 중인 작업까지 커밋에 쓸어담는다. 경로 범위를
  좁힐 것.
- **Orphaned superseded docs(고아가 된 대체 문서)** — 서로 모순되는 핸드오프 두 개가 남아 있는데 어느
  쪽이 우선인지 아무도 말해주지 않는 상태.
- **선택적 읽을거리로 프롬프트 과적재** — 수신자가 링크된 스펙을 반드시 읽을 필요는 없어야 한다. 핵심
  경로는 인라인으로 유지하고, 나머지는 "더 깊이 알고 싶다면 열어보라" 정도로 둔다.
- **수신자에게 플랜 파일을 읽도록 강제** — 해당 조각을 프롬프트 안으로 추출하라(self-containment).

## Where to put handoffs

지속되어야 할 핸드오프(B, 그리고 살아남기를 원하는 모든 프롬프트)는 gitignore된 디렉터리가 아니라 git으로
추적되는 디렉터리(예: `doc/handoff/`)에 둔다 — 그렇지 않으면 다른 머신/세션에 도달하지 못한다. 파일명에
날짜를 찍어 최신 버전이 명확하게 드러나게 한다. 두 산출물 각각의 참조 템플릿은 `references/`에 있다.
