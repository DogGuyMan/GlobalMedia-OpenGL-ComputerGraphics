# test/ — CLAUDE.md
Catch2 v3 기반 테스트 스위트 — CPU 단위 테스트 + GL 픽스처 테스트 + 골든 이미지 비교 게이트.

## Purpose (owns / configures)
- CPU 단위 테스트 12종 (`SJH::<module>` 공개 헤더에만 바인딩 — contract-anchoring)
- `test/gpu` GL 컨텍스트 픽스처 테스트 + `test/golden`/`test/golden_compare` 골든 이미지 비교 게이트

## Quick commands
```bash
# CPU 단위 + GPU 픽스처 (골든 제외 — build_ninja 에는 골든 ctest 가 등록되지 않는다)
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure

# 골든 이미지 게이트 = 전용 프리셋 전유 (별도 build dir, 게임 빌드와 공존)
cmake --preset ninja-golden          # SJH_GOLDEN_CAPTURE + ENABLE_TESTING + vcpkg feature golden
cmake --build --preset ninja-golden --target tests
ctest --test-dir build_ninja-golden -R "골든" --output-on-failure
```

골든 프리셋 4종 (전부 `SJH_GOLDEN_CAPTURE=ON` + `VCPKG_MANIFEST_FEATURES=golden`):

| 프리셋 | build dir | REF 호환 |
|---|---|---|
| `ninja-golden` | `build_ninja-golden` | ✅ 정본 (REF 가 이 조합의 캡처본) |
| `ninja-release-golden` | `build_ninja-release-golden` | ✅ Debug 캡처와 14/14 비트 동일(2026-07-26 실측) |
| `msvc-golden` / `msvc-2022-golden` | `build_msvc-golden` (공유) | ❌ REF 는 macOS 2560x1440 Retina 캡처 — Windows 는 크기 불일치로 FAIL |

## Key files
- `test/CMakeLists.txt` — `sjh_add_test` 매크로(Catch2WithMain link + catch_discover_tests) + 골든 체인 정의(`if(SJH_GOLDEN_CAPTURE)` 게이트)
- `CMakePresets.json` `ninja-golden` — 골든 게이트 전용 프리셋(캡처 진입점 + ENABLE_TESTING 내장)
- `test/smoke/` — 빌드 스모크
- `test/gpu/` — GL 컨텍스트 픽스처(셰이더 링크/상태캐시/roundtrip/상태누수/bitmap font)
- `test/golden/` — 커밋된 골든 이미지 레퍼런스(PNG, 2560×1440)
- `test/golden_compare/golden_compare.cpp` — OpenCV absdiff 비교(순수 CPU)

