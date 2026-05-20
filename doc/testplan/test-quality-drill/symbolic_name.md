# Sabotage Drill — SymbolicName

> [메인 문서로](../test-quality-drill.md)

**대상**: [src/diagnostics/gl_state_fields.cpp](../../../src/diagnostics/gl_state_fields.cpp)의 `SymbolicName(GLenum e)` 함수 (사전 37개 entry + GL_TEXTUREn 동적).
**테스트**: [test/test_gl_state_fields.cpp](../../../test/test_gl_state_fields.cpp) (9 케이스).

## 표 (예측 vs 실측) — 4 사보타지 (N11 후속 추가)

| # | 사보타지 | 적용 위치 | 예상 잡는 케이스 | 실제 | 드릴 날짜 |
|--:|---|---|---|---|---|
| 1 | unknown enum → 사전 첫 entry 반환 (default fallback이 `"GL_NEVER"` 같은 식, hex 무시) | SymbolicName의 hex fallback 부분 | "미적중 → hex fallback" — `Equals("0xDEAD")` 단언 깨짐 | (실측) | YYYY-MM-DD |
| 2 | 결과 lowercase (`"gl_less"`) | snprintf 또는 case의 string lit 손상 | "사전 적중" — `Equals("GL_LESS")` 정확 매칭 깨짐 + "depth_func 모든 8개" | (실측) | YYYY-MM-DD |
| 3 | 사전에서 `case GL_LESS: return "GL_LESS";` 라인 삭제 | SymbolicName switch | "사전 적중" + "depth_func 모든 8개" 둘 다 깨짐 | (실측) | YYYY-MM-DD |
| 4 | **(N11 후속)** vertex type case 삭제 (`case GL_FLOAT: return "GL_FLOAT";`) | SymbolicName switch의 vertex attribute types 영역 | [test/test_gl_state_log.cpp](../../../test/test_gl_state_log.cpp) "FieldsToString — attribute enabled 시 size/type/stride/vbo 출력" → `attrib[0]: vec3 0x1406, ...` 처럼 hex로 새고 `ContainsSubstring("GL_FLOAT")` 깨짐 | (실측) | YYYY-MM-DD |

## 결과 노트

(드릴 실행 후)

## 발견된 plan 결함 회귀 사보타지 (N11)

**N11 회귀 시나리오**: 본 plan 작성 시 audit 트랙 A에서 `attribute_layouts`를 추가하면서 `glGetVertexAttribiv(... GL_VERTEX_ATTRIB_ARRAY_TYPE ...)`로 GL_FLOAT/GL_INT/GL_UNSIGNED_BYTE 등을 캡처하지만, *그 enum들이 SymbolicName 사전에 없었음*. Task 5 빌드 후 13/13 PASS 중 1개 FAIL로 발견 → 9개 vertex type enum 추가.

**사보타지 4가 의미**:
- audit가 새 enum context 도입 시 SymbolicName 사전 갱신을 *놓치는* 패턴을 영구히 잡는 회귀.
- 미래 stencil/blend equation 등 새 enum 영역 추가 시에도 같은 사보타지 패턴 적용 가능.

**추가 사보타지 후보**:
- 사전의 9개 vertex type 중 하나만 삭제 (N11 정확 재현)
- TEXTUREn 동적 영역 (`if (e >= GL_TEXTURE0 && e <= GL_TEXTURE0 + 15)`)을 `e <= GL_TEXTURE0 + 1`로 줄여 GL_TEXTURE2-15 hex로 새기 검증
