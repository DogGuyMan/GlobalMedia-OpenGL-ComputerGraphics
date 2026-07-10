# test/ — CLAUDE.md
Catch2 v3 기반 테스트 스위트 — CPU 단위 테스트 + GL 픽스처 테스트 + 골든 이미지 비교 게이트.

## Purpose (owns / configures)
- CPU 단위 테스트 12종 (`SJH::<module>` 공개 헤더에만 바인딩 — contract-anchoring)
- `test/gpu` GL 컨텍스트 픽스처 테스트 + `test/golden`/`test/golden_compare` 골든 이미지 비교 게이트

## Quick commands
```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure
```

## Key files
- `test/CMakeLists.txt` — `sjh_add_test` 매크로(Catch2WithMain link + catch_discover_tests) + 골든 체인 정의
- `test/smoke/` — 빌드 스모크
- `test/gpu/` — GL 컨텍스트 픽스처(셰이더 링크/상태캐시/roundtrip/상태누수/bitmap font)
- `test/golden/` — 커밋된 골든 이미지 레퍼런스(PNG, 2560×1440)
- `test/golden_compare/golden_compare.cpp` — OpenCV absdiff 비교(순수 CPU)

## Gotchas
- 주의: `-DENABLE_TESTING=ON` 없이는 테스트가 빌드조차 안 됨 — Why: 루트 `CMakeLists.txt` 의 `ENABLE_TESTING` 옵션 기본값이 OFF.
- 주의: 골든 비교는 GL 캡처(`golden_capture` fixture)가 선행돼야 함 — Why: `FIXTURES_SETUP GOLDEN` 이 `_MyApp_` 를 `SJH_GOLDEN_CAPTURE=1` 로 먼저 실행해 PNG 를 생성.
- 주의: 각 test 실행 파일은 해당 `SJH::<module>` 공개 헤더에만 link — Why: contract-anchoring, 내부 구현 변경에 테스트가 부수적으로 깨지지 않게.

## Cross-module deps
- 의존: `Catch2::Catch2WithMain`(PRIVATE) + 각 `SJH::<module>` + OpenCV(`golden_compare` 한정).
- 피의존: 없음(테스트는 leaf) — 단 CPU 테스트가 깨지면 해당 코어 모듈 API 변경이 원인.

## Common modification patterns
- 신규 CPU 단위테스트 추가: `test/CMakeLists.txt` 에 `sjh_add_test(test_<name> SJH::<module>)` 한 줄 + `tests` 우산 DEPENDS 목록에 등록.
- 골든 이미지 갱신: `_MyApp_` capture 모드(`SJH_GOLDEN_CAPTURE=1`)로 PNG 재생성 후 `test/golden/` 커밋.
- GL 픽스처(gpu) 테스트 추가: `test/gpu/CMakeLists.txt` 하위 등록(slangc 미탐지 시 graceful skip 패턴 준수).

## See also
- [ARCHITECTURE.md](../ARCHITECTURE.md) (HO-3 생성 중) · [결정 스토어](../doc/adr/README.md)
- [2026-06-27-test-expansion-design.md](../doc/superpowers/specs/2026-06-27-test-expansion-design.md)