## Gotchas
- 주의: `-DENABLE_TESTING=ON` 없이는 테스트가 빌드조차 안 됨 — Why: 루트 `CMakeLists.txt` 의 `ENABLE_TESTING` 옵션 기본값이 OFF.
- 주의: 골든은 3줄이 한 세트(configure → build → ctest) — `ctest` 만 다시 돌리지 말 것. Why: `ctest` 는 빌드하지 않으므로 `--target tests` 를 빠뜨리면 **옛 바이너리를 검증하고 GREEN** 을 준다(stale 검증). 게임 빌드와 디렉토리가 달라 평소 개발 빌드로는 최신화되지 않는다 — 예전 단일 `build_ninja` 시절에는 자동으로 최신이었다. 재빌드 실측 = configure 7초 + 빌드 29초.
- 주의: 골든 게이트는 **빌드 모드**다 — `build_ninja` 에서는 `ctest` 가 100% GREEN 이어도 렌더 회귀를 전혀 안 본다. Why: 캡처 진입점(`capture_application`)이 컴파일 정의 `SJH_GOLDEN_CAPTURE` 로만 선택되고, 그 정의가 없는 빌드에는 `test/CMakeLists.txt` 가 골든 ctest 를 **등록조차 하지 않는다**(위양성 GREEN 방지). 게이트는 `--preset ninja-golden` 전유.
- 주의: 환경 변수 `SJH_GOLDEN_CAPTURE=1` 로 `_MyApp_` 를 실행하는 구 방식은 **2026-07-26 폐기** — Why: 런타임 분기가 컴파일 타임 진입점 선택으로 바뀌었다. 게임 빌드 바이너리에 캡처 코드가 링크되지 않으므로, 그 명령은 에러 없이 그냥 게임 창만 띄운다(조용한 실패 → "골든 통과" 오독 위험). 옛 플랜/핸드오프 문서에 이 명령이 남아 있으면 무시할 것.
- 주의: 골든 비교는 GL 캡처(`golden_capture` fixture)가 선행돼야 함 — Why: `FIXTURES_SETUP GOLDEN` 이 캡처 빌드 `_MyApp_` 를 먼저 실행해 PNG 14장을 생성(180 프레임 후 자동 종료).
- 주의: 각 test 실행 파일은 해당 `SJH::<module>` 공개 헤더에만 link — Why: contract-anchoring, 내부 구현 변경에 테스트가 부수적으로 깨지지 않게.
- 주의: OpenCV 는 **C++ 링크 전용**(vcpkg manifest) — Python `import cv2`/PIL 은 없다. 이미지 비교가 필요하면 `golden_compare` 를 쓰고 디코더를 새로 만들지 말 것. Why: `import cv2` 실패를 "OpenCV 없음"으로 오독해 기존 게이트를 중복 구현한 실사례(2026-07-26).
- 주의: skybox 하나만 바꿔도 `golden_skybox` + `golden_world_*` 가 **함께** 깨진다 — Why: `SkyboxPass` 가 sceneFB 를 clear+배경으로 채우고 World 큐가 그 위에 누적 렌더(`render_passable.impls.cpp` mClearsTarget=false). 골든 재생성 시 영향 범위를 skybox 1장으로 오판하지 말 것.
- 주의: 판정 임계는 `kChannelDiffThreshold=0`(비트동일) — 서브픽셀 시프트만으로도 차이픽셀 38% 가 나온다. FAIL 시 픽셀 **수**가 아니라 diff 아티팩트의 채널차 **크기**로 성격을 판단할 것(구조적 오류면 255 급, 보간 차이면 수십).
- 주의: 골든 REF 를 의도적으로 갱신하는 변경이면 REF 를 **원인 커밋과 같은 커밋에** 넣을 것 — Why: 나중 커밋으로 미루면 원인 커밋이 게이트 RED 로 남고(`git bisect`/체크아웃 시 FAIL), `git log -- test/golden/` 이 REF 를 흡수한 무관한 커밋을 지목해 추적자가 헤맨다. 실사례 = `0f0877c`(Mesh::CreateSphere) 가 픽셀을 바꿨는데 REF 는 `9931b72` 에 들어감.

## Cross-module deps
- 의존: `Catch2::Catch2WithMain`(PRIVATE) + 각 `SJH::<module>` + OpenCV(`golden_compare` 한정).
- 피의존: 없음(테스트는 leaf) — 단 CPU 테스트가 깨지면 해당 코어 모듈 API 변경이 원인.

## Common modification patterns
- 신규 CPU 단위테스트 추가: `test/CMakeLists.txt` 에 `sjh_add_test(test_<name> SJH::<module>)` 한 줄 + `tests` 우산 DEPENDS 목록에 등록.
- 골든 이미지 갱신: `cmake --build --preset ninja-golden --target _MyApp_` 후 `ctest --test-dir build_ninja-golden -R golden_capture` 로 PNG 재생성 → `build_ninja-golden/apps/_MyApp_/test/golden/` 산출물을 `test/golden/` 로 복사 후 커밋.
- GL 픽스처(gpu) 테스트 추가: `test/gpu/CMakeLists.txt` 하위 등록(slangc 미탐지 시 graceful skip 패턴 준수).

## See also
- [ARCHITECTURE.md](../ARCHITECTURE.md) (HO-3 생성 중) · [결정 스토어](../doc/adr/README.md)
- [2026-06-27-test-expansion-design.md](../doc/superpowers/specs/2026-06-27-test-expansion-design.md)
