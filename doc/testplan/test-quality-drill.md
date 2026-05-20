# Test Quality Drill — 사보타지 드릴 운영

> 본 문서는 [2026-05-07-gl-state-and-test-quality-design.md](2026-05-07-gl-state-and-test-quality-design.md) §6.2 의 운영 산출물.
> 5개 컴포넌트 × 3개 사보타지 = **총 15회 드릴**의 운영 절차 + 살아있는 표.

## 1. 개념

"테스트 통과"가 진짜 안전을 의미하는지 *적대적으로* 검증. 컴포넌트마다 3개의 그럴듯한 사보타지를 손으로 적용하고 ≥1 케이스 FAIL을 강제한다.

근거:
- [testing-curriculum.md](testing-curriculum.md) §부록 A.5 (적대적 사고)
- A 논문 §8 (Mutation Score caveat) — 본 spec [§6.3](2026-05-07-gl-state-and-test-quality-design.md) 참조
- [bug-coverage-audit.md](bug-coverage-audit.md) §4 — 현재 plan이 잡지 못하는 카테고리 명시

## 2. 언제 실행하나

| 트리거 | 빈도 | 의무성 |
|---|---|---|
| 신규 테스트 인프라 컴포넌트 머지 직전 | 컴포넌트 1개당 1회 | **필수** |
| 분기별 정기 — 기존 컴포넌트 대상 | 3개월마다 | 선택 |
| production 회귀가 *기존 테스트를 통과한 채로* 슬립 | 그 컴포넌트 즉시 | **의무** |
| Plan 결함 발견 시 (예: N10/N11) | 그 결함이 가리키는 컴포넌트 | **필수** |

## 3. 절차 (4 step)

```bash
# 1. 안전 상태 확보
git status                                      # clean working tree 확인
git stash --include-untracked                   # 안전 보존

# 2. 한 사보타지씩 손으로 적용
$EDITOR src/diagnostics/gl_state_fields.cpp     # 표의 사보타지 1번을 직접 적용
cmake --build build_Darwin -j --target tests    # N1 함정 회피 — --target tests 명시
ctest --test-dir build_Darwin --output-on-failure
# → 결과 기록 (어느 케이스가 FAIL했는지)

# 3. 복원
git checkout -- src/diagnostics/gl_state_fields.cpp
ctest --test-dir build_Darwin --output-on-failure   # 복원 검증 (모두 PASS 회복)

# 4. 컴포넌트별 표 갱신 (다음 섹션의 링크된 파일)
$EDITOR doc/testplan/test-quality-drill/gl_state_capture.md
git add doc/testplan/test-quality-drill/gl_state_capture.md
# git commit -m "test: drill record for CaptureGLState (sabotage 1/3)"  # 사용자 명시 요청 시
```

## 4. 컴포넌트별 표 (살아있는 문서)

각 컴포넌트의 사보타지 표는 *분리 파일*로 관리 — diff 노이즈 최소화 + git blame 친화 + 컴포넌트 독립 평가.

| 컴포넌트 | 표 파일 | 출처 | 사보타지 수 | 마지막 드릴 |
|---|---|---|---:|---|
| CaptureGLState | [test-quality-drill/gl_state_capture.md](test-quality-drill/gl_state_capture.md) | Task 2 | 3 | (드릴 후 갱신) |
| Diff | [test-quality-drill/diff.md](test-quality-drill/diff.md) | Task 6 | 3 | (드릴 후 갱신) |
| SymbolicName | [test-quality-drill/symbolic_name.md](test-quality-drill/symbolic_name.md) | Task 1 + N11 | **4** | (드릴 후 갱신) |
| GLStateSnapshot::ToString | [test-quality-drill/snapshot_tostring.md](test-quality-drill/snapshot_tostring.md) | Task 5/6 | 3 | (드릴 후 갱신) |
| **Smell Linter** *(N10 후속)* | [test-quality-drill/smell_linter.md](test-quality-drill/smell_linter.md) | Task 7 + N10 | 3 | (드릴 후 갱신) |
| **합계** | | | **16** | |

## 5. 결과 해석

- **모든 사보타지 ≥1 케이스 FAIL** → 합격. 표에 FAIL한 케이스 기록.
- **어떤 사보타지가 0 케이스 FAIL** → blind spot. 그 카테고리에 케이스 추가 후 재드릴.
- **사보타지를 잡은 케이스가 *예상과 다름*** → 케이스 의도 모호 (이름/주석 보강).
- **예측이 빗나간 비율** → 사용자의 *멘탈모델 정확도*. 빗나감이 잦으면 코드/케이스의 의도가 코드만으로 안 드러난다는 신호.

## 6. Mutation Testing 보류 — 진입 트리거

현재 mull 도입 안 함 (macOS arm64 LLVM 매칭 부담, 컴포넌트 5개로 ROI ↓).

**다음 중 하나라도 발생 시 도입 검토**:

1. 신규 테스트 인프라 컴포넌트 ≥ 10개
2. production 회귀가 *기존 테스트를 통과한 채로* 슬립
3. CI 시간 < 5분 + 머신 여유
4. [.claude/agents/render-quality-gate.md](../../.claude/agents/render-quality-gate.md) PoC 시점 도달

지금은 사람이 *어떤 변경이 그럴듯한가*를 판단하는 학습 가치를 우선.

## 7. 본 plan 자체의 회귀 추적 (N0-N14 결함)

본 drill의 *2차 효익*: [implementation plan §Implementation Notes](2026-05-07-gl-state-and-test-quality-implementation.md)의 N1-N14 결함이 *우연이 아니라 plan 자체의 진화*임을 검증.

- **N10** (STATIC_REQUIRE 누락) → smell_linter.md의 사보타지 1번이 그대로 재현
- **N11** (SymbolicName 사전 vertex type 누락) → symbolic_name.md의 사보타지 4번이 그대로 재현

→ 향후 plan에 새 결함이 발견되면 *그 결함을 사보타지 표에 즉시 추가*하는 것이 표준 절차.
