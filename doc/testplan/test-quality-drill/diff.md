# Sabotage Drill — Diff

> [메인 문서로](../test-quality-drill.md)

**대상**: [test/support/gl_state_snapshot.cpp](../../../test/support/gl_state_snapshot.cpp)의 `Diff(A, B)` 함수.
**테스트**: [test/test_gl_state_snapshot.cpp](../../../test/test_gl_state_snapshot.cpp) (13 케이스).

## 표 (예측 vs 실측)

| # | 사보타지 | 적용 위치 | 예상 잡는 케이스 | 실제 | 드릴 날짜 |
|--:|---|---|---|---|---|
| 1 | 변화 무관 항상 `"(no GL state change)\n"` 반환 | gl_state_snapshot.cpp Diff 함수 첫 줄에 `return "(no GL state change)\n";` 강제 | "Diff — handle 변화는 raw 정수", "enum 변화는 SymbolicName", "element_buffer 변화 + VAO=0", "texture unit 변화", "attribute slot 변화", "attribute size 변화" 등 변화 케이스 다수 | (실측) | YYYY-MM-DD |
| 2 | 변화 없는 필드도 출력 (`DiffField` template에서 `if (a != b)` 조건 제거) | gl_state_snapshot.cpp `DiffField` helper | "Diff — 동일 snapshot은 '(no GL state change)'" — 출력이 비지 않을 것 + "attribute 모든 필드 동일하면 변화 출력 안 됨" | (실측) | YYYY-MM-DD |
| 3 | before/after 인자 swap (Diff 본문에서 `const auto& a = A.fields; const auto& b = B.fields;` 의 A/B 교환) | gl_state_snapshot.cpp Diff 함수 본문 | "Diff — handle 변화는 raw 정수" — `3 → 5` 가 `5 → 3`로 출력. 케이스가 substring "3"과 "5" 둘 다 검사하므로 **잡힘 X 가능**. <br>→ 잡히지 않으면 케이스 강화 필요: 출력 형식의 *방향성*까지 단언 (예: `Contains("3 → 5")`). | (실측) | YYYY-MM-DD |

## 결과 노트

(드릴 실행 후)

**사보타지 3 강화 후보**: 현재 케이스가 substring을 *비방향성*으로만 검사하므로, before/after swap 사보타지가 우연 통과 가능. 다음 단언 추가:

```cpp
// 강화된 단언
REQUIRE_THAT(d, ContainsSubstring("3 → 5"));  // 정확한 방향
REQUIRE_FALSE(d.find("5 → 3") != std::string::npos);  // 역방향 없음
```

## 발견된 plan 결함 회귀 사보타지

본 컴포넌트 구현 중 직접 발견된 새 결함은 없음. 단, 미래 회귀 검증을 위한 사보타지 후보:
- audit 트랙 A의 `DiffAttrib` helper에서 `if (a == b) return;` 제거 → 동일 attribute에도 출력
- VAO=0 EBO 주석 분기를 항상/절대 적용 (snapshot_tostring과 유사 사보타지)
