# Sabotage Drill — CaptureGLState

> [메인 문서로](../test-quality-drill.md)

**대상**: [src/diagnostics/gl_state_fields.cpp](../../../src/diagnostics/gl_state_fields.cpp)의 `CaptureGLState()` 함수.
**테스트**: [test/test_gl_state_capture.cpp](../../../test/test_gl_state_capture.cpp) (9 케이스).

## 표 (예측 vs 실측)

| # | 사보타지 | 적용 위치 | 예상 잡는 케이스 | 실제 잡힌 케이스 | 드릴 날짜 |
|--:|---|---|---|---|---|
| 1 | `GL_VERTEX_ARRAY_BINDING` ↔ `GL_CURRENT_PROGRAM` swap (vao/program 두 줄 교환) | gl_state_fields.cpp의 vao/program 조회 라인 | "fresh fixture default" + "VAO 바인딩 후 fields.vao 반영" | (실측 후 기재) | YYYY-MM-DD |
| 2 | 텍스처 unit loop `i < 16` → `i < 1` (slot 1+ 캡처 누락) | gl_state_fields.cpp 텍스처 순회부 | 사보타지 적용 후 unit 5에 텍스처 바인딩하고 캡처 → fields.texture_2d_per_unit[5] != 실제 핸들 잡힘 (직접 검증). 현재 plan 케이스로는 *fresh fixture만 사용해서 잡힘 어려움* — **케이스 추가 후보** | (실측) | YYYY-MM-DD |
| 3 | `glActiveTexture(static_cast<GLenum>(saved_active));` 복원 라인 삭제 | gl_state_fields.cpp의 텍스처 unit 순회 끝 | "부수효과 0 — active_texture 변하지 않음" | (실측) | YYYY-MM-DD |

## 결과 노트

(드릴 실행 후 채움)

- 예측이 정확했나?
- 잡지 못한 사보타지 → 추가한 케이스
- 잡았지만 케이스명이 의도를 안 드러내면 → 리네임 기록
- **사보타지 2 추가 검증**: fresh fixture만 사용하는 한 모든 unit이 0이라 unit loop 사보타지가 우연 통과. 진짜 검증을 위해 *unit 5에 텍스처 바인딩* 케이스를 [test_gl_state_capture.cpp](../../../test/test_gl_state_capture.cpp)에 추가 권장 (Task 9 후속).

## 발견된 plan 결함 회귀 사보타지

본 컴포넌트 구현 중 발견된 [plan 결함](../2026-05-07-gl-state-and-test-quality-implementation.md):
- **N7** (Retina viewport): viewport 절대값 비교 → actual GL 상태 비교로 변경. 사보타지로 변환 가능: viewport[2] 직접 비교 코드를 추가하면 macOS Retina에서 즉시 실패 → 향후 Retina 대응 회귀 추적 가능.
- **N8** (VAO=0 driver-dependent): "fresh fixture (VAO=0)에서 size==4 단언" 코드를 추가하면 macOS에서 즉시 실패.
