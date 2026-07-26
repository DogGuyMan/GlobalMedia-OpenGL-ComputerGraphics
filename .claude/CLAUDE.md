# CLAUDE.md

OpenGL Computer Graphics — C++17 CMake. `game/main` 브랜치는 *Core/Client 분리* + *Actor/Component 씬그래프* + *SceneRenderer* 로 전환된 데모/엔진 단계. 의존성은 vcpkg manifest 모드(외부 `$env{VCPKG_ROOT}`). 주석·소통은 한국어 선호.

## Quick commands

```bash
# Configure (macOS/Linux)
cmake --preset ninja

# _MyApp_ 빌드 + 실행 (현재 활성 타겟 단독)
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_   # 리소스 상대경로라 cd 필수

# 테스트 (Catch2 v3 + CTest)
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure

# 렌더 출력이 바뀌는 변경(셰이더/메시/패스/GL상태)의 정본 검증 = 골든 게이트
# ★ 전용 프리셋 (별도 build dir). 게임 빌드(build_ninja)의 _MyApp_ 는 골든을 캡처하지
#   않으므로 build_ninja 에는 골든 ctest 가 아예 등록되지 않는다.
#   빌드 GREEN 도, `ctest --test-dir build_ninja` 통과도 렌더 회귀를 못 잡는다.
# ★ 3줄이 한 세트 — build 를 빠뜨리면 ctest 가 옛 바이너리를 검증하고 GREEN 을 준다.
cmake --preset ninja-golden
cmake --build --preset ninja-golden --target tests
ctest --test-dir build_ninja-golden -R "골든" --output-on-failure
```

골든 프리셋 4종 = `ninja-golden`(정본) · `ninja-release-golden`(Release, Debug 캡처와 14/14 비트동일 실측) · `msvc-golden`/`msvc-2022-golden`(Windows — REF 가 macOS 2560x1440 캡처라 크기 불일치로 FAIL, 캡처 생성 용도). build dir·REF 호환표는 [`test/CLAUDE.md`](../test/CLAUDE.md).

전체 프리셋(msvc/msvc-2022 등) 및 개발 CLI(`python3 scripts/dev.py <서브커맨드>` — 구 `shell/` 통합) 목록은 `doc/CLAUDE-extended.md` 참조.

## Active target (CRITICAL)

`apps/CMakeLists.txt` 는 데모 타겟을 나열하되 **한 번에 하나/소수만 활성화** 하는 컨벤션(현재 `_MyApp_` 단독). 새 데모를 빌드하려면 해당 `add_subdirectory` 줄의 주석을 해제해야 한다. 빌드 실패 시 가장 먼저 확인할 지점.

## Key files

- [`apps/_MyApp_/CLAUDE.md`](../apps/_MyApp_/CLAUDE.md) — 활성 데모(탑다운 슈터) 상세
- [`src/CLAUDE.md`](../src/CLAUDE.md) — 18개 코어 모듈(`SJH::<module>` STATIC + `SJH::engine` 우산) 가이드
- [`ARCHITECTURE.md`](../ARCHITECTURE.md) — 모듈 의존 지도 + 역인덱스
- [`doc/api/EngineAPI.md`](../doc/api/EngineAPI.md) — SJH 엔진 코어 API 레퍼런스(정본)
- [`doc/adr/README.md`](../doc/adr/README.md) — 결정 스토어 포인터

## Gotchas

- 헤더가드는 `__CHAPTER_N_ENTRY_H__` 형식(`#pragma once` 미사용). 주의: 이 컨벤션 위반 금지.
- stb_image 단일 owner = `src/texture/image.cpp` 한 곳만 `STB_IMAGE_IMPLEMENTATION`. Why: 중복 정의 시 링크 에러. 직접 `stbi_*` 호출 금지 — `SJH::Image::Load` 위임.
- ESC 키는 sb7.h run 루프가 하드와이어드 종료 폴링. 주의: 인게임 액션에 ESC 못 씀(Pause 등은 다른 키/클릭 UI). Why: sb7 수정 금지 규율(`extern/sb7code` 절대 불변).
- 주석·문서는 한국어. 코드 주석 = Doxygen + ASCII/한글만(특수문자 0).
- 검증 도구는 **만들기 전에 [`test/CLAUDE.md`](../test/CLAUDE.md) 확인** — 골든 비교기·GL 픽스처·CPU 테스트 120종이 이미 있다. 주의: OpenCV 는 **C++ 링크 전용**(vcpkg)이라 `import cv2` 는 실패하지만 게이트는 정상 동작. Why: 언어 스코프 프로브(`import X`)로 도구 부재를 단정하면 기존 유틸을 중복 구현하게 됨 — 도구 유무는 소비자(CMake/테스트)에서 확인할 것.

## Cross-module deps

모듈별 상세는 각 `<module>/CLAUDE.md`(src/·apps/_MyApp_/·test/·cmake/·scripts/·vcpkg-overlay-ports/ 6종), 의존 그래프는 [`ARCHITECTURE.md`](../ARCHITECTURE.md), 결정 스토어는 [`doc/adr/README.md`](../doc/adr/README.md), 확장 전문(구 281줄 원문)은 [`doc/CLAUDE-extended.md`](../doc/CLAUDE-extended.md).

## 설계 결정 가드레일

- 구조 결정의 **확정 주체는 항상 사용자** — AI는 실측·옵션 비교표·추천까지 ("진행하세요" 류 단독 확정 금지). 확정 전 기록은 `[제안됨]`, 검증 후 `[검증됨 <커밋>]`.
- 배치 결정 = 기존 소유자 grep 선행(design-decision-discipline §6.1), 모호하면 후보별 소유권 그래프(graphviz-class-diagram 결정 게이트). 신규 구조물 = 신고 4항(§2.5 + [`doc/templates/신규구조물신고서.md`](../doc/templates/신규구조물신고서.md)).
- lock 결정 정본 = `doc/superpowers/specs/` D# — 새 제안은 정합표 선행, 상충은 "번복 제안" 분리(architecture-design-workflow).
- 모든 설계 추천에 🔵실측/💭판단 라벨 + 등급별 행동(confidence-and-sourcing §1.5). 다축 제안은 축 분리 선언(§2.7 — 한 턴에 한 축).

## See also

전역 Skill(범용 가치관·방법론, `.claude/skills/`): design-decision-discipline / modular-build-discipline / code-design-review-lenses / architecture-design-workflow / benchmark-research-method / agent-orchestration-anti-gaming / response-quality-calibration / confidence-and-sourcing / personal-naming-conventions.

각 가치관 상세·프로젝트 문서 전체 인덱스는 [`doc/CLAUDE-extended.md`](../doc/CLAUDE-extended.md) 참조.
