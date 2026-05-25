@mainpage SJH::render — Module Dependency Overview

# 개요

`src/render` 모듈은 *SceneRenderer*, *MeshPassProcessor*, *DeviceContext*, *RenderTarget*, *PipelineStateSetter*, *PropertyBlockSetter*, *IRenderStage* 등 GL 호출의 단일 게이트웨이 + draw command 오케스트레이션을 담당합니다.

본 문서는 `src/render` 가 *다른 SJH:: 모듈* 과 어떻게 얽혀있는지, 그리고 *내부 클래스 협력 관계* 를 한 페이지에 모아 시각화합니다.

> **공통 토대 (그래프 1 캡션)** — `project_deps` (GL / sb7 / glfw3) 는 *거의 모든* SJH 모듈의 PUBLIC 의존이므로 그래프 1에서는 노드로 표시하지 않습니다. `common` / `fsm` 을 제외한 모든 모듈이 `project_deps` 에 PUBLIC link 됩니다.

---

# 1. 전체 SJH:: 모듈 의존 그래프

실선(굵음) = `PUBLIC` (헤더 노출), 점선 = `PRIVATE` (.cpp 전용).
총 14개 SJH 모듈 중 `SJH::render` (강조 노드) 는 *7개 모듈* 과 link 됩니다.

@dot
digraph SJH_modules {
    rankdir=LR;
    bgcolor="transparent";
    compound=true;
    node  [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=11,
           fontcolor="#e6edf3", color="#30363d"];
    edge  [fontname="Helvetica", fontsize=9, color="#7d8590", fontcolor="#7d8590"];

    // ── render 강조 ────────────────────────────────────────────
    render            [label="SJH::render",            fillcolor="#1f6feb", color="#388bfd", penwidth=2, fontsize=13];

    // ── 다른 SJH 모듈 ──────────────────────────────────────────
    common            [label="SJH::common",            fillcolor="#21262d"];
    diagnostics       [label="SJH::diagnostics",       fillcolor="#21262d"];
    program           [label="SJH::program",           fillcolor="#21262d"];
    shader            [label="SJH::shader",            fillcolor="#21262d"];
    material          [label="SJH::material",          fillcolor="#21262d"];
    buffer            [label="SJH::buffer",            fillcolor="#21262d"];
    layout            [label="SJH::layout",            fillcolor="#21262d"];
    object            [label="SJH::object",            fillcolor="#21262d"];
    scene             [label="SJH::scene",             fillcolor="#21262d"];
    resource_registry [label="SJH::resource_registry", fillcolor="#21262d"];
    input             [label="SJH::input",             fillcolor="#21262d"];
    sprite            [label="SJH::sprite",            fillcolor="#21262d"];
    fsm               [label="SJH::fsm (INTERFACE)",   fillcolor="#21262d", style="rounded,filled,dashed"];

    // ── PUBLIC (실선 굵음) ──────────────────────────────────────
    edge [penwidth=2, color="#3fb950"];

    render            -> common;
    render            -> program;

    buffer            -> common;
    buffer            -> program;
    buffer            -> render;
    buffer            -> resource_registry;

    layout            -> common;
    layout            -> buffer;

    material          -> common;
    material          -> program;

    object            -> common;
    object            -> buffer;
    object            -> layout;
    object            -> material;
    object            -> scene;

    program           -> common;
    program           -> shader;

    resource_registry -> common;
    resource_registry -> object;
    resource_registry -> material;
    resource_registry -> program;

    scene             -> common;
    scene             -> object;

    shader            -> common;

    sprite            -> scene;
    sprite            -> resource_registry;

    fsm               -> scene;

    // ── PRIVATE (점선) ─────────────────────────────────────────
    edge [penwidth=1, style=dashed, color="#d29922"];

    render            -> diagnostics;
    render            -> material;
    render            -> object;
    render            -> resource_registry;
    render            -> scene;
    render            -> buffer;

    buffer            -> diagnostics;
    layout            -> diagnostics;
    program           -> diagnostics;
    program           -> material;
    resource_registry -> diagnostics;
    shader            -> diagnostics;
}
@enddot

> **참고** — `buffer ↔ render` 와 `object ↔ scene` 은 양방향 link 처럼 보이지만, *forward decl + PUBLIC/PRIVATE 분리* 로 컴파일 순환은 없습니다 (`buffer/framebuffer.h` 가 `RenderTarget` 상속, `render/scene_renderer.cpp` 만 `framebuffer.h` 사용).

