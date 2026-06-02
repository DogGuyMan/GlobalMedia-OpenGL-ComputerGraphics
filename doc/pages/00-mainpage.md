# OpenGL-ComputerGraphics {#mainpage}

**SJH 엔진** — SuperBible 7th Edition 코스워크에서 출발해 *Core/Client 분리* + *Actor/Component 씬 그래프* + *SceneRenderer* 로 전환된 데모/엔진.
C++17 / CMake 단독 빌드. macOS·Linux (Ninja) + Windows (MSVC) 크로스 플랫폼.

> **교수 제출용 — vcpkg 미사용.** 모든 서드파티는 `lib/`·`include/` 사전 빌드 산출물을 체크인해 CMake 단독으로 완결된다.
> 재빌드는 `shell/BuildExternLibs.{sh,bat}`. 본 문서는 `src/` 코어 모듈 16종 + `doc/pages/` 가이드를 대상으로 한다 (`apps/` 데모는 범위 외).

## 모듈 레이어 다이어그램

> 개념적 레이어 뷰. 정확한 클래스 단위 관계는 각 클래스 페이지의 자동 collaboration graph 참조.
> 16개 코어 모듈은 INTERFACE 우산 `SJH::engine` 으로 묶여 한 줄 link 된다
> (`target_link_libraries(<demo> PRIVATE project_deps SJH::engine)`).

\dot
digraph EngineArchitecture {
  rankdir=BT;
  node [shape=box, style=rounded, fontname="Helvetica"];
  compound=true;

  subgraph cluster_app {
    label="앱 (Client — 라이프 사이클)"; style=dashed;
    MyApp [label="apps/_MyApp_\n(탑다운 슈터)", style="rounded,filled", fillcolor="#fff7d6"];
  }

  subgraph cluster_umbrella {
    label="우산 (INTERFACE)"; style=dashed;
    engine [label="SJH::engine", style="rounded,filled", fillcolor="#e8f5e9"];
  }

  subgraph cluster_content {
    label="콘텐츠 / 게임플레이"; style=dashed;
    sprite; fsm; playable; input; resource_registry; timer;
  }

  subgraph cluster_scene {
    label="씬 / 렌더"; style=dashed;
    scene; render;
  }

  subgraph cluster_gl {
    label="GL 자원 / 머티리얼"; style=dashed;
    buffer; shader; program; layout; material; object;
  }

  subgraph cluster_base {
    label="기반"; style=dashed;
    common; diagnostics;
  }

  subgraph cluster_project_deps {
    label="project_deps (GL/윈도우)"; style=filled; fillcolor="#f0f0f0";
    sb7; glfw3; OpenGL;
  }

  subgraph cluster_game_deps {
    label="game_deps (게임/엔진)"; style=filled; fillcolor="#eef3fb";
    box2d; Effekseer; assimp; spdlog; tweeny; stb; FMOD [label="fmod\n(조건부)"];
  }

  // 앱 -> 우산 -> 모듈 그룹 (레이어 흐름)
  MyApp   -> engine;
  MyApp   -> FMOD [lhead=cluster_game_deps, style=dashed, label="POST_BUILD dll copy"];
  engine  -> sprite    [lhead=cluster_content];
  engine  -> scene     [lhead=cluster_scene];
  engine  -> buffer    [lhead=cluster_gl];
  engine  -> common    [lhead=cluster_base];

  // 코어 모듈 -> 외부
  buffer  -> sb7    [ltail=cluster_gl, lhead=cluster_project_deps];
  scene   -> sb7    [ltail=cluster_scene, lhead=cluster_project_deps];
  resource_registry -> box2d [lhead=cluster_game_deps, label="PUBLIC\n(FMOD/Effekseer/stb)"];
  diagnostics -> spdlog [style=dotted];
}
\enddot

## 빌드 / 실행 흐름 (high-level)

