# 의존 라이브러리 {#dependencies}

본 프로젝트는 vcpkg manifest mode([vcpkg.json](../../vcpkg.json))로 의존성을 관리한다.
`cmake --preset <name>` 시점에 자동 설치되며, `cmake/Dependency.cmake` 가 `find_package` 호출.

## 패키지 표

| 패키지 | vcpkg 명 | CMake 타겟 | 사용처 |
|--------|----------|-----------|--------|
| **fmt** | `fmt` | `fmt::fmt` | 포매팅 (spdlog 백엔드 + diagnostics 의 `fmt::join`) |
| **spdlog** | `spdlog` | `spdlog::spdlog` | 모든 모듈의 로깅 백엔드 |
| **GLFW** | `glfw3` | `glfw` (네임스페이스 없음 — 정적 `.a`) | 윈도우/입력/GL 컨텍스트 — `app/main.cpp` + `Context::ProcessInput` |
| **glad** | `glad` | `glad::glad` | OpenGL 함수 로더 — 모든 GL 모듈 |
| **glm** | `glm` | `glm::glm` | 행렬/벡터 수학 — `Camera`, `Context`, `ResourceRegistry` |
| **stb** | `stb` | `${Stb_INCLUDE_DIR}` (헤더 only) | 이미지 디코딩 — `ResourceRegistry` (PRIVATE) |
| **ImGui** | `imgui[glfw-binding,opengl3-binding]` | `imgui::imgui` | 디버그 UI — `app/main.cpp` 가 GLFW + OpenGL3 바인딩으로 초기화. `Context` 의 라이팅 멤버 (`mLightPos`/`mObjectColor`/`mAmbient*` 등) 를 위젯으로 직접 편집. |
| **ImGuizmo** | `imguizmo` | `imguizmo::imguizmo` | ImGui 위 좌표축 / 행렬 조작 위젯. 카메라/광원 transform gizmo 용 (Phase 10~). |
| **Catch2** | `catch2` | `Catch2::Catch2WithMain` | 단위 테스트 (`test/`) |

## 의존성 그래프 (모듈 → 외부)

\dot
digraph DepGraph {
  rankdir=LR;
  node [shape=box, fontname="Helvetica"];

  subgraph cluster_internal {
    label="내부 모듈";
    style=dashed;
    common; diagnostics; shader; program;
    buffer; layout; resource_registry; object;
    context;
  }

  subgraph cluster_external {
    label="외부 (vcpkg)";
    style=filled; fillcolor="#f0f0f0";
    fmt; spdlog; glfw; glad; glm; stb; catch2; imgui; imguizmo;
  }

  // app 노드 (라이프 사이클 + ImGui 호스트)
  app [shape=ellipse, style=filled, fillcolor="#fff7d6"];
  app -> context;
  app -> imgui;
  app -> imguizmo;
  app -> glfw;
  app -> glad;

  common -> spdlog;

  diagnostics -> spdlog;
  diagnostics -> fmt;
  diagnostics -> glad;

  shader -> common;
  shader -> diagnostics;
  shader -> glad;

  program -> common;
  program -> shader;
  program -> diagnostics;
  program -> glad;

  buffer -> common;
  buffer -> diagnostics;
  buffer -> glad;

  layout -> common;
  layout -> diagnostics;
  layout -> glad;

  resource_registry -> common;
  resource_registry -> diagnostics;
  resource_registry -> glad;
  resource_registry -> glm;
  resource_registry -> spdlog;
  resource_registry -> stb;

  object -> glm;

  context -> program;
  context -> shader;
  context -> buffer;
  context -> layout;
  context -> resource_registry;
  context -> object;
  context -> diagnostics;
  context -> glad;
  context -> glm;
  context -> glfw;
}
\enddot

## 모듈별 PUBLIC / PRIVATE 노출 정책

| 모듈 | PUBLIC (소비자에게 전파) | PRIVATE (내부 전용) |
|------|--------------------------|--------------------|
| common | `spdlog::spdlog` | — |
| diagnostics | `glad::glad`, `spdlog::spdlog`, `fmt::fmt` | — |
| shader | `SJH::common`, `glad::glad` | `SJH::diagnostics` |
| program | `SJH::common`, `SJH::shader`, `glad::glad` | `SJH::diagnostics` |
| buffer | `SJH::common`, `glad::glad` | `SJH::diagnostics` |
| layout | `SJH::common`, `glad::glad` | `SJH::diagnostics` |
| resource_registry | `SJH::common`, `glad::glad`, `glm::glm`, `spdlog::spdlog` | `SJH::diagnostics`, `${Stb_INCLUDE_DIR}` |
| object (INTERFACE) | `glm::glm` | — |
| context | `SJH::*` 모듈 + `glad::glad` + `glm::glm` + `glfw` | `SJH::diagnostics` |

> 원칙: `diagnostics` 는 항상 PRIVATE — 소비자 헤더에 진단 헤더가 새지 않게 함.
> 자세한 정책은 `.claude/architecture.md` §4 의 PUBLIC/PRIVATE 매트릭스 참조.

## 명시적 의존성 설치 타겟

```bash
cmake --build build_Darwin --target install-deps
```

내부적으로 `$ENV{VCPKG_ROOT}/vcpkg install --triplet ${VCPKG_TARGET_TRIPLET}` 호출.
`vcpkg.json` 변경 후 재구성 없이 의존성만 갱신할 때 사용.

## 트러블슈팅

| 증상 | 원인 | 해결 |
|------|------|------|
| `ld: library 'fmt' not found` | `fmt::fmt` 가 아닌 `fmt` 로 링크 | `target_link_libraries(... fmt::fmt)` |
| `ninja required v1.13.2` | 구버전 ninja | `brew install ninja` |
| glad "OpenGL header already included" | glfw3.h가 glad보다 먼저 include | `glad/glad.h` 를 가장 위에 |
| `glm/glm.hpp not found` (object/camera) | `object` INTERFACE 타겟이 glm 미링크 | `src/object/CMakeLists.txt` 에 `glm::glm` INTERFACE 링크 |
| 텍스처가 거꾸로 보임 | stb_image 의 위→아래 스캔과 GL 의 아래→위 UV 차이 | `stbi_set_flip_vertically_on_load(1)` 호출 |

자세한 내용은 `.claude/build-system.md` §의존성 트러블슈팅.