---

# 2. src/render 내부 클래스 협력 그래프 (SSoT 리팩토링 반영)

**SSoT 리팩토링 핵심**: `DrawCommand` 가 더 이상 `program/mesh/material/actor` 4개 외부 포인터를 직접 보유하지 않고, **`Scene::MeshRenderer*` 단일 포인터** 만 보유합니다. 나머지는 모두 `meshRenderer` 경유 접근 (Filament/Unreal/Cocos 정통).

@dot
digraph render_internal {
    rankdir=TB;
    bgcolor="transparent";
    node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=11,
          fontcolor="#e6edf3", color="#30363d", fillcolor="#21262d"];
    edge [fontname="Helvetica", fontsize=9, color="#7d8590", fontcolor="#7d8590"];

    // ── 추상 (abstract) ─────────────────────────────────────────
    IRenderStage  [label="«interface»\nIRenderStage",  fillcolor="#3a2d5a", color="#8957e5", style="rounded,filled,dashed"];
    RenderTarget  [label="«abstract»\nRenderTarget",   fillcolor="#3a2d5a", color="#8957e5", style="rounded,filled,dashed"];

    // ── 구체 — render 모듈 핵심 (파랑) ────────────────────────
    SceneRenderer        [label="SceneRenderer\n(High-level Orchestrator)",       fillcolor="#1f6feb", color="#388bfd", penwidth=2];
    MeshPassProcessor    [label="MeshPassProcessor\n(Low-level Orchestrator)",    fillcolor="#1f6feb", color="#388bfd"];
    DeviceContext        [label="DeviceContext\n(singleton — GL gateway)",        fillcolor="#1f6feb", color="#388bfd"];
    DefaultRenderTarget  [label="DefaultRenderTarget\n(FBO 0 backbuffer)"];
    PipelineStateSetter  [label="PipelineStateSetter\n(GL state Applier)"];
    PropertyBlockSetter  [label="PropertyBlockSetter\n(namespace — uniforms+tex)"];
    DrawCommand          [label="DrawCommand\n{ meshRenderer*,\n  modelMatrix,\n  queueLayer, depth }", fillcolor="#3a4d2d", color="#3fb950"];

    // ── scene 모듈 핵심 — SSoT 의 진입점 (초록 강조) ──────────
    MeshRenderer  [label="Scene::MeshRenderer\n{ Mesh*, Material*, QueueOffset,\n  Visible }\n(SSoT 진입점)", fillcolor="#2d5a3d", color="#3fb950", penwidth=2];

    // ── 외부 노드 (회색 — meshRenderer 경유 접근) ─────────────
    Program      [label="SJH::Program",                fillcolor="#161b22", color="#484f58"];
    Mesh         [label="SJH::Mesh",                   fillcolor="#161b22", color="#484f58"];
    Material     [label="SJH::Material\n{ Properties, Pass, Program* }", fillcolor="#161b22", color="#484f58"];
    Pass         [label="SJH::Pass::PipelineState",    fillcolor="#161b22", color="#484f58"];
    MPB          [label="SJH::MaterialPropertyBlock",  fillcolor="#161b22", color="#484f58"];
    Light        [label="SJH::Dir/Point/SpotLight",    fillcolor="#161b22", color="#484f58"];
    Camera       [label="Scene::Camera",               fillcolor="#161b22", color="#484f58"];
    Actor        [label="Scene::Actor",                fillcolor="#161b22", color="#484f58"];
    Director     [label="Scene::Director\n(singleton — Root())", fillcolor="#161b22", color="#484f58"];
    Framebuffer  [label="SJH::Framebuffer\n(buffer 모듈)", fillcolor="#161b22", color="#484f58"];

    // ── 상속 (보라 △) ──────────────────────────────────────────
    edge [arrowhead=empty, color="#8957e5", penwidth=1.5];
    SceneRenderer       -> IRenderStage;
    DefaultRenderTarget -> RenderTarget;
    Framebuffer         -> RenderTarget;
    MeshRenderer        -> Actor [style=dashed, label="Component of", color="#8957e5"];

    // ── 컴포지션 (초록 ◆) ──────────────────────────────────────
    edge [arrowhead=diamond, color="#3fb950", penwidth=1.5, style=solid, label=""];
    SceneRenderer -> MeshPassProcessor [label="composes\n(mProcessor)"];
    MeshPassProcessor -> DrawCommand   [label="vector<>"];
    DrawCommand -> MeshRenderer        [label="ptr (SSoT)"];

    // ── 사용 (노랑 → ) ─────────────────────────────────────────
    edge [arrowhead=open, color="#d29922", penwidth=1, style=solid];

    // SceneRenderer 의 의존
    SceneRenderer       -> RenderTarget        [label="Render(target)"];
    SceneRenderer       -> Director            [label="Get().Root()"];
    SceneRenderer       -> Camera              [label="collect, sort by Depth"];
    SceneRenderer       -> Actor               [label="DFS traverse"];
    SceneRenderer       -> Light               [label="collect (Dir/Point/Spot)"];
    SceneRenderer       -> Program             [label="SendLightUniforms"];
    SceneRenderer       -> DeviceContext       [label="BeginFrame/UseProgram"];
    Camera              -> RenderTarget        [label="GetTargetRenderTarget()"];

    // MeshPassProcessor 의 의존
    MeshPassProcessor   -> DeviceContext       [label="Process(rc, ...)"];
    MeshPassProcessor   -> PipelineStateSetter [label="per-cmd Set"];
    MeshPassProcessor   -> PropertyBlockSetter [label="per-mat Set"];
    MeshPassProcessor   -> MeshRenderer        [label="cmd.meshRenderer\n경유 접근", color="#3fb950"];

    // SSoT 접근 경로 — MeshRenderer 가 program/mesh/material 의 진입점
    edge [color="#3fb950", style=dotted, penwidth=1];
    MeshRenderer        -> Mesh                [label="Mesh*"];
    MeshRenderer        -> Material            [label="Material*"];
    Material            -> Program             [label="GetProgram()"];
    Material            -> Pass                [label="GetPass()"];

    // Setter 의 의존
    edge [color="#d29922", style=solid, penwidth=1];
    DeviceContext       -> Program             [label="UseProgram"];
    DeviceContext       -> RenderTarget        [label="BeginFrame/Bind"];
    PipelineStateSetter -> Pass                [label="Set(want)"];
    PropertyBlockSetter -> MPB                 [label="reads"];
    PropertyBlockSetter -> Program             [label="writes uniforms"];
    PropertyBlockSetter -> DeviceContext       [label="param"];
}
@enddot

