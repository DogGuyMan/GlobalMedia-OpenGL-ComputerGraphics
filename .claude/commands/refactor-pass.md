---
description: src/<module>/ 또는 챕터의 안전한 리팩토링을 5-에이전트 운영 분리로 실행
argument-hint: <target-path> "<refactoring-instruction>"
---

# Refactor Pipeline — 5-Agent

사용자가 인자로 지정한 대상 파일/디렉토리와 리팩토링 지시서를 받아, D 논문의 5-에이전트 운영 분리 구조로 안전한 리팩토링을 실행한다.

본 저장소는 RHI 추상화 / render pass 구조가 아니라 **`src/<module>/` STATIC 라이브러리 11개 + `apps/<chapter>/` 한 번에 하나만 활성** 구조다. 리팩토링 대상은 보통 다음 중 하나:

| 대상 유형 | 예시 |
|---|---|
| `src/<module>/<file>.cpp` | `src/buffer/buffer.cpp`, `src/program/program.cpp`, `src/diagnostics/gl_log.cpp` |
| `src/<module>/` 전체 | `src/object/` (Mesh/Model/Material/Camera/Light) 같은 응집 도메인 |
| `<apps>/chapterN/main.cpp` | 단일 챕터의 sb7::application 본체 |
| `test/test_<x>.cpp` | 단일 Catch2 테스트 파일 |

## 인자
- `$1`: 대상 경로 (예: `src/program/program.cpp` 또는 `src/object/`)
- `$2`: 리팩토링 지시서 (자연어, 예: "uniform 캐시를 program 외부 TU-local static 에서 멤버로 이전")

## 실행 절차 (Process.sequential)

### Step 1: render-architect 호출
- 입력: `$1`, `$2`
- 출력: `doc/analysis/<timestamp>/architecture.md`
- 파일 수정 권한 없음 (도구가 Read/Grep/Glob)
- **사용자 검수 게이트**: 아키텍트 보고서를 사람이 30초 훑어보고 OK 해야 다음 단계

### Step 2: render-refactorer 호출 (worktree 격리)

```bash
# worktree 생성 — 본 저장소는 .worktrees/ 디렉토리에 격리 (.gitignore 처리됨)
BRANCH="refactor-$(date +%s)"
git worktree add .worktrees/$BRANCH HEAD -b $BRANCH

# 골든 디렉토리가 존재한다면 read-only (현재 미존재)
[ -d .worktrees/$BRANCH/test/golden ] && chmod -R a-w .worktrees/$BRANCH/test/golden
```

- 입력: architecture.md + 측정 가능 기준
- A 논문의 PromptSmith Compilation Loop 따름:
  - max outer 6, inner 8 iterations
  - failure 수 < 10 이면 FOCUSEDLOOP, 이상이면 EDITPROMPT
- 빌드 검증: `cmake --preset ninja && cmake --build --preset ninja --target <대상 모듈/챕터>`
- 출력: 변경된 파일 + 빌드 결과

### Step 3: render-test-debug 호출
- 입력: 변경된 worktree
- Catch2 단위 테스트: `ctest --test-dir .worktrees/$BRANCH/build_ninja --output-on-failure` (test/ wiring 후)
- (선택) 골든 이미지 비교: `test/golden/` 존재 시 FLIP 또는 ImageMagick `compare`
- (선택) `gl_state_snapshot` diff: `test/support/gl_state_snapshot` 으로 GL 상태 회귀 확인
- 출력: PASS/WARNING/FAIL

### Step 4: render-quality-gate 호출
- 입력: 변경된 worktree (test-debug PASS 인 경우만)
- 게이트: 빌드 + ctest + clang-tidy (compile_commands.json) + (선택) cppcheck + (선택) mutation
- 출력: 종합 보고서

### Step 5: render-pm 호출
- 입력: 위 4개 에이전트 결과 모두
- 의사결정 매트릭스로 MERGE / MERGE+WARN / HOLD / REVERT 결정
- 출력: 최종 결정 + 머지/롤백 실행

## 절대 금지 (`doc/testplan/Graphics-Testing-Prompt.md` 따름)

- 1-2 라운드 피드백으로 끝내지 마라 (B 논문 Table 9: 25% 악화). No-feedback 또는 3+ rounds 둘 중 하나만.
- 같은 세션에서 테스트와 코드를 함께 만들지 마라 (A §7).
- Mutation Score 100% 무비판 신뢰 금지 (A §8).
- 같은 파일 3회 이상 수정하면 사람에게 escalate (A §6.3 oscillation).

## 비용 견적

A 논문 §6.5 기준 **PR당 약 $5 / 1-2시간** (C++ 빌드 시간 포함). hooks 의 hard cap 이 $10 초과 시 강제 종료.

## 사용 예시

```
/refactor-pass src/program/program.cpp "uniform 캐시를 program 외부 TU-local static 에서 Program 멤버로 이전"
/refactor-pass src/object/ "Light 위치/방향을 SceneNode Transform 에서 읽도록 분리 — Light 자체는 색상/감쇠만"
/refactor-pass <apps>/chapter8/main.cpp "멀티 라이팅 uniform 집합을 namespace LightingUniforms 로 묶음"
```
