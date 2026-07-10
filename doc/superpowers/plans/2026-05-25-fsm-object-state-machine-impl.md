# FSM Object State Machine — 구현 완료 노트

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **Status: COMPLETED** (2026-05-25)
> 원래 plan: Subagent-Driven Development 패턴으로 실행 → 실행 중 design 진화 → 사용자 review 로 추가 정리 → 최종 commit `3bdf499`
> 최신 design: [`../specs/2026-05-25-fsm-object-state-machine-design.md`](../specs/2026-05-25-fsm-object-state-machine-design.md)

## 완료된 commits (시간 순)

| Commit | 메시지 | 산출 |
|---|---|---|
| `78dea2a` | `feat(fsm): SJH::fsm 코어 모듈 신설 + StateMachineProcessor template` | `<src>/fsm/state_machine_processor.h` (P1, switch-on-enum, **이후 폐기**) |
| `16a6cdd` | `feat(fsm): ObjectStateMachine + IFsmState 추가 (M2 P1.5)` | `<src>/fsm/i_fsm_state.h` + `<src>/fsm/object_state_machine.h` (Stage 2 — State 객체화) |
| `6f5346c` | `refactor(fsm): 파일/클래스 명 정리 + P1 StateMachineProcessor 폐기` | rename 2건 + Stage 1 폐기 (test_fsm.cpp 동반) |
| `3bdf499` | `refactor(fsm): TTransit 제거 + GetTransitFlag 기반 그래프 정보 응집` | TTransit template parameter 제거 + targetOf_ map 제거 + State self-transitioning |

## 최종 산출 파일 (working tree)

```
src/fsm/
├── CMakeLists.txt       (M2 P1 그대로 — INTERFACE library)
├── fsm_state.h          IFsmState<TOwner> (Stage 4 시점, GetStateFlag/GetTransitFlag 보유)
└── state_machine.h      StateMachine<TState, TOwner> (Stage 4 시점, TTransit 없음)
```

## 원래 plan 의 step 들 — Subagent-Driven Pipeline 으로 실행됨

원본 plan 은 Task 1~4 (헤더 2개 작성 + 빌드 가드 + commit) 의 step-by-step 가이드였다. 다음 pipeline 으로 실행:

1. **Implementer subagent** (general-purpose) — Task 1+2 합쳐 헤더 2개 작성 → DONE
2. **Spec compliance reviewer** (general-purpose) — ❌ 보고했으나 *controller (나) 의 reviewer prompt 축약 실수* 에 의한 false positive → controller 가 plan ↔ 실제 파일 직접 비교 후 PASS 판정
3. **Code quality reviewer** (superpowers:code-reviewer) — *Approved with notes*. **Issue #1 critical 발견**: NONE 시작 trap (spec 의 사용 예제가 첫 실행에서 abort 했을 design bug). Issue #2/#3 doc-only
4. **Issue fix subagent** — 생성자에 `startup` 인자 + Issue #2/#3 doxygen 추가
5. **Controller 직접** — 빌드 회귀 가드 (8/8 PASS) + commit `16a6cdd`

## 실행 중 design 진화 — plan 범위 초과 사건

원래 plan 은 *헤더 2개 추가* 만으로 완료될 예정이었으나, 사용자가 *3가지 추가 통찰* 을 제기하며 design 가 진화:

### (a) 사용자 제기: "FsmState.h 새로 만들었음" → rename + GetStateFlag/GetTransitFlag 추가
- 사용자가 직접 파일 작성 (`FsmState.h` 임시 → `fsm_state.h` 로 정착)
- 본래 `i_fsm_state.h` 와 헤더 가드 충돌 → 정리 필요
- Stage 3 commit (`6f5346c`) 으로 흡수

### (b) 사용자 제기: "state_machine_processor.h 도 반드시 필요한가?"
- grep 결과 사용처 = `test_fsm.cpp` 자기 자신뿐 (프로덕션 0)
- spec 의 "Timer 류 공존" 정당화가 *방어적 정당화* 였음 controller 인정
- Stage 3 commit (`6f5346c`) 에서 폐기

### (c) 사용자 제기: "GetTransitFlag 로 자가 검증 + targetOf_ 제거 가능"
- 그래프 정보를 *State 클래스 안에* 응집 (DDD Entity within Aggregate 정통)
- TTransit template parameter 완전 제거
- Stage 4 commit (`3bdf499`)

## Learning — Subagent-Driven Development 회고

**잘 작동한 것**:
- *fresh subagent per task* 패턴이 *implementer 가 plan 의 코드를 정확히 옮기게* 강제 (drift 0)
- *code quality reviewer* 가 *진짜 design bug* (NONE 시작 trap) 를 발견 — 사람 review 가 놓쳤을 가능성 큰 case
- Issue fix → 새 dispatch 패턴이 명료

**개선 필요**:
- *Spec compliance reviewer prompt* 작성 시 controller 가 expected 코드를 *축약하면* false positive 발생 → reviewer 에게는 *plan 원본을 그대로* 인용해야 함. 본 세션에서 한 번 학습.
- *plan 작성 시점 design 이 최종 design 이 아닐 수 있음* → plan 은 *spec 의 한 단면* 일 뿐. 사용자 통찰이 design 을 진화시키는 것을 *환영* 해야지 *plan 위반* 으로 간주하지 말 것.

## 다음 plan 작성 시 (M2 P2 등)

본 plan 의 *원본 step* 은 commit history (`78dea2a` → `3bdf499`) 와 spec 의 §"진화 History" 에 더 정확히 기록되어 있다. 본 파일은 *완료 보고서* 로만 유지.

M2 P2 작업 시 본 spec 의 §"사용 패턴 (M2 P2 미리보기)" 코드 그대로 시작 가능 — 모든 design 결정이 *현재 design 정본* 에 통합됨.

## 변경 기록

| 일자 | 변경 |
|---|---|
| 2026-05-25 (초안) | Stage 2 (M2 P1.5) 의 step-by-step plan 작성 |
| 2026-05-25 (1차 갱신) | 4-stage 진화 완료 후 완료 노트로 단순화 — original step 은 commit history 위임 |
