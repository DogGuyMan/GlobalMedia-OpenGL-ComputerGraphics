# OpenGL-With-CMake {#mainpage}

OpenGL 학습 프로젝트 — C++17 + CMake + vcpkg manifest mode.

## 모듈 레이어 다이어그램

\dot
digraph LayerArchitecture {
  rankdir=BT;
  node [shape=box, style=rounded, fontname="Helvetica"];

  subgraph cluster_app {
    label="앱 (라이프 사이클)";
    style=dashed;
    main [label="app/main.cpp"];
  }

  subgraph cluster_scene {
    label="씬 캡슐화 (Context)";
    style=dashed;
    Context;
  }

  subgraph cluster_gl {
    label="GL 자원 / 셰이더";
    style=dashed;
    Program; Shader;
    Buffer [label="Buffer\n(VBO/EBO)"];
    VertexLayout [label="VertexLayout\n(VAO)"];
    ResourceRegistry [label="ResourceRegistry\n(Texture/Material/Model)"];
  }

  subgraph cluster_objects {
    label="씬 객체";
    style=dashed;
    Camera;
  }

  subgraph cluster_base {
    label="기반";
    style=dashed;
    Common; Diagnostics;
  }

  subgraph cluster_external {
    label="외부 (vcpkg)";
    style=filled; fillcolor="#f0f0f0";
    glfw; glad; spdlog; fmt; glm; stb;
  }

  main -> Context;
  main -> spdlog;
  main -> glfw;
  main -> glad;

  Context -> Program;
  Context -> Shader;
  Context -> Buffer;
  Context -> VertexLayout;
  Context -> ResourceRegistry;
  Context -> Camera;
  Context -> Diagnostics;
  Context -> glm;
  Context -> glad;

  Program -> Shader;
  Program -> Diagnostics;
  Program -> Common;
  Program -> glad;

  Shader -> Common;
  Shader -> Diagnostics;
  Shader -> glad;

  Buffer -> Common;
  Buffer -> Diagnostics;
  Buffer -> glad;

  VertexLayout -> Common;
  VertexLayout -> Diagnostics;
  VertexLayout -> glad;

  ResourceRegistry -> Common;
  ResourceRegistry -> Diagnostics;
  ResourceRegistry -> glad;
  ResourceRegistry -> glm;
  ResourceRegistry -> stb;

  Camera -> glm;

  Common -> spdlog;
  Diagnostics -> spdlog;
  Diagnostics -> fmt;
  Diagnostics -> glad;
}
\enddot

## 렌더링 시퀀스 (high-level)

\dot
digraph RenderSequence {
  rankdir=LR;
  node [shape=box, fontname="Helvetica"];
  Start [label="main()", shape=ellipse];
  init [label="GLFW init\n+ glad load"];
  ctx [label="Context::Create()\n→ Shader → Program\n→ VAO → VBO/EBO\n→ Texture (Image: transient)"];
  uptr [label="glfwSetWindowUserPointer(window, ctx)\n(콜백에서 Context 역참조용)"];
  callbacks [label="GLFW callbacks 등록\n(framebuffer / key / cursor / mouse)"];
  loop [label="while (!shouldClose)\n  ProcessInput()\n  Render()\n   swapBuffers + pollEvents",
        shape=box, style="rounded,filled", fillcolor="#fff7d6"];
  term [label="GLFW terminate", shape=ellipse];
  Start -> init -> ctx -> uptr -> callbacks -> loop -> term;
}
\enddot

## 입력 → 카메라 위임 흐름 (Phase 7, 커밋 `3696136` 반영)

