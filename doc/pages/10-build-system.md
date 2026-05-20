# 빌드 시스템 {#build-system}

> 본 페이지는 `.claude/build-system.md` 의 핵심을 발췌. 상세 트러블슈팅은 원본 참조.

## 한 줄 요약

C++17, CMake 3.21+, vcpkg manifest mode, 크로스 플랫폼(macOS arm64 / Windows x64).

## include() 순서 (루트 `CMakeLists.txt`)

\dot
digraph IncludeOrder {
  rankdir=TB;
  node [shape=box, fontname="Helvetica"];
  cxx [label="cmake/CXXStandard.cmake\n(C++17, compile_commands)"];
  doxy [label="cmake/Doxygen.cmake\n(sjhopengl_setup_doxygen)"];
  dep [label="cmake/Dependency.cmake\n(find_package vcpkg)"];
  cfg [label="cmake/Config.cmake\n(WINDOW_NAME/WIDTH/HEIGHT)"];
  src [label="add_subdirectory(src)\n(SJH:: aliases)"];
  app [label="add_subdirectory(app)\n(configure_file → config.h)"];
  test [label="add_subdirectory(test)\n(Catch2)"];
  cxx -> doxy -> dep -> cfg -> src -> app -> test;
}
\enddot

`Config.cmake` 가 `add_subdirectory(app)` 보다 먼저 include 되어야 함 — `configure_file(config.h.in config.h)` 가 `WINDOW_*` 변수에 의존.

## 빌드 명령

```bash
# macOS 첫 셋업 / 의존성 변경 시
cmake --preset debug && cmake --build build_Darwin

# 코드만 수정 (vcpkg 실행 없음)
cmake --build build_Darwin

# vcpkg 스킵 빠른 재구성
cmake --preset debug -DVCPKG_MANIFEST_INSTALL=OFF

# 의존성만 명시적 설치
cmake --build build_Darwin --target install-deps

# 본 문서 빌드
cmake --build build_Darwin --target doxygen
```

## 프리셋 매트릭스

| Preset | OS | Generator | binaryDir |
|--------|----|-----------|-----------|
| `debug` | macOS | Unix Makefiles | `build_Darwin` |
| `release` | macOS | Unix Makefiles | `build_Darwin` |
| `debug-windows` | Windows | VS 17 2022 (x64) | `build_Windows` |
| `release-windows` | Windows | VS 17 2022 (x64) | `build_Windows` |

## config.h 생성 흐름

```
cmake/Config.cmake (set WINDOW_*)
  ↓ include()
app/CMakeLists.txt: configure_file(config.h.in config.h)
  ↓
build_Darwin/app/config.h
  ↓ #include "config.h"
app/main.cpp
```

## 빌드 옵션

| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `SJH_OPENGL_BUILD_TESTS` | `ON` | Catch2 단위 테스트 빌드 |
| `SJH_OPENGL_BUILD_DOCS`  | `ON` | `doxygen` 타겟 등록 |
