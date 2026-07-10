# M2 Parallel Dispatch Prompts

> ⚠ 2026-05 시점 archival 문서 — 경로가 현재 코드베이스와 불일치할 수 있음 (예: `test/` 는 2026-06 이후 구조 재편, `InputHandler/CameraController` 는 리팩토링으로 개명/제거됨).

본 디렉토리는 [`../2026-05-24-M2-fsm-player-follow.md`](../2026-05-24-M2-fsm-player-follow.md) M2 plan 의 *병렬 dispatch 가능한 3개 task* 의 prompt 를 *각각 별도 self-contained markdown 파일* 로 추출한 것.

## 파일 구성

| 파일 | Task | 영역 | 예상 시간 |
|---|---|---|---|
| [P1-fsm-module.md](P1-fsm-module.md) | `SJH::fsm` 코어 모듈 + 단위 테스트 | `src/fsm/`, `src/CMakeLists.txt`, `test/test_fsm.cpp`, `test/CMakeLists.txt` | 15~25분 |
| [P2-player-controller-follow.md](P2-player-controller-follow.md) | PlayerController + Camera follow + main.cpp 정리 | `apps/_MyApp_/src/InputHandler/`, `apps/_MyApp_/main.cpp` | 30~50분 |
| [P3-claude-md-update.md](P3-claude-md-update.md) | `.claude/CLAUDE.md` Active Target Management 갱신 | `.claude/CLAUDE.md` | 3~5분 |

**충돌 영역 0** — 3 파일 모두 동시 dispatch 안전.

## 사용 방법

### 방법 A — Claude Code Agent tool (controller 가 multi-tool 호출)

controller 가 single message 에 multi-tool block 으로 Agent 3개 동시 호출. 각 Agent 의 `prompt` 인자에 P1/P2/P3 md 파일의 `## Prompt` 섹션 *전체 내용* 을 그대로 복사:

```
Agent(description="P1 fsm module", subagent_type="general-purpose", prompt=<P1.md 의 ## Prompt 이하 전체>)
Agent(description="P2 player + follow", subagent_type="general-purpose", prompt=<P2.md 의 ## Prompt 이하 전체>)
Agent(description="P3 CLAUDE.md", subagent_type="general-purpose", prompt=<P3.md 의 ## Prompt 이하 전체>)
```

### 방법 B — 별도 Claude Code 세션 (다른 사람/터미널)

각 P1/P2/P3 md 파일을 *별도 Claude Code 세션* 에서 *첫 메시지* 로 붙여넣기. 세션마다 다른 working tree (git worktree) 도 가능 — 단 본 plan 은 *같은 working tree* + *충돌 영역 0* 전제라 세션 분리만으로도 OK.

### 방법 C — 외부 LLM (다른 회사 AI)

각 P1/P2/P3 md 가 *self-contained* — Project conventions, 사전 정독 파일, 코드 인라인, 빌드 명령, commit 메시지, Self-Review 체크리스트 모두 포함. 외부 LLM 에 그대로 전달해도 작동 (단 *외부 LLM 이 read/edit 도구* 보유 필요).

## Dispatch 후 review pipeline

3 implementer 모두 DONE 후:

1. **spec compliance review** — 각 task 별 1 subagent (총 3개)
2. **code quality review** — 각 task 별 1 subagent (총 3개)
3. 모든 review PASS 후 M2 완료

review subagent prompts 는 본 plan 에 인라인 없음 — controller 가 spec-reviewer / code-quality-reviewer prompt 템플릿 사용 (`superpowers:subagent-driven-development` 스킬).

## 의존성 (외부 컨텍스트)

각 prompt 가 *자동 인지* 하는 것:
- `.claude/CLAUDE.md` (project-level) — cwd 기반 자동 로드 ✓
- `extern/sb7code/include/sb7.h` 등 라이브러리 헤더 — Read 도구로 접근 ✓

각 prompt 가 *명시 전달* 받는 것:
- spec §6.0.2 코드 (P1) — prompt 안에 인라인
- compound_actor 비상속 컨벤션 (P2) — memory 인용 prompt 안에 인라인
- 현 `apps/_MyApp_/main.cpp` 의 위치 — prompt 안에 line 번호 명시

각 prompt 가 *Read 도구로 직접 정독* 권장:
- P1: `src/scene/actor.h:27-47`, `src/sprite/CMakeLists.txt`, `<test>/test_uniform_atlas.cpp`, `test/CMakeLists.txt:222-243`
- P2: `src/input/keyboard_input.h` (필수), `<apps>/_MyApp_/src/InputHandler/CameraController.h/.cpp` (필수), `apps/_MyApp_/main.cpp`, `src/scene/actor.h:37`
- P3: `.claude/CLAUDE.md` (전체), `apps/CMakeLists.txt:1-6`

## 변경 기록

| 일자 | 변경 |
|---|---|
| 2026-05-24 | M2 plan + 3 prompt 파일 초안 작성 |
