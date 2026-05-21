# Sabotage Drill — Test Smell Linter (check_test_smells.py)

> [메인 문서로](../test-quality-drill.md)
> **신규 컴포넌트** (N10 후속 추가) — Task 7 결함 발견 후 plan 강화의 일환.

**대상**: [scripts/check_test_smells.py](../../../scripts/check_test_smells.py) (~120 LoC Python).
**검증**: 수동 실행 + ctest #86 (`test_smells` LABEL=lint).

## 표 (예측 vs 실측)

본 컴포넌트의 사보타지는 *linter 자체의 회귀 감지력* 검증용. 자동 회귀 테스트가 없으므로 (linter는 ctest에 PASS-only로 등록됨), 사보타지 후 *수동 결과 비교*가 핵심.

| # | 사보타지 | 적용 위치 | 예상 잡는 행동 / 검증 절차 | 실제 | 드릴 날짜 |
|--:|---|---|---|---|---|
| 1 | **(N10 회귀)** ASSERT_RE에서 `STATIC_REQUIRE\|STATIC_CHECK` alternation 삭제 | scripts/check_test_smells.py ASSERT_RE | `python3 scripts/check_test_smells.py test/` 실행 → test_glfw_utils.cpp의 6개 TEST_CASE에 R1 false positive 발생 (현재 0 warnings → 6 warnings) | (실측) | YYYY-MM-DD |
| 2 | ASSERT_RE의 `REQUIRE` alternation 삭제 (가장 흔한 매크로 미감지) | 동상 | 거의 모든 테스트가 R1 false positive (test_buffer, test_program_uniforms, test_gl_state_capture 등 다수) | (실측) | YYYY-MM-DD |
| 3 | TEST_CASE_RE의 tag capture group 누락 (두 번째 인자 무시 — `(?:,\s*"(?P<tags>[^"]*)")?` 부분 삭제) | 동상 | 모든 TEST_CASE가 R3 (tag 누락) false positive 폭증 | (실측) | YYYY-MM-DD |

## 검증 절차

```bash
# 1. baseline — 현재 0 warnings 확인
python3 scripts/check_test_smells.py test/
# expected: "check_test_smells: 0 warnings across 13 files"

# 2. 사보타지 1 적용 (예: ASSERT_RE에서 STATIC_REQUIRE 제거)
$EDITOR scripts/check_test_smells.py
python3 scripts/check_test_smells.py test/
# expected: 6 warnings (test_glfw_utils.cpp의 6 STATIC_REQUIRE-only 케이스)

# 3. 복원
git checkout -- scripts/check_test_smells.py
python3 scripts/check_test_smells.py test/
# expected: 0 warnings 복귀
```

## 결과 노트

(드릴 실행 후)

본 컴포넌트는 *linter 자체의 회귀*를 추적. linter 코드가 변경될 때마다 본 드릴로:
- false positive 폭증이 없는지
- false negative (진짜 smell이 silent로 통과)가 없는지

## 발견된 plan 결함 회귀 사보타지 (N10)

**N10 회귀 시나리오**: Task 7 첫 빌드 시 13 test 파일에 대해 `test_glfw_utils.cpp`에서 6 R1 false positive 발생. 원인 = ASSERT_RE에 `STATIC_REQUIRE`/`STATIC_CHECK` (compile-time 단언 매크로) 누락.

**해결**: regex에 `STATIC_REQUIRE(?:_FALSE)?|STATIC_CHECK(?:_FALSE)?` 추가 → 0 warnings 회복.

**사보타지 1이 의미**: N10이 *우연이 아닌 plan의 진짜 결함*임을 영구 검증. 미래에 Catch2 새 매크로 (예: `STATIC_REQUIRE_*`) 도입 시 같은 패턴으로 빠질 수 있으므로 본 사보타지가 가드.

## 추가 사보타지 후보 (Phase 2)

- TEST_CASE_RE의 brace-counting 누락 (body 추출 실패) → 모든 케이스가 단언 0개로 감지
- DISABLED_TAGS list에서 `[.]` 제거 → R4 false negative
- ASSERT_RE에 `*_THROWS` 변종 누락 → 예외 단언 사용 케이스가 R1 false positive

이 후보들은 본 plan 외부의 Phase 2 — *linter 본체 강화 사이클*에서 추가.
