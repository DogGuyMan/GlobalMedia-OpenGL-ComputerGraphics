# CMake C++ Environment Init — OpenGL-ComputerGraphics 전용

본 저장소(SuperBible 7 코스워크) 의 빌드 시스템과 모듈 구조를 파악해 에이전트의 컨텍스트를 초기화한다.

> 이 커맨드는 *본 저장소* 의 실체에 맞춰 작성됐다 — **vcpkg manifest 모드 (외부 `$env{VCPKG_ROOT}`, 2026-06-20 전이 완료)**. 전이 의존(box2d/assimp/spdlog/tweeny/stb/catch2/nlohmann-json)은 vcpkg `find_package`, 잔류 의존(glfw3·sb7·Effekseer·FMOD·imgui)만 `lib/`·`include/` 사전 빌드 IMPORTED. `find_package` 는 `OpenGL`(필수) + vcpkg 7종 + `Doxygen`(QUIET). 셰이더는 Slang(`find_program slangc`, tool-only).

## Steps (run in order)

### 1. Discover Files

Glob 으로 다음을 찾는다 (없으면 skip):
- `CMakeLists.txt` — 루트
- `CMakePresets.json` — ninja / msvc / msvc-2022 프리셋 (`base` 가 vcpkg toolchain 주입)
- `vcpkg.json` + `vcpkg-configuration.json` — manifest + overlay-port(box2d arm64)
- `cmake/*.cmake` — `CXXStandard.cmake`, `Dependency.cmake`, `Doxygen.cmake`, `Slang.cmake`, `SlangPostProcess410.cmake` (이 5개)
- `src/CMakeLists.txt` + `src/<module>/CMakeLists.txt` (19개 모듈, 우산 `SJH::engine` INTERFACE)
- `apps/CMakeLists.txt` + `apps/<chapter>/CMakeLists.txt` (현재 `_MyApp_` 단독 — 다른 데모는 본 브랜치에서 제거됨)
- `test/` · `test_smoke/` — **현재 부재** (구 Catch2 v3 21개 2026-06-20 폐기, 재작성 대기). 루트 `option(ENABLE_TESTING ...)` 배선은 `EXISTS` 가드라 부재해도 통과
- `extern/sb7code` 등 서브모듈
- `.vscode/settings.json`, `.clangd`

### 2. Read Core CMake Files

**필수:**
1. 루트 `CMakeLists.txt` → project 이름/include order/add_subdirectory layout
2. `CMakePresets.json` → ninja(macOS/Linux) / msvc(VS2019 v142) / msvc-2022(VS2022 v143). `base` 가 `CMAKE_TOOLCHAIN_FILE=$env{VCPKG_ROOT}/...` + `VCPKG_MANIFEST_INSTALL ON`. `binaryDir = ${sourceDir}/build_${presetName}` (msvc 계열은 `build_msvc` 공유). Windows triplet = `x64-windows-static`
3. `cmake/CXXStandard.cmake` → C++17, `_USE_MATH_DEFINES`, Debug 컴파일 옵션 (`-Wall -Werror` / `/utf-8 /Zc:__cplusplus`), WIN32 시 `NOMINMAX WIN32_LEAN_AND_MEAN`
4. `cmake/Dependency.cmake` → vcpkg `find_package`(box2d/assimp/spdlog/tweeny/Stb/nlohmann_json) + 잔류 IMPORTED(sb7/glfw3/Effekseer/FMOD), `project_deps` / `game_deps` INTERFACE 집약
5. `cmake/Slang.cmake` → `find_program(SLANGC_EXECUTABLE)` + `sjh_compile_slang()` (셰이더 .slang → GLSL 410, 링크 없음)

**모듈/앱:**
- `src/CMakeLists.txt` → 19개 add_subdirectory (buffer/common/diagnostics/fsm/input/layout/material/object/playable/program/render/render_bootstrap/resource_registry/scene/shader/sprite/text/texture/timer) + 우산 INTERFACE 타겟 `SJH::engine`
- 각 `src/<module>/CMakeLists.txt` → `SJH::<module>` ALIAS + 의존 PUBLIC/PRIVATE 명시
- `apps/CMakeLists.txt` → `add_subdirectory(_MyApp_)` 단독. 다른 데모(migrate_demo/audio_demo/box2d_demo/effekseer_demo/tweeny_demo)는 본 브랜치에서 디렉토리 자체 제거됨 — 다른 브랜치 참고
- `apps/_MyApp_/CMakeLists.txt` → 하위 STATIC + 얇은 entry (패턴 A 변형) + imgui v1.53 client-side 직접 컴파일

### 3. Extract Build System Rules

