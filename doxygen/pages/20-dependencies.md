# 의존 라이브러리 {#dependencies}

본 프로젝트는 **vcpkg 를 쓰지 않는다** (교수 제출용). 모든 서드파티는 `cmake/Dependency.cmake` 가
`lib/{macos,windows}/` 사전 빌드 정적/동적 라이브러리를 `IMPORTED` 로 등록하고 두 개의 INTERFACE
집계 타겟으로 묶는다 — `project_deps` (GL/윈도우) 와 `game_deps` (게임/엔진).

## 집계 타겟

| 타겟 | 멤버 | 링크 대상 |
|------|------|-----------|
| **`project_deps`** | `sb7`, `glfw3`, OpenGL + 플랫폼 프레임워크. include = `include/` | 모든 데모 (필수) |
| **`game_deps`** | `box2d`, `EffekseerRendererGL`(→`Effekseer`), `assimp`(→`zlibstatic`), `spdlog`, `tweeny`, `stb_extra`, 조건부 `fmod`/`fmodstudio`. SYSTEM include = `include/` | 게임/엔진 데모만 opt-in |

```cmake
# 게임 데모는 둘 다, 일반 챕터는 project_deps 만
target_link_libraries(<demo> PRIVATE project_deps game_deps SJH::engine)
```

`game_deps` 의 `include/` 는 **SYSTEM** 인클루드 — 서드파티 헤더가 Debug `-Wall -Werror` 대상에서 제외된다.

## 패키지 표

| 패키지 | CMake 타겟 | 종류 | 사용처 |
|--------|-----------|------|--------|
| **sb7** | `sb7` | STATIC IMPORTED (Debug `_d`) | SuperBible base `sb7::application` |
| **GLFW** | `glfw3` | STATIC IMPORTED (Debug `_d`) | 윈도우/입력/GL 컨텍스트 |
| **OpenGL** | `${OPENGL_LIBRARIES}` | `find_package(OpenGL)` | GL 함수 (gl3w 로더 경유) |
| **Box2D** v2.4.1 | `box2d` | STATIC IMPORTED | 2D 물리 (`apps/_MyApp_/src/Physics/`) |
| **Effekseer** 1.7.3.0 | `EffekseerRendererGL` → `Effekseer` | STATIC IMPORTED | 파티클 + GL 렌더러 |
| **assimp** v5.4.3 | `assimp` → `zlibstatic` | STATIC IMPORTED | 3D 모델 임포트 (번들 zlib) |
| **spdlog** v1.17.0 | `spdlog` / `spdlog::spdlog` | STATIC IMPORTED (`SPDLOG_COMPILED_LIB`) | 게임/엔진 로깅 |
| **Tweeny** | `tweeny` | INTERFACE (헤더 온리) | 트위닝 |
| **stb** | `stb_extra` | INTERFACE (헤더 온리) | 이미지 디코딩 (`SJH::Image`) |
| **FMOD Core/Studio** | `fmod` / `fmodstudio` | SHARED IMPORTED (조건부) | 오디오 (`.bank` 이벤트). 독점 SDK — `doc/FMOD_Setup.md` |

> **FMOD 가드:** `include/fmod/fmod.h` 존재 시에만 등록. 미설치 환경은 STATUS 메시지만 출력하고 오디오 비활성.
> dynamic-only 이므로 `game_deps` 사용 데모는 POST_BUILD 에서 `$<TARGET_FILE:fmod>` 를 실행 파일 옆으로 `copy_if_different` 필수.

## 의존성 그래프 (집계 타겟)

\dot
digraph DepGraph {
  rankdir=LR;
  node [shape=box, fontname="Helvetica"];

  subgraph cluster_engine {
    label="SJH 엔진"; style=dashed;
    engine [label="SJH::engine\n(16 모듈 우산)"];
    resource_registry;
  }

  subgraph cluster_proj {
    label="project_deps"; style=filled; fillcolor="#f0f0f0";
    sb7; glfw3; OpenGL;
  }

  subgraph cluster_game {
    label="game_deps"; style=filled; fillcolor="#eef3fb";
    box2d; Effekseer; EffekseerRendererGL; assimp; zlibstatic;
    spdlog; tweeny; stb; fmod; fmodstudio;
  }

  app [shape=ellipse, style=filled, fillcolor="#fff7d6", label="apps/_MyApp_"];
  app -> engine;
  app -> sb7   [lhead=cluster_proj];
  app -> box2d [lhead=cluster_game];

  engine -> sb7 [lhead=cluster_proj];
  resource_registry -> fmod      [label="PUBLIC"];
  resource_registry -> Effekseer [label="PUBLIC"];
  resource_registry -> stb;

  EffekseerRendererGL -> Effekseer;
  assimp -> zlibstatic;
  fmodstudio -> fmod;
}
\enddot

## 서브모듈 (버전 추적 / 재빌드 소스용)

`extern/` 의 서브모듈은 **버전 추적·재빌드 소스**이며, 실제 빌드는 `lib/`·`include/` 체크인 산출물에 의존한다.

| 서브모듈 | 비고 |
|----------|------|
| `extern/sb7code` | SuperBible 7 원본 (glfw3 + sb7) |
| `extern/box2d` (v2.4.1) / `extern/Effekseer` (1.7.3.0) / `extern/assimp` (v5.4.3) / `extern/spdlog` (v1.17.0) | 컴파일 라이브러리 |
| `extern/tweeny` / `extern/stb` | 헤더 온리 |
| `extern/Catch2` (v3.15.0) | `ENABLE_TESTING=ON` 시 `add_subdirectory` 로만 컴파일 (사전 빌드 lib 없음) |
| `extern/imgui` (v1.53) | GLFW 3.0.4 호환 마지막 태그 — 핀 필수 |

```bash
git submodule update --init --recursive   # 서브모듈 동기화
python3 scripts/dev.py extern             # extern -> lib/include 재생성
```

## 트러블슈팅

| 증상 | 원인 | 해결 |
|------|------|------|
| `undefined reference to stbi_load` | `STB_IMAGE_IMPLEMENTATION` 미정의 | `src/resource_registry/image.cpp` 단일 owner — 다른 곳 정의 금지 |
| FMOD link 에러 (game 데모) | FMOD SDK 미설치 | `doc/FMOD_Setup.md` 따라 `include/fmod/` + `lib/` 배치 |
| startup 시 `libfmod.dylib not found` | POST_BUILD dll copy 누락 | 데모 CMakeLists 에 `copy_if_different $<TARGET_FILE:fmod>` 추가 |
| `extern/Catch2 가 비어 있습니다` | 서브모듈 미초기화 | `git submodule update --init --recursive` |