\dot
digraph InputDelegation {
  rankdir=LR;
  node [shape=box, fontname="Helvetica"];

  user    [label="사용자 입력", shape=ellipse, style=filled, fillcolor="#fff7d6"];
  glfw    [label="GLFW 콜백\n(main.cpp Handle*)", style=filled, fillcolor="#e8f0ff"];
  uptr    [label="glfwGetWindowUserPointer()\n→ SJH::Context*", shape=note, style=filled, fillcolor="#fff3e0"];
  context [label="Context 위임 메서드\nProcessInput / MouseMove\nMouseButton / Reshape", style=filled, fillcolor="#e8f5e9"];
  camera  [label="Camera 상태\nmPos / mEulerYaw / mEulerPitch\nmIsCamControl / mAspect", style=filled, fillcolor="#fce4ec"];

  user -> glfw [label="키 / 마우스 / 리사이즈"];
  glfw -> uptr [label="콜백 진입 시 캐스팅"];
  uptr -> context;
  context -> camera [label="mPos += speed*GetFront()\nmEulerYaw/Pitch ±= delta\nmIsCamControl 토글\nSetAspect(w,h)"];
  camera -> context [label="GetForwardViewMatrix()\nGetProjMatrix()\nGetFront()", style=dashed];
}
\enddot

## 클래스 의존 그래프 (전 프로젝트 통합)

> 모듈 단위가 아닌 *클래스* 단위로 본 의존 관계.
> 자동 collaboration graph 는 클래스 페이지마다 따로 생성되지만,
> 본 그래프는 *전체 프로젝트* 를 한 화면에 보여 줘 새 멤버를 어디 끼워야 할지 한눈에 잡기 위한 것.

\dot
digraph ClassDependencyGraph {
  rankdir=BT;
  compound=true;
  node [shape=box, style=rounded, fontname="Helvetica"];

  // 정적 진단 헬퍼 (인스턴스 없음, 모두 static)
  subgraph cluster_diag {
    label="Diagnostics (static-only + POD struct)"; style=dashed; color="#aaaaaa";
    GLObjectLog; GLDebug; UniformDiagnostics; GLStateLog;
    // POD struct (Phase 9) — 노드 모양 구분
    GLStateFields    [shape=note];
    VertexAttribInfo [shape=note];
  }

  // 자원 / 데이터
  subgraph cluster_resource {
    label="Resource"; style=dashed; color="#aaaaaa";
    Image; Texture; ResourceRegistry;
  }

  // GL 객체 RAII + 메시/프레임버퍼 (Phase 16+)
  subgraph cluster_gl {
    label="GL Object RAII + Mesh/FBO"; style=dashed; color="#aaaaaa";
    Shader; Program; Buffer; VertexLayout; Mesh; Framebuffer;
  }

  // 씬 + 라이팅/머티리얼/모델 (Phase 12+)
  subgraph cluster_scene {
    label="Scene + Light/Material/Model + SceneGraph"; style=dashed; color="#aaaaaa";
    Camera; Light; DirLight; PointLight; SpotLight; Material; Model; Context;
    Transform; SceneNode; SceneGraph;
  }

  // 입력 디스패치 — 콜백 바인딩 (논리 액션 계층)
  subgraph cluster_input {
    label="Input"; style=dashed; color="#aaaaaa";
    KeyboardInput; MouseInput;
  }

  // 소유 관계 (실선) — 멤버로 보유, 수명 결합
  Context -> Program          [label="UPtr ×4\n(lighting/simple/texture/post)"];
  Context -> ResourceRegistry [label="UPtr (mRM)"];
  Context -> Camera           [label="value"];
  Context -> DirLight         [label="value (mDirLight)"];
  Context -> PointLight       [label="value ×2\n(mPointLights)"];
  Context -> SpotLight        [label="value (mSpotLight)"];
  Context -> Mesh             [label="MeshUPtr ×2\n(mBox + mPlane)"];
  Context -> Framebuffer      [label="FramebufferUPtr"];
  Context -> SceneGraph       [label="value (mScene)"];
  Context -> KeyboardInput    [label="value (mKeyboard)\n<GameAction>"];
  Context -> MouseInput       [label="value (mMouse)"];

  Mesh -> VertexLayout        [label="UPtr"];
  Mesh -> Buffer              [label="BufferPtr ×2\n(VBO/EBO)"];

  ResourceRegistry -> Texture   [label="map<name, UPtr>"];
  ResourceRegistry -> Material  [label="map<name, UPtr>"];
  ResourceRegistry -> Model     [label="map<name, UPtr>"];

  Model -> Mesh     [label="vector<RenderUnit>"];
  Model -> Material [label="vector<MaterialUPtr>"];
  Model -> Texture  [label="vector<TextureUPtr>"];

  Framebuffer -> Texture [label="TexturePtr\n(색상 어태치먼트)"];

  SceneGraph -> SceneNode  [label="array<SceneNode, Count>"];
  SceneNode  -> Transform  [label="value (mLocal)"];

  // 입력 의존 (긴 점선) — 멤버 X, 팩토리 인자 / 이름 키 해석 / 비소유 관찰자
  edge [style=dashed, color="#5b6b80"];
  Program  -> Shader   [label="vector<ShaderPtr>\n(Create 인자)"];
  Texture  -> Image    [label="Image*\n(CreateTexture 인자)"];
  Material -> Texture  [label="const Texture*\n관찰자 (비소유)"];
  Material -> Program  [label="const Program*\n관찰자 (비소유)"];
  SceneNode -> SceneNode [label="SceneNode* 부모 / vector<SceneNode*> 자식\n(비소유 관찰자)"];

  // 정적 진단 사용 (짧은 점선) — 인스턴스 X
  edge [style=dotted, color="#9aa6b8", fontcolor="#9aa6b8"];
  Shader       -> GLObjectLog;
  Program      -> GLObjectLog;
  Program      -> UniformDiagnostics;
  Buffer       -> GLDebug;
  VertexLayout -> GLDebug;

  // GL State 진단 (Phase 9) — Log → Fields → struct 종속
  GLStateLog       -> GLStateFields    [label="CaptureGLState\nFieldsToString"];
  GLStateFields    -> VertexAttribInfo [label="array<.., 16>"];
}
\enddot