## 노드 색상 범례

| 색상 | 의미 |
|---|---|
| 🔵 파랑 (filled) | `src/render` 의 핵심 구체 클래스 |
| 🟢 초록 (강조) | **SSoT 진입점** — `Scene::MeshRenderer` (모든 program/mesh/material 접근은 이걸 경유) |
| 🟢 초록 (연한) | `DrawCommand` — 이제 `meshRenderer*` 만 보유 |
| 🟣 보라 (dashed) | 추상/인터페이스 (`IRenderStage`, `RenderTarget`) |
| ⚫ 회색 | meshRenderer 경유 접근되는 외부 모듈 타입 |

## 엣지 색상 범례

| 색상 | 화살촉 | 의미 |
|---|---|---|
| 🟣 보라 | empty (△) | 상속 (`: public X`) |
| 🟢 초록 실선 | diamond (◆) | 컴포지션 (값 보유, vector / member) |
| 🟢 초록 점선 | open (→) | **SSoT 접근 경로** — meshRenderer 경유 chain |
| 🟡 노랑 실선 | open (→) | 사용 (파라미터, 함수 호출) |

## SSoT 접근 경로 — 코드와 1:1 매칭

`MeshPassProcessor::Process()` 안에서 (`mesh_pass_processor.cpp:96-99`):
```cpp
if (!cmd.meshRenderer) continue;
const Material* material = cmd.meshRenderer->Material;
const Mesh*     mesh     = cmd.meshRenderer->Mesh;
const Program*  program  = material->GetProgram();
```
이 4줄이 그래프 2의 초록 점선 chain 입니다 — `DrawCommand → MeshRenderer → {Mesh, Material → Program}`.

---

# 3. 자동 생성 그래프 진입점

위 두 그래프는 **수동으로 그린 큰 그림** 입니다. Doxygen 이 자동 생성한 *파일/클래스별 상세 그래프* 는 좌측 트리뷰 또는 아래 링크에서:

- [Files 페이지](files.html) — 8개 헤더 + 7개 cpp 의 include 그래프
- [Classes 페이지](annotated.html) — 9개 클래스의 collaboration / inheritance 그래프
- [Directories 페이지](dirs.html) — `src/render` 디렉토리 의존 그래프