\dot
digraph RunFlow {
  rankdir=LR;
  node [shape=box, fontname="Helvetica"];
  Start    [label="apps/_MyApp_/main.cpp", shape=ellipse];
  init     [label="sb7::application::init()\nGL 4.1 Core / GLSL 410"];
  build    [label="Director + SceneRenderer\nActor/Component 씬 구성\nResourceRegistry 자원 위탁"];
  loop     [label="while (running)\n  Update(dt)\n  SceneRenderer::Render()\n  swapBuffers",
            shape=box, style="rounded,filled", fillcolor="#fff7d6"];
  term     [label="자원 자동 파괴 (UPtr)", shape=ellipse];
  Start -> init -> build -> loop -> term;
}
\enddot

## 씬 시스템 관계 (아키텍처 스케치)

> 권위 있는 정의는 각 클래스 페이지의 자동 그래프 + `src/scene/`·`src/render/` 헤더.
> 본 다이어그램은 새 Component/Playable 를 어디에 끼울지 잡기 위한 개념 스케치.

\dot
digraph SceneSystem {
  rankdir=BT;
  node [shape=box, style=rounded, fontname="Helvetica"];

  Scene; Actor; Component; Director;
  SceneRenderer; DeviceContext; MeshPassProcessor;
  Material; Program; Texture; IPlayable;

  Scene  -> Actor      [label="보유"];
  Actor  -> Component  [label="보유 (Actor 비상속)"];
  Director -> IPlayable [label="구동 (Play/Update)", style=dashed];
  Director -> SceneRenderer [label="Render 위임", style=dashed];
  SceneRenderer -> DeviceContext [label="glUseProgram owner"];
  SceneRenderer -> MeshPassProcessor;
  SceneRenderer -> Component [label="Light/Camera 수집", style=dashed];
  Material -> Program [label="const* (비소유)", style=dashed];
  Material -> Texture [label="const* (비소유)", style=dashed];

  edge [style=dotted, color="#9aa6b8"];
  Program -> "SJH::Diagnostics" [label="컴파일/uniform 진단"];
}
\enddot

## 코어 모듈 (16종)

| 모듈 | 책임 |
|------|------|
| `common` | 공통 유틸 (`common.h`), GL 로더 비의존 |
| `diagnostics` | GL 호출/셰이더/uniform/상태 진단 + `GLValidate` Cat A–F |
| `buffer` | VBO/EBO 통합 RAII (`Buffer`) |
| `shader` | 셰이더 컴파일 + InfoLog (`Shader::CreateFromSource`) |
| `program` | 프로그램 링킹 + uniform 핸들/캐시 |
| `layout` | Vertex 레이아웃 (`vertex.h`) + VAO attribute setter |
| `material` | Phong/PBR Material 값 클래스 + `Texture*`/`Program*` 보관 |
| `object` | Mesh + Geometry 생성기 + `Light`/`Transform` POD |
| `scene` | Actor + Component + Scene 그래프 (Actor 비상속) |
| `render` | DeviceContext + SceneRenderer + MeshPassProcessor |
| `sprite` | 2D 스프라이트 atlas + `SpriteSequencePlayable` + `UniformAtlas` |
| `fsm` | `StateMachine<TState, TOwner>` + `IFsmState<TOwner>` |
| `playable` | `IPlayable` + `PlayableBase` + Composite (Sequence/Parallel) |
| `input` | `KeyboardInput<TAction>` / `MouseInput` 디스패치 |
| `resource_registry` | 9종 자원 캐시 (Texture/Material/Model/Program/Mesh/Framebuffer/UniformAtlas/Sound/Effect). game_deps PUBLIC |
| `timer` | 게임플레이 타이머 |

## 추가 페이지

- @ref build-system "빌드 시스템 가이드"
- @ref dependencies "의존 라이브러리 (project_deps / game_deps)"

> **클래스 의존 그래프 갱신:** 새 클래스를 추가했거나 멤버 구성이 바뀌면
> `.claude/skills/doxygen-class-graph/` 절차에 따라 위 "씬 시스템 관계" 스케치를 보강한다.
