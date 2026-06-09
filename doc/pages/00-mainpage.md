# OpenGL-ComputerGraphics {#mainpage}

**SJH 엔진** — SuperBible 7th Edition 코스워크에서 출발해 *Core/Client 분리* + *Actor/Component 씬 그래프* + *SceneRenderer* 로 전환된 데모/엔진.
C++17 / CMake 단독 빌드. macOS·Linux (Ninja) + Windows (MSVC) 크로스 플랫폼.

> **교수 제출용 — vcpkg 미사용.** 모든 서드파티는 `lib/`·`include/` 사전 빌드 산출물을 체크인해 CMake 단독으로 완결된다.
> 재빌드는 `shell/BuildExternLibs.{sh,bat}`. 본 문서는 `src/` 코어 모듈 16종 + `doc/pages/` 가이드를 대상으로 한다 (`apps/` 데모는 범위 외).

## 모듈 의존 그래프 (`src/` 내부)

> `src/<module>/` 코드의 **실제 인스턴스 의존**만 표시한다 — `A → B` 는 *A 가 B 의 클래스 인스턴스를 소유·보유·생성하거나, B 인스턴스의 비-static 멤버를 호출/상속* 함을 뜻한다.
>
> **노이즈 제외 규칙** (의존 엣지를 그리지 않는 경우):
> ① **static 전용 유틸 클래스** 접근 (예: `diagnostics` 의 `GLObjectLog::Check*` 등)
> ② **namespace free function** 호출 (예: `Uniforms::Set*`, `Geometry::*`, `LoadTextFile`)
> ③ **매크로** (`CLASS_PTR`)
>
> (산출: `src/*/CMakeLists.txt` link + cross-module `#include` + 사용 형태(인스턴스 vs static/free-func) 분석. 클래스 단위 관계는 각 클래스 페이지의 자동 collaboration graph 참조.)

\dot
digraph ModuleDeps {
  rankdir=TB;
  node [shape=box, style=rounded, fontname="Helvetica", fontsize=11];
  edge [color="#5b6b80", arrowsize=0.7];

  // 기반 유틸 — static/macro/free-func 라 의존 엣지를 의도적으로 생략 (분리 박스)
  subgraph cluster_util {
    label="기반 유틸 — 의존 엣지 생략 (static · macro · free-func)";
    style=filled; fillcolor="#f4f4f4"; fontsize=10;
    node [style="rounded,filled", fillcolor="#eaeaea"];
    common      [label="common\nCLASS_PTR 매크로 · LoadTextFile()"];
    diagnostics [label="diagnostics\nGL* / Effekseer static 진단"];
    input       [label="input\nKeyboardInput<T> · MouseInput (leaf)"];
  }

  // 인스턴스 의존 노드
  node [style=rounded];
  shader; program; layout; material; object; buffer;
  scene; render; resource_registry; sprite; playable; fsm; timer; text;

  // ── 상호 의존 (응집 핫스팟) — 빨강 양방향 ──
  edge [color="#c0392b", dir=both, penwidth=1.3];
  scene  -> object            [label="Transform ↔ Light:Component"];
  scene  -> render            [label="comp 생성 ↔ comp 수집"];
  buffer -> resource_registry [label="Framebuffer ↔ Texture 캐시"];

  // ── 단방향 인스턴스 의존 ──
  edge [color="#5b6b80", dir=forward, penwidth=1.0];
  program -> shader;
  material -> program;
  object -> buffer;
  object -> layout;
  object -> material;
  object -> resource_registry;
  buffer -> render;
  resource_registry -> material;
  resource_registry -> object;
  resource_registry -> program;
  resource_registry -> sprite;
  scene -> buffer;
  scene -> material;
  render -> buffer;
  render -> material;
  render -> object;
  render -> program;
  render -> resource_registry;
  sprite -> material;
  sprite -> object;
  sprite -> playable;
  sprite -> render;
  sprite -> resource_registry;
  playable -> scene;
  fsm -> scene;
  timer -> scene;
  text -> resource_registry;
  text -> scene;
  text -> sprite;
}
\enddot

**범례**

| 표기 | 의미 |
|---|---|
| A → B (회색) | A 가 B 의 인스턴스를 소유·보유·생성하거나 비-static 멤버 호출/상속 (실제 결합) |
| A ↔ B (빨강) | 상호 의존 — 응집 핫스팟 (리팩토링 후보) |
| 기반 유틸 박스 | `common`·`diagnostics`·`input` — 의존 엣지를 **의도적으로 생략** |

**의도적으로 제외한 노이즈 엣지:**
- `→ diagnostics` — 전부 `static` 진단 호출 (`GLObjectLog::Check*`, `GLDebug::*`, `GLValidate::*`, `EffekseerDiagnostics`)
- `→ common` — `CLASS_PTR` 매크로 + `LoadTextFile()` free function 뿐 (인스턴스 클래스 없음)
- `program → object` — 오직 `program_uniforms.cpp` 의 `Uniforms::Set*Light()` **free function** 인자로 `object::Light` 사용
- `sprite·object → program` — 오직 `Uniforms::Set*()` **free function** 호출 (직접 `#include "program/"` 없음)

> ⚠️ **레이어링 이상:** `common/layer.h` 가 `scene/layer.h` 를 re-export (`common → scene`, 역방향). 의존 그래프에선 제외했으나 정리 후보.

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
