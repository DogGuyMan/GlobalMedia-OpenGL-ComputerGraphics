# cmake/ — CLAUDE.md
루트 CMakeLists.txt 가 `include()` 하는 공용 CMake 모듈 4개 — 표준/의존성/문서화/셰이더 컴파일 파이프라인을 담당한다.

## Purpose (owns / configures)
- C++ 표준 + 플랫폼별 컴파일러 경고 정책 전역 설정 (`CXXStandard.cmake`)
- 전이 의존(vcpkg) + 잔류 의존(prebuilt IMPORTED) 을 `project_deps`/`game_deps` INTERFACE 타겟으로 집약 (`Dependency.cmake`)
- Doxygen `doxygen` 커스텀 타겟 등록 (`Doxygen.cmake`)
- Slang 셰이더 → GLSL410 컴파일 + Python post-process 파이프라인 (`Slang.cmake`)

## Quick commands
```bash
# 이 디렉토리 파일들은 직접 실행 대상이 아니다 — 루트 CMakeLists.txt 가 include() 함
# 문서화 타겟만 별도 호출 가능:
cmake --build build_ninja --target doxygen   # doxygen/html/ 아래 index.html 생성 (빌드 산출물)

# Slang 셰이더 재컴파일은 매 빌드 시 COMMAND 로 자동 실행됨 (수동 트리거 불필요)
```

## Key files
- `cmake/CXXStandard.cmake` — C++17 + `_USE_MATH_DEFINES` 전역, GCC/Clang `-Wall -Werror -g -O0`, MSVC `/utf-8 /Zc:__cplusplus`
- `cmake/Dependency.cmake` — vcpkg `find_package`(box2d/assimp/spdlog/tweeny/stb/catch2) + 잔류 IMPORTED(glfw3/sb7/Effekseer/FMOD) → `project_deps`/`game_deps`
- `cmake/Doxygen.cmake` — `sjhopengl_setup_doxygen()`, `option(SJH_OPENGL_BUILD_DOCS ON)`
- `cmake/Slang.cmake` — Slang→GLSL410 컴파일 + `scripts/slang_compile.py` post-process 연동

## Gotchas
- 주의: MSVC Debug 는 narrowing 경고를 침묵(`/wd4244 /wd4305 /wd4267`)시키지만 Release 는 재활성화됨 — Why: Release 빌드에서만 실제 narrowing 버그가 노출되므로 Debug 통과를 신뢰하면 안 된다.
- 주의: FMOD/Effekseer 는 dynamic-only IMPORTED 타겟 — Why: `game_deps` 링크 챕터는 POST_BUILD 에서 dll `copy_if_different` 를 빠뜨리면 런타임 로드 실패로만 드러난다.
- 주의: `Slang.cmake` 의 post-process 는 매 빌드 COMMAND 로 실행됨 — Why: glslang 통과가 macOS GL 런타임 링크 성공을 보장하지 않는다(varying 이름 불일치·sampler `_N` 접미사 정규화 필요, 관련 MEMORY: slang-glsl410-traps).
- 주의: 루트 `.claude/CLAUDE.md` 는 "`cmake/` 에 3개 파일"이라 적어두었으나 `Slang.cmake` 신설로 현재 4개 — 문서가 stale 하므로 실측(`ls cmake/`)을 우선한다.

## Cross-module deps
- 의존: vcpkg toolchain(`CMakePresets.json` base 의 `$env{VCPKG_ROOT}`), `vcpkg.json`/`vcpkg-configuration` 오버라이드
- 피의존: 루트 `CMakeLists.txt` 전체 + 모든 활성 데모 빌드(이 파일들을 바꾸면 전체 빌드가 흔들림)

## Common modification patterns
- 새 외부 의존 추가: vcpkg 가능 여부 확인 후 `Dependency.cmake` 의 `project_deps`(엔진 공통) vs `game_deps`(게임 전용) 중 어디에 넣을지 결정.
- 컴파일러 경고/표준 정책 변경: `CXXStandard.cmake` 의 GCC/Clang·MSVC 분기 수정(Debug/Release 차이 유의).
- Slang 셰이더 파이프라인 조정: `Slang.cmake` + `scripts/slang_compile.py` post-process 동시 확인.

## See also
- [ARCHITECTURE.md](../ARCHITECTURE.md) (HO-3 생성 중) · [결정 스토어](../doc/adr/README.md) · [.claude/architecture.md](../.claude/architecture.md)