**범례**

| 선 / 노드 | 의미 |
|---|---|
| 실선 | *소유* — 멤버 변수로 보유. 부모 소멸 시 자식도 소멸 (`UPtr` / value 멤버). |
| 긴 점선 | *입력 의존* — 팩토리/생성자 인자. 수명 결합 없음. |
| 짧은 점선 | *정적 호출* — 인스턴스 없이 free/static 함수만 사용. |
| 사각 노드 | 클래스 (멤버 함수 + 캡슐화). |
| 노트 모양 노드 | POD struct — 모든 필드 public, 동작 없음 (예: `GLStateFields`, `VertexAttribInfo`). |

> 갱신 방법: 새 클래스를 추가했거나 멤버 구성이 바뀌면 `.claude/skills/doxygen-class-graph/` 의 절차를 따른다.

## 추가 페이지

- @ref build-system "빌드 시스템 가이드"
- @ref dependencies "의존 라이브러리"

## 핵심 모듈

| 모듈 | 클래스 | 역할 |
|------|--------|------|
| common | — | @c CLASS_PTR 매크로 + @ref SJH::LoadTextFile |
| diagnostics | @ref SJH::Diagnostics::GLObjectLog "GLObjectLog", @ref SJH::Diagnostics::GLDebug "GLDebug", @ref SJH::Diagnostics::UniformDiagnostics "UniformDiagnostics" | GL 에러 로깅 + uniform warn-once 일원화 |
| shader | @ref SJH::Shader "Shader" | 셰이더 컴파일 RAII |
| program | @ref SJH::Program "Program" + `SJH::Uniforms::*` 자유 함수 | 프로그램 링크 RAII + uniform setter (캐시 친구 함수) |
| buffer | @ref SJH::Buffer "Buffer" | VBO/EBO RAII (`glGenBuffers` ~ `glDeleteBuffers`) |
| layout | @ref SJH::VertexLayout "VertexLayout" | VAO + `glVertexAttribPointer` 진단 통합 |
| resource_registry | @ref SJH::ResourceRegistry "ResourceRegistry", @ref SJH::Image "Image", @ref SJH::Texture "Texture" | Texture/Material/Model 이름 키 캐시 — `Create*`/`Find*` 동사 분리, 세션 수명 불변식 |
| object | @ref SJH::Camera "Camera" | 카메라 상태 + view/projection 행렬 산출 |
| context | @ref SJH::Context "Context" | 씬 자원 + 매 프레임 draw + GLFW 콜백 위임 진입점 |
