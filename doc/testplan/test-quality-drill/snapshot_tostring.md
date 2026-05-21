# Sabotage Drill — GLStateSnapshot::ToString (FieldsToString)

> [메인 문서로](../test-quality-drill.md)

**대상**: [src/diagnostics/gl_state_fields.cpp](../../../src/diagnostics/gl_state_fields.cpp)의 `FieldsToString(const GLStateFields&)` 자유 함수.<br>
`GLStateSnapshot::ToString()`이 위임하는 공통 포매터.

**테스트**: [test/test_gl_state_log.cpp](../../../test/test_gl_state_log.cpp) (FieldsToString 케이스) + [test/test_gl_state_snapshot.cpp](../../../test/test_gl_state_snapshot.cpp) (ToString 케이스).

## 표 (예측 vs 실측)

| # | 사보타지 | 적용 위치 | 예상 잡는 케이스 | 실제 | 드릴 날짜 |
|--:|---|---|---|---|---|
| 1 | VAO=0 EBO 주석을 *항상* 출력 (vao 검사 제거) | FieldsToString의 element_buffer 라인의 `if (f.vao == 0)` → `if (true)` | test_gl_state_snapshot.cpp "ToString VAO≠0 — 주석 미포함" + "Diff — element_buffer 변화 + VAO≠0 이면 주석 미포함" | (실측) | YYYY-MM-DD |
| 2 | VAO=0 EBO 주석을 *절대* 출력 안 함 | 같은 위치, `if (f.vao == 0)` → `if (false)` | "ToString VAO=0 — 'EBO state is per-VAO' 주석 포함" + "Diff — element_buffer 변화 + VAO=0 시 EBO 주석" | (실측) | YYYY-MM-DD |
| 3 | enum 자리에 raw 정수 출력 (SymbolicName 호출 제거) | FieldsToString의 depth_func / blend_src_rgb / cull_face_mode / front_face / active_texture / attribute type 등 → `{}` format으로 raw 정수 | test_gl_state_log.cpp "FieldsToString — enum 필드는 SymbolicName 적용" + test_gl_state_snapshot.cpp "Diff — enum 변화는 SymbolicName" | (실측) | YYYY-MM-DD |

## 결과 노트

(드릴 실행 후)

## 발견된 plan 결함 회귀 사보타지

**audit 트랙 A 후속 사보타지 후보**:
- attribute_layouts 출력에서 `enabled` 검사 제거 → disabled slot도 출력 → 노이즈 폭증
- attribute_layouts 출력에서 `enabled` 검사를 `!enabled`로 inverted → 활성 slot이 *안 보임* (silent UI 회귀)
- texture_2d_per_unit 출력에서 0 unit 필터링 제거 → 16 unit 모두 출력 (노이즈)

**현재 plan 케이스로 잡히는지 확인 필요**: `"FieldsToString — texture unit 0 모두면 '(all units empty)'"` 케이스가 *attribute_layouts에는 적용 안 됨*. attribute용 동등 케이스가 있는지 검증 필요 (있다: "FieldsToString — attribute 모두 disabled면 '(all disabled)'").