문서화할 사실:
- **C++ standard** — C++17 (`cmake/CXXStandard.cmake`), `CMAKE_EXPORT_COMPILE_COMMANDS ON`
- **Binary dir** — `build_ninja`, `build_ninja-release`, `build_msvc` (msvc/msvc-2022 모두 `build_msvc` 로 명시 고정)
- **플랫폼 프리셋** — `ninja` / `ninja-release` (macOS+Linux, Windows 아님 condition), `msvc`(VS2019 v142) / `msvc-2022`(VS2022 v143) (Windows, `MultiThreaded$<$<CONFIG:Debug>:Debug>` runtime, `x64-windows-static` triplet, `CMAKE_POLICY_VERSION_MINIMUM 3.5`)
- **타겟 명명** — 내부 `SJH::<module>` + 우산 INTERFACE `SJH::engine`. vcpkg `find_package` 타겟 `box2d::box2d` / `assimp::assimp` / `spdlog::spdlog` / `tweeny` / `nlohmann_json::nlohmann_json` (+ `Stb_INCLUDE_DIR` → `stb_extra` INTERFACE). 잔류 IMPORTED `sb7` / `glfw3` / `Effekseer` / `EffekseerRendererGL` (+ 조건부 `fmod` / `fmodstudio` SHARED).
- **find_package** — `OpenGL`(REQUIRED) + vcpkg CONFIG: `box2d` `assimp` `spdlog` `tweeny` `Stb` `nlohmann_json` (+ `Catch2 3` ENABLE_TESTING 시) + `Doxygen`(QUIET — 미설치 시 docs 타겟 스킵). 셰이더는 `find_program(SLANGC_EXECUTABLE)`.
- **vcpkg manifest 모드** — `vcpkg.json` (builtin-baseline = 로컬 vcpkg HEAD, box2d 2.4.1·assimp 5.4.3 override 핀) + `vcpkg-configuration.json` (box2d arm64 overlay-port).
- **챕터 활성화** — `apps/CMakeLists.txt` 가 `_MyApp_` 단독. 새 데모는 `add_subdirectory` 추가.
- **리소스 복사** — POST_BUILD 에서 `resources/` 전체가 실행 파일 디렉토리로 복사 (상대 경로 로드 가정). Slang 셰이더는 POST_BUILD 후처리로 GLSL 410 overlay.

### 4. Check VSCode / clangd Config

`.vscode/settings.json` / `.clangd`:
- `cmake.buildDirectory` 가 실제 binaryDir 와 일치
- clangd 가 `build_ninja/compile_commands.json` 를 사용
- GLSL 린팅은 `glslangValidator`

### 5. Report

사용자에게 다음 형식으로 출력:

```
## CMake C++ Environment Initialized

Project:      OpenGL-ComputerGraphics v0.1.0 (C CXX)
C++ Standard: C++17 (CMAKE_EXPORT_COMPILE_COMMANDS ON)
CMake Min:    3.14
Deps:         vcpkg manifest 모드 (외부 $env{VCPKG_ROOT}) + 잔류 prebuilt lib/·include/

Platforms:
  macOS/Linux: ninja / ninja-release → build_ninja / build_ninja-release (Ninja)
  Windows:     msvc (VS2019 v142) / msvc-2022 (VS2022 v143) → build_msvc
               x64-windows-static triplet, MT$<Debug:Debug> runtime, POLICY_VERSION_MINIMUM 3.5

Build:
  First setup: cmake --preset ninja && cmake --build --preset ninja --target _MyApp_
  Active target: apps/CMakeLists.txt = _MyApp_ 단독
  Run: cd build_ninja/apps/_MyApp_ && ./_MyApp_   (리소스 상대경로 → cd 필수)

Internal modules (19): buffer/common/diagnostics/fsm/input/layout/material/object/
                       playable/program/render/render_bootstrap/resource_registry/scene/
                       shader/sprite/text/texture/timer
                       → 우산 INTERFACE 타겟 SJH::engine 이 19개 한 줄 link
find_package:          OpenGL(REQUIRED) · box2d · assimp · spdlog · tweeny · Stb ·
                       nlohmann_json · Catch2 3(ENABLE_TESTING 시) — vcpkg CONFIG.
                       Doxygen(QUIET). find_program: slangc (셰이더 tool-only)
잔류 IMPORTED:         sb7 / glfw3 (_d 접미사) · Effekseer · FMOD(조건부 SHARED) · imgui v1.53
                       (project_deps INTERFACE / game_deps INTERFACE 로 집약)

Test wiring:  test/ · test_smoke/ 디렉토리 부재 (구 21개 OutDated 폐기, 재작성 대기).
              루트 option(ENABLE_TESTING OFF) + EXISTS 가드라 -DENABLE_TESTING=ON 도 조용히 스킵.

Warnings: [감지된 불일치 또는 "none"]
```

## Notes

- Read-only. 코드/CMake 를 수정하지 않는다.
- 보고만 출력하고, `<.claude>/MEMORY.md` 같은 외부 인덱스를 새로 만들지는 않는다 (본 저장소는 그런 인덱스 미사용 — 참고로 사용자 전역 auto-memory 는 레포 외부 `~/.claude/projects/.../memory/MEMORY.md` 에 별도 존재하나 이 저장소 파일이 아니다).
