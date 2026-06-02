# 빌드 시스템 {#build-system}

> 상세는 루트 `.claude/CLAUDE.md` + `.claude/architecture.md` 참조. 본 페이지는 핵심 발췌.

## 한 줄 요약

C++17, CMake 3.14+, **vcpkg 미사용** (교수 제출용 — `lib/`·`include/` 사전 빌드 체크인).
크로스 플랫폼: macOS·Linux (Ninja) / Windows (MSVC).

## 루트 `CMakeLists.txt` include 순서

\dot
digraph IncludeOrder {
  rankdir=TB;
  node [shape=box, fontname="Helvetica"];
  proj [label="project(OpenGL-ComputerGraphics)"];
  opt  [label="option(SJH_OPENGL_BUILD_DOCS ON)\noption(ENABLE_TESTING OFF)"];
  cxx  [label="cmake/CXXStandard.cmake\n(C++17, -Werror 등)"];
  doxy [label="cmake/Doxygen.cmake\n(sjhopengl_setup_doxygen 정의)"];
  dep  [label="cmake/Dependency.cmake\n(project_deps + game_deps IMPORTED)"];
  src  [label="add_subdirectory(src)\n(SJH::<module> + SJH::engine 우산)"];
  apps [label="add_subdirectory(apps)\n(활성 데모만 add_subdirectory)"];
  docs [label="sjhopengl_setup_doxygen()\n(SJH_OPENGL_BUILD_DOCS 시)"];
  proj -> opt -> cxx -> doxy -> dep -> src -> apps -> docs;
}
\enddot

## 빌드 명령

```bash
# Configure (macOS/Linux)
cmake --preset ninja                 # Debug
cmake --preset ninja-release         # Release

# 데모 빌드 + 실행 (리소스 상대경로 때문에 cd 필요)
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_

# 문서 빌드 (본 HTML)
cmake --build --preset ninja --target doxygen
open doc/html/index.html             # macOS

# Shell 헬퍼 (macOS/Linux)
sh shell/CMakeALL.sh debug _MyApp_   # clean + configure + build + run
```

Windows (MSVC):

```bat
cmake --preset msvc        :: VS 2019 (저사양/학교 PC)
cmake --preset msvc-2022   :: VS 2022 (신형/CI)
cmake --build --preset msvc --target _MyApp_
```

## 프리셋 매트릭스

| Configure 프리셋 | OS | Generator | binaryDir |
|------------------|----|-----------|-----------|
| `ninja` | macOS/Linux | Ninja (Debug) | `build_ninja` |
| `ninja-release` | macOS/Linux | Ninja (Release) | `build_ninja-release` |
| `msvc` | Windows | VS 2019 (x64) | `build_msvc` |
| `msvc-2022` | Windows | VS 2022 (x64) | `build_msvc` |

> ARM64 Windows 호스트는 `cmake --preset msvc-2022 -A x64` 로 x64 강제 (사전 빌드 lib 가 x64).

## 빌드 옵션

| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `SJH_OPENGL_BUILD_DOCS` | `ON` | `doxygen` 타겟 등록. Doxygen 미설치 시 `find_package(... QUIET)` 로 조용히 스킵 |
| `ENABLE_TESTING` | `OFF` | Catch2 v3 단위 테스트 (`extern/Catch2` 서브모듈). `-DENABLE_TESTING=ON` |

```bash
# 테스트 활성화
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure
```

## Active Target 컨벤션

`apps/CMakeLists.txt` 는 데모를 `add_subdirectory(...)` 로 나열하되 **한 번에 소수만 활성화**한다.
새 데모 빌드 실패 시 가장 먼저 해당 줄의 주석 해제 여부를 확인 (현재 활성: `_MyApp_`).

## 외부 라이브러리 재생성 (평소 불필요)

`lib/`·`include/` 산출물은 체크인되어 있어 평소 재빌드 불필요. 재생성 시:

```bash
sh shell/BuildExternLibs.sh          # extern/sb7code -> glfw3 + sb7 (Release/Debug)
# 생성물을 lib/{macos,windows}/ + include/ 로 수동 복사 (스크립트 말미 안내)
```
