<!-- # 렌더 파이프라인 보고서 — MultiPass · PostProcessing · 외부 모듈 통합

> 대상 코드: `src/render/` (엔진 코어) + `apps/_MyApp_/` (실행/통합). 모든 참조는 `파일:줄` 형식.
> 범위: ① MultiPass·PostProcessing 동작 ② 코드 흐름·계층 ③ Effekseer·ImGui 를 렌더 타겟에 묶은 방법.

---

## 1. 계층 구조 한눈에

`src/render` 는 **stage 추상 1개 + 구체 3개 + 협력자 5개**로 구성된다. stage 의 *소유·순서·실행 루프*는 코어가 아니라 **Application(`apps/_MyApp_/main.cpp`)** 이 가진다.

```
IRenderStage (render_stage.h:44, 순수추상: Render(RenderTarget&)=0, OnResize)
 ├─ CameraStage      (camera_stage.h:38)      : SceneRenderer* + Scene::Camera* (둘 다 비소유)
 ├─ ScreenQuadStage  (screen_quad_stage.h:47) : Program& + Mesh& + vector<const Framebuffer*> mSources
 └─ SceneRenderer    (scene_renderer.h:54)    : Render()는 [deprecated], 정통=RenderWithCamera()

Application (main.cpp:438)
  └ vector<unique_ptr<IRenderStage>> mStages   ← 모든 stage 의 실제 소유처

SceneRenderer 의 협력자 (멤버로 값 보유, scene_renderer.h:90-93)
  ├ MeshPassProcessor      : DrawCommand 큐 정렬·발행
  └ LightUniformDispatcher : Light → 모든 Program uniform 송신
MeshPassProcessor 가 호출하는 Applier
  ├ PropertyBlockSetter    : Material 값 → glUniform* (타입별 디스패치)
  └ PipelineStateSetter    : Pass 상태 → glEnable/Depth/Blend/Cull (dirty 캐시)
최하위 GL 래퍼
  └ DeviceContext (싱글톤)  : glUseProgram owner, BindTarget/Clear/BindVAO/DrawIndexed/BindTexture
```

**렌더 타겟**은 `RenderTarget`(추상, `render_target.h:57`) → `DefaultRenderTarget`(백버퍼 FBO 0, `:85`)와 `Framebuffer`(오프스크린 FBO, `buffer/framebuffer.h:46`)로 갈린다. **Camera 가 자기 RT 를 보유**한다(`camera.h:145/150/172`, Unity `Camera.targetTexture` 식) — CameraStage 는 RT 를 만들지 않는다.

---

## 2. MultiPass — stage 벡터 순회

### 2.1 프레임 최상위 루프 (`main.cpp` `render()`)

```
354  out = SceneRenderer().GetLastSceneOutput()                 // 지난 프레임 PostFX 최종 출력 FB
355  mScreenQuadStagePtr->SetSources({ out ? out : mSceneFB })  // 최종 합성 stage 입력 주입
357  for (auto& s : mStages) s->Render(*mDefaultTarget)         // ★ 멀티패스 순차 실행
361  mImGuiStack.RenderAll(...) ; 362 ImGui::Render()           // ImGui 는 stage 밖, 항상 최후
```

`mStages` 는 셋업(`main.cpp:173-188`)에서 vector `insert/push` 로 다음 순서로 고정된다:

```
[0] CameraStage(worldCam)   [1] ParticleStage   [2] CameraStage(screenCam)   [3] ScreenQuadStage
```

CameraStage 가 **2개**(world 원근 + screen 직교)인 것이 핵심이다. world 카메라는 3D 씬을, screen 직교 카메라는 PostFX 패스(=PassComponent)를 처리한다.

### 2.2 각 stage 의 `Render()`

- **CameraStage::Render(target)** (`camera_stage.cpp:26`) — `target` 인자 무시, `mRenderer->RenderWithCamera(*mCamera)` 로 단순 위임(`:33`).
- **ScreenQuadStage::Render(target)** (`screen_quad_stage.cpp:47`) — `target`=백버퍼. `mSources` 의 FB 색 텍스처를 `uScene` 로 받아 백버퍼에 blit (3.3절).
- **SceneRenderer::RenderWithCamera(cam)** — 1패스 코어 (2.3절).

### 2.3 SceneRenderer::RenderWithCamera — 1패스 코어 (`scene_renderer.cpp:71`)

```
73   rt = cam.GetTargetRenderTarget()                  // Camera 가 RT(FBO) 보유. null 이면 skip
86   if (cam.NoClear)  BindTarget(*rt)+Depth/Blend     // screen cam: world 결과 보존(clear 안 함)
     else             DeviceContext::BeginFrame(*rt)   // world cam: bind+clear+depth/blend
97   view=GetViewMatrix; proj=GetProjectionMatrix; cullingMask; viewPos(카메라 월드좌표)
111  Director context 에서 DirLight/PointLight[]/SpotLight[] (enabled 만) 수집
123  programs = ResourceRegistry::GetAllPrograms()
124  mDispatcher.Dispatch(programs, dir, points, spots, viewPos)  // 모든 program 에 light/viewPos
126  mProcessor.Clear()
127  CollectFromActor(Director::Root(), view, cullingMask)        // Actor DFS → DrawCommand Submit
128  mProcessor.SortMultiStage()                                  // stable_sort
129  mProcessor.Process(rc, view, proj)                           // GL draw 발행 (여기서 uProj/uView 송신)
130  mLastSceneOutput = mProcessor.GetLastOutputFB()              // PostFX 마지막 출력 추적
```

> **stage 간 핸드오프**는 CPU 포인터 전달이다: `RenderWithCamera` 가 `mLastSceneOutput`(`const Framebuffer*`)을 갱신 → `GetLastSceneOutput()`(`scene_renderer.h:75`) → `ScreenQuadStage::SetSources()`. 단 `main.cpp:354` 는 *지난 프레임* 값을 읽어 1프레임 지연 소지가 있다(코드에 개발자 의문 주석 잔존).

---

## 3. 코드 흐름 — 메시 1개를 그리는 풀 체인

`MeshPassProcessor::Process` (`mesh_pass_processor.cpp:95`) 가 정렬된 `DrawCommand` 를 돌며, **program/material/state 전환을 결정**하고 Applier 에 위임한다. WorldMesh 한 건 기준(`:142-177`):

```
145  material = cmd.meshRenderer->Material; mesh = ..Mesh; program = material->GetProgram()  // SSoT
152  if (program != lastProg): UseProgram(program) + SetMat4("uView",view) + SetMat4("uProj",proj)
163  if (material != lastMat): PropertyBlockSetter::Set(rc, material->Properties, program)
170  passState = Pass::DefaultPipelineStateOf(material->GetPass()); stateSetter.Set(passState)
174  SetMat4("uModel", cmd.modelMatrix); BindVAO(mesh->GetVAO()); DrawIndexed(mesh->GetIndexCount())
181  (루프 종료) stateSetter.RestoreDefaults()
```

**순서**: `UseProgram → (view/proj) → PropertyBlockSetter → PipelineStateSetter → (model) → BindVAO → DrawIndexed`.

### 3.1 호출 방향 (단방향 위→아래)

```
CameraStage::Render
  └ SceneRenderer::RenderWithCamera                 [수집·조율: "무엇을"]
      ├ LightUniformDispatcher::Dispatch            (형제. 모든 Program 에 light 송신)
      ├ CollectFromActor(DFS) → MeshPassProcessor::Submit
      └ MeshPassProcessor::Process                  [발행·전환결정: "언제"]
          ├ PropertyBlockSetter::Set                [Applier: "어떻게" — uniform]
          ├ PipelineStateSetter::Set                [Applier: "어떻게" — GL state]
          └ DeviceContext (UseProgram/BindVAO/DrawIndexed/BindTexture/BeginFrame)
```

- **수집 vs 발행 분리**: `MeshRenderer`(Component, 데이터) → `SceneRenderer::CollectFromActor`(DrawCommand 빌드) → `MeshPassProcessor`(씬을 모름, DrawCommand 만 받음).
- **uniform 2계층**: ① transform·light = `Uniforms::Set*` 직접 호출 ② material 값·sampler = `PropertyBlockSetter` 경유. **DeviceContext 는 program *바인딩*만, uniform *값*은 우회**(GL state facade 책임만).

### 3.2 PropertyBlockSetter — 타입별 디스패치 (`property_block_setter.cpp:49-104`)

셰이더의 uniform 캐시(schema)를 순회하며 `entry.Type` 으로 분기 → Material 의 typed map(`Floats/Ints/Vec3s/Mat4s/Textures`) 조회 → `glUniform*`. **`GL_BOOL` 은 `GL_INT` 와 fall-through**(`:63-68`)로 `Ints` 맵에서 0/1 송신 — 이 fall-through 가 없으면 `uEnableHit`/`uEnableDissolve` 같은 bool uniform 이 영영 미업로드된다. Sampler 는 `SetInt(unit)+BindTexture(unit, texID)`(`:88-98`).

### 3.3 PipelineStateSetter / LightUniformDispatcher

- **PipelineStateSetter**: `Pass::DefaultPipelineStateOf(Kind)`(`material/pass.h:171`, **GL 상태 SSoT**)가 정한 Stencil/Depth/Cull/Blend 를 적용. `mLast` dirty 캐시로 중복 호출 제거. `CullMode==0` → `glDisable(GL_CULL_FACE)` (AlphaTest 스프라이트의 flipX winding 반전 대응).
- **LightUniformDispatcher** (`light_uniform_dispatcher.cpp`): `viewPos` location<0 인 program(simple/postfx)은 통째 skip. `dirLight` + `pointLights[i]`/`spotLights[i]` (+ `*Enabled[i]` 플래그)를 **항상 16/16 슬롯 전체** 순회 송신 → 셰이더 미초기화 차단.

---

## 4. PostProcessing — PassComponent 선형 체인

### 4.1 체인 빌드 (`render_pipeline.cpp:82-139`, `BuildPostFXChain`)

PostFX 는 **ping-pong(2 FBO 번갈이)이 아니라 패스당 FBO 1개씩 둔 선형 체인**이다.

```
sceneFB → FB_gamma → FB_bloom → FB_fog → FB_vignette → ...
93   prevFB = sceneFB
119  각 패스: fb = Framebuffer::Create(w,h)
129  pc = passActor->AddComponent<PassComponent>(prevFB, fbPtr, mat)   // Input=prevFB, Output=fbPtr
133  prevFB = fbPtr                                                     // 다음 패스 Input = 이번 Output
```

`PassComponent` (`pass_component.h:48`)는 **렌더 로직이 없는 순수 데이터**다:

```cpp
Framebuffer *const InputFB;   // 읽기 소스 (= 이전 패스 OutputFB 와 동일 포인터)
Framebuffer *const OutputFB;  // 쓰기 대상 (= 다음 패스 InputFB 와 동일 포인터, Input!=Output 보장)
Material          *mMaterial; // PostFX 셰이더+state. null/Enabled=false ⇒ bypass blit
int  QueueOffset = 9000;      // 월드 지오메트리(0~8999) 이후 보장
```

`PassActor` 는 `Layer::Screen` 으로 screen 직교 카메라의 자식에 추가된다(`:128,135`) → **screenCam 의 `CollectFromActor` DFS 에 잡혀** ScreenQuad DrawCommand 로 변환된다(`scene_renderer.cpp:161-172`).

### 4.2 패스 실행 — MeshPassProcessor ScreenQuad 분기 (`mesh_pass_processor.cpp:106-140`)

```
118  rc.BeginFrame(*cmd.outputFB)                                   // 출력 FBO bind+clear
119  SetDepthTest(false); SetBlend(false)
122  Properties.Textures["uScene"] = { cmd.inputFB->GetColorAttachment(), 0 }  // 직전 출력 자동 주입
125  UseProgram(prog)
126  PropertyBlockSetter::Set(rc, props, prog)                      // uScene 외 모든 uniform 일괄 송신
128  BindVAO(screenQuad); 130 ebo->Bind();  132 DrawIndexed(...)    // ebo 재핀 = VAO 오염 가드(5.1)
135  mLastOutputFB = cmd.outputFB
```

`uScene`(직전 패스 출력 색)만 코어가 자동 주입한다. **fog 전용 입력은 코어가 모른다** — `apps/_MyApp_/main.cpp` 가 fog Material 의 `Properties` 맵에 직접 써넣고, `PropertyBlockSetter` 가 draw 직전 송신한다:

| uniform | 세팅 위치 | 비고 |
|---|---|---|
| `uScene` | `mesh_pass_processor.cpp:122` | inputFB 색, 매 프레임 자동 |
| `uDepth` | `main.cpp:485` `RebindFogUniforms` | `sceneFB->GetDepthAttachment()`, unit 1 |
| `uInverseProjection` | `main.cpp:347-349` | `mCamera->GetInverseProjectionMatrix()`, 매 프레임 |
| `uFogColor/Mode/Density…` | `main.cpp:152-153` 등 | startup 1회 |

**depth 를 fog 가 샘플하려면 depth 가 RBO 가 아닌 텍스처여야 한다.** 그래서 sceneFB 를 `Framebuffer::CreateWithDepthTexture(w,h)`(`main.cpp:110`)로 만들어 `GL_DEPTH24_STENCIL8` 텍스처를 attach하고 `GetDepthAttachment()`(`framebuffer.h:98`)로 노출한다. 일반 `Create(w,h)` 는 depth 가 RBO 라 fog 불가.

### 4.3 최종 합성 (`screen_quad_stage.cpp:47`)

`ScreenQuadStage` 는 체인 최종 출력 FB 하나를 받아 **백버퍼(FBO 0)** 에 그린다: `BindTarget(target)`+`Clear`(`:58-59`) → `ebo->Bind()` 재핀(`:70`) → `mSources` 순회하며 `BindTexture(0,tex)`+`SetInt("uScene",0)`+`DrawIndexed`(`:82-89`) → 종료 시 `SetDepthTest/Blend(true)` 복원(후속 ImGui 용, `:93-94`).

---

## 5. 외부 모듈을 렌더 타겟에 묶기

핵심 전략: **둘 다 sb7 의 단일 GL 컨텍스트를 공유**하고, Effekseer 는 *엔진 stage 어댑터*로, ImGui 는 *stage 순회 종료 후 직접 호출*로 합류한다.

### 5.1 Effekseer — IRenderStage 어댑터 + sceneFB 합성

**부트** (`apps/_MyApp_/src/VFX/VFXSystem.cpp:38-61`): GraphicsDevice → Renderer → Manager 순. 별도 컨텍스트를 만들지 않고 `EffekseerRendererGL::CreateGraphicsDevice(OpenGL3)` 로 **현재 활성 GL 컨텍스트를 그대로** 점유. SubRenderer 5종(Sprite/Ribbon/Ring/Track/Model) + Loader 4종 주입, `maxSprites=8000`. Manager/Renderer 는 전역 1개씩.

**렌더 타겟 — 화면 직접이 아니라 엔진 sceneFB 에 그린다.** `ParticleStage`(`IRenderStage` 구현)가 `mStages[1]` 로 끼어들어, worldCam 의 RT(=sceneFB)를 bind 한 뒤 Effekseer Draw 를 호출(`ParticleStage.cpp:57-80`):

```cpp
auto* rt = mWorldCam->GetTargetRenderTarget();        // = sceneFB (진실의 원천 단일화)
DeviceContext::Get().BindTarget(*rt);                 // NoClear — worldCam 3D 결과 보존
mVFX->Draw(&mWorldCam->GetViewMatrix()[0][0], &mWorldCam->GetProjectionMatrix()[0][0]);
```

즉 worldCam(stage0)이 sceneFB 에 3D 씬을 그린 **위에** ParticleStage(stage1)가 반투명 파티클을 얹고, 이후 PostFX(stage2)+ScreenQuad(stage3)가 그 합성 결과를 백버퍼로 옮긴다.

**카메라 연동** (`VFXSystem.cpp:84-98`): vmath `mat4` 의 `float[16]` 을 `Effekseer::Matrix44::Values` 로 `memcpy` 후 `SetCameraMatrix`/`SetProjectionMatrix` → `BeginRendering`/`Draw`/`EndRendering`. 둘 다 RH 컬럼메이저라 변환 없이 그대로 전달. **Update** 는 `mManager->Update(dt*60)`(deltaFrame 단위, `:73`) — CombatPlay 상태에서만 호출.

**VAO/EBO 오염 가드**: Effekseer `BeginRendering/Draw` 가 현재 바인딩된 VAO 의 EBO 참조를 덮어쓸 수 있어, 직후 실행되는 `ScreenQuadStage::Render` 가 매 프레임 자기 quad 의 `ebo->Bind()` 를 재핀한다(`screen_quad_stage.cpp:64-71`). 안 하면 `glDrawElements` 가 `GL_INVALID_OPERATION`.

### 5.2 ImGui — v1.53 결합 백엔드, 암묵적 백버퍼

ImGui 는 `extern/imgui` v1.53 의 **결합형 백엔드 `imgui_impl_glfw_gl3`**(현대식 분리형 `ImGui_ImplGlfw`+`ImGui_ImplOpenGL3` 아님)를 executable 에 직접 컴파일한다(`apps/_MyApp_/CMakeLists.txt`). 함수는 `ImGui_ImplGlfwGL3_*` 하나.

**부트** (`apps/_MyApp_/src/UI/UiBootstrap.cpp:31-50`): `CreateContext` → `ImGui_ImplGlfwGL3_Init(window, install_callbacks=false)` (sb7 가 GLFW 콜백 소유하므로 Scroll/Char 만 수동 설치). Init 내부에서 `io.RenderDrawListsFn = ImGui_ImplGlfwGL3_RenderDrawLists` 설정 — **`ImGui::Render()` 가 이 콜백을 내부에서 자동 호출**한다(그래서 main 에 별도 `RenderDrawData` 호출 없음).

**프레임 순서**: `ImGui_ImplGlfwGL3_NewFrame()` 는 루프 최상단(`main.cpp:319`, `WantCapture*` 갱신용) → stages 순회 종료 후 `mImGuiStack.RenderAll()`(위젯 빌드, `:361`) → `ImGui::Render()`(`:362`, 실제 GL 드로우).

**렌더 타겟 = 백버퍼 0 (암묵적)**: `ImGui_ImplGlfwGL3_RenderDrawLists` 는 GL 상태를 save/restore 하지만 **`glBindFramebuffer` 를 호출하지 않는다** → *현재 바인딩된 FBO* 에 그린다. 직전 stage 인 ScreenQuadStage 가 백버퍼(FBO 0)를 bind 하고 끝냈으므로 ImGui 는 그 위에 얹힌다. UI 레이어는 `ImGuiLayerStack`(`vector<unique_ptr<IImGuiLayer>>`)이 Push 순서로 `OnBuildUI()` 순회(PauseButton/PostFXDebug/StateOverlay/VfxSpawn 4종).

---

## 6. 전체 프레임 합성 순서 (요약)

| # | 호출 (main.cpp render()) | 렌더 타겟 | 동작 |
|---|---|---|---|
| 0 | `ImGui_…NewFrame()` (319) | — | UI 프레임 시작, `WantCapture*` 갱신 |
| 0.5 | `StageFsm.Update` → `Manager.Update` → Effekseer `Update(dt*60)`+Physics (328) | — | 게임 sim, 파티클 전진(CombatPlay) |
| 0.7 | `ScreenQuadStage.SetSources({sceneOut})` (354) | — | 최종 합성 입력 주입 |
| 1 | stages[0] `CameraStage(worldCam)`→SceneRenderer | **sceneFB** | clear + 3D WorldMesh |
| 2 | stages[1] `ParticleStage`→VFXSystem.Draw | **sceneFB (NoClear)** | Effekseer 파티클 합성 |
| 3 | stages[2] `CameraStage(screenCam)` + PassComponent 체인 | 중간 FBO 들 | PostFX(fog/bloom/…) 선형 체인 |
| 4 | stages[3] `ScreenQuadStage` | **백버퍼 0** | 체인 결과 blit (ebo 재핀, depth/blend 복원) |
| 5 | `mImGuiStack.RenderAll` (361) | — | UI 위젯 빌드 |
| 6 | `ImGui::Render()` (362) | **백버퍼 0** | UI 드로우(직전 바인딩 FBO 의존) |

**한 줄 결론**: 멀티패스는 Application 의 `mStages` 벡터 순회로 구동되고, Camera 가 자기 RT(FBO)를 보유하며, PostProcessing 은 `PassComponent` 의 InputFB/OutputFB 포인터 공유로 엮은 **패스당-1-FBO 선형 체인**(`MeshPassProcessor::Process` 의 ScreenQuad 분기가 실행)이다. Effekseer 는 worldCam 의 sceneFB 에 그리는 **stage 어댑터**(`ParticleStage`)로, ImGui 는 stage 순회 종료 후 **직전 바인딩(백버퍼 0)에 직접** 합류한다 — 둘 다 sb7 단일 GL 컨텍스트 공유. 코어는 `uScene` 만 자동 주입하고 `uDepth`/`uInverseProjection` 등은 Application 이 Material `Properties` 에 써넣는다.

---

### 참조 코어 파일
`src/render/`: `render_stage.h` · `camera_stage.{h,cpp}` · `scene_renderer.{h,cpp}` · `screen_quad_stage.{h,cpp}` · `mesh_pass_processor.{h,cpp}` · `light_uniform_dispatcher.{h,cpp}` · `property_block_setter.{h,cpp}` · `pipeline_state_setter.{h,cpp}` · `device_context.{h,cpp}` · `render_pipeline.{h,cpp}` · `pass_component.h` · `render_target.{h,cpp}`
`src/buffer/framebuffer.h` · `src/material/pass.h`
`apps/_MyApp_/src/`: `VFX/{VFXSystem,ParticleStage}.{h,cpp}` · `UI/{UiBootstrap.cpp,ImGuiLayerStack.h,IImGuiLayer.h}` · `main.cpp` -->


----


# 렌더 파이프라인 보고서 — 쉬운 설명판

> 대상 코드: `src/render/` (엔진 코어) + `apps/_MyApp_/` (실행/통합).
> 읽는 순서: ① 전제(어디에 그리나) → ② 무엇을 그리나(Mesh/Effekseer/ImGui) → ③ 데이터는 언제 모으나 → ④ MultiPass 2종 → ⑤ 외부 모듈을 끼우려 무엇을 고쳤나.

---

## 1. 전제 — 모든 그리기는 "카메라 앞에 붙은 텍스처" 위에서 일어난다

이 엔진의 단 하나의 대전제는 이것이다.

> **카메라마다 자기 앞에 "그림판(텍스처)"이 한 장 붙어 있고, 그 카메라가 그리는 모든 것은 화면이 아니라 그 텍스처에 먼저 그려진다.**

이 "그림판"이 곧 **렌더 타겟(RenderTarget = FBO)** 이다. 코드로는 Camera 가 자기 RT 를 들고 있다 (`camera.h:145/150`, Unity `Camera.targetTexture` 와 같은 발상).

- **월드 카메라**(원근) 앞 그림판 = `sceneFB` (오프스크린 텍스처). 3D 장면이 여기 그려진다.
- **스크린 카메라**(직교) 앞 그림판 = PostFX FBO 들. 후처리 결과가 여기 그려진다.
- **맨 마지막 한 번만** 진짜 화면(백버퍼, `DefaultRenderTarget` = FBO 0)에 옮겨 출력한다.

특히 후처리는 말 그대로 **"카메라 앞에 화면 크기 사각형(Rect=풀스크린 quad) 한 장을 세우고, 그 위에 직전 결과 텍스처를 붙여 다시 찍는 것"** 이다. "텍스처를 붙인 사각형을 카메라로 찍는다" — 이 한 문장이 후처리의 전부다.

> 그림판 종류: `Framebuffer::Create(w,h)`(보통)와 `CreateWithDepthTexture(w,h)`(깊이까지 텍스처로 보관, fog 가 깊이를 읽어야 해서 필요 — `framebuffer.h:64,74`). `sceneFB` 는 후자로 만든다(`main.cpp:110`).

---

## 2. 무엇을 그리나 — Mesh · Effekseer · ImGui (그리고 각자의 문제)

화면에 올려야 할 대상은 세 종류다. **Mesh 는 엔진이 처음부터 자기 규칙대로 그리도록 만든 "네이티브"** 이고, **Effekseer/ImGui 는 외부 모듈이라 자기 멋대로 GL 을 호출**한다 — 여기서 문제가 갈린다.

| 대상 | 누가 그리나 | 우리 규칙을 따르나? | 핵심 문제 |
|---|---|---|---|
| **Mesh** | 엔진 코어가 직접 | ✅ RenderTarget·Material·DeviceContext 규칙대로 | 없음 (설계의 기준) |
| **Effekseer** (파티클) | 외부 모듈의 자체 Renderer | ❌ 자기 GL 호출 | ① 우리 그림판(FBO)을 모름 ② 우리 카메라 행렬을 모름 ③ **우리 VAO 의 EBO 를 오염시킴** |
| **ImGui** (UI) | 외부 모듈의 자체 백엔드 | ❌ 자기 GL 호출 | ① **자기가 어느 FBO 에 그릴지 명시 안 함**(지금 bind 된 곳에 그림) ② v1.53 결합형 백엔드라 콜백 방식 |

정리하면 — **Mesh 는 "엔진 안에서" 그려지고, Effekseer/ImGui 는 "엔진 밖에서 끼어든다".** 그래서 이 둘을 어떻게 우리 그림판 위에 얹느냐가 §5의 과제가 된다. 먼저 Mesh 가 그려지는 정상 경로(§3, §4)를 보고, 그 흐름에 외부 모듈을 끼우는 순서로 간다.

---

## 3. 셰이더에 넘길 데이터(Light·Material)는 언제 모으나

Mesh 를 그리려면 셰이더에 **카메라 행렬 · 조명 · 재질**을 넘겨야 한다. 이걸 **언제** 모아 **언제** 보내는지가 흐름의 핵심이다. 전부 한 카메라의 `SceneRenderer::RenderWithCamera()`(`scene_renderer.cpp:71`) 안에서, **그리기 직전에** 일어난다.

```
RenderWithCamera(cam):
  (1) 그림판 준비   : cam.GetTargetRenderTarget() 를 bind (+필요시 clear)
  (2) 조명 수집     : Director 의 씬에서 "켜진" DirLight/PointLight[]/SpotLight[] 만 모음
  (3) 조명 송신     : LightUniformDispatcher 가 "모든 셰이더"에 조명+카메라위치를 한 번에 전송  ← 패스당 1회
  (4) 대상 수집     : 액터 트리를 DFS 로 훑어 그릴거리(DrawCommand)를 큐에 담음
  (5) 정렬          : 불투명→투명, 가까운→먼 순으로 정렬
  (6) 그리기        : 큐를 돌며 셰이더/재질을 바꿔가며 실제 draw
```

**타이밍 요약 — 데이터마다 보내는 시점이 다르다:**

| 데이터 | 모으는 곳 | 보내는 시점 | 담당 |
|---|---|---|---|
| 조명(Light) + 카메라 위치 | 씬 컨텍스트에서 "켜진 것만" | **패스 시작 시 1회**, 모든 셰이더에 일괄 | `LightUniformDispatcher::Dispatch` (`:124`) |
| 카메라 행렬 `uView`/`uProj` | 카메라 | **셰이더(program)가 바뀔 때마다** | `MeshPassProcessor::Process` (`:152`) |
| 재질(Material) 값·텍스처 | 액터의 Material | **재질이 바뀔 때마다** | `PropertyBlockSetter::Set` (`:163`) |
| 모델 행렬 `uModel` | DrawCommand | **메시 하나하나마다** | `Uniforms::SetMat4` (`:174`) |

즉 **자주 안 바뀌는 건 드물게(조명=패스당 1회), 자주 바뀌는 건 그때그때(모델=메시마다)** 보낸다. 같은 셰이더/재질을 연속으로 쓰면 재전송을 건너뛰어 낭비를 줄인다(`program != lastProg` / `material != lastMat` 비교).

> 한 메시를 그리는 실제 순서: `셰이더 bind → (uView/uProj) → 재질 uniform·텍스처 → GL 상태(깊이/블렌드/컬링) → uModel → VAO bind → draw`. 조명 송신만 이 루프 *바깥*(패스 앞)에서 미리 끝낸다. (재질 값은 `PropertyBlockSetter` 가 타입별로 `glUniform*` 매핑 — bool 은 int 로 보내는 등.)

---

## 4. MultiPass — "일반 패스"와 "후처리 패스" 두 종류

한 프레임은 **여러 패스(pass)를 순서대로** 거친다. 코드로는 Application 이 든 **패스 리스트**(`mStages` 벡터, `main.cpp`)를 차례로 실행할 뿐이다. 패스는 딱 두 종류로 나뉜다.

```
mStages = [ ① worldCam ,  ② (Effekseer) ,  ③ screenCam(후처리) ,  ④ 최종 화면출력 ]
            └─ 일반 패스 ─┘                  └──── 후처리 패스 ────┘
```

### (A) 일반 패스 (Geometry) — 3D Mesh 를 sceneFB 에 그림
월드 카메라가 §3 의 흐름대로 액터 트리의 Mesh 들을 자기 그림판(`sceneFB`)에 그린다. 결과는 화면이 아니라 **텍스처 한 장**. 그런데 이 "한 번의 일반 패스" 안에서 Mesh 들은 그냥 막 그려지는 게 아니라 **종류와 순서로 잘게 나뉘어** 그려진다 (아래 A-심화).

### (A-심화) 일반 패스 안을 더 잘게 — "불투명·투명·스텐실"을 객체로 굳히고 우선순위로 줄 세우기

"이 재질을 어떻게 그릴까(불투명? 반투명? 외곽선?)"를 **if 분기가 아니라 데이터(객체) 두 개**로 굳혀 놨다 (`src/material/pass.h`).

**① `Pass::Kind` — "렌더링 의도"를 enum 으로** (`pass.h:54`). 재질마다 자기 종류 `Kind` 하나를 든다. 이 enum 의 **정수값이 곧 그리는 우선순위**다(Unity Render Queue 차용 — 작을수록 먼저):

| Kind | 정수(우선순위) | 의미 | 핵심 GL 상태 |
|---|---|---|---|
| StencilMaskWrite | 1999 | 외곽선 1단계: 도장 찍기 | 스텐실에 ref=1 기록 |
| **Opaque** | 2000 | 불투명(기본) | depth 쓰기 on, blend off, cull BACK |
| AlphaTest | 2450 | 스프라이트(알파 구멍) | depth on, **cull off**(flipX winding) |
| Skybox | 2500 | 하늘 | depth 쓰기 off, **앞면 cull**(큐브 안쪽) |
| **Transparent** | 3000 | 반투명 | depth 쓰기 off, **blend on**, 양면 |
| OutlineVisible/XRay | 4000/4001 | 외곽선 2단계: 그리기 | 스텐실 NOTEQUAL ref=1 |

**② `PipelineState` — Kind 가 푸는 "GL 고정상태 묶음"** (`pass.h:86`). `Kind` 를 `DefaultPipelineStateOf(Kind)`(`pass.h:171`)에 넣으면 **Depth / Cull / Blend / Stencil 4영역 + 정렬키(QueueLayer)** 가 한꺼번에 튀어나온다(DX12 `PSO` / Vulkan state 정통). 즉 "불투명/투명/스텐실"의 차이가 **분기문이 아니라 굳은 데이터**다. 예:
- **불투명**(Opaque): `depth test on · depth write on · blend off · cull BACK`
- **반투명**(Transparent): `depth write off`(투명끼리 안 가리게) · `blend on` · `cull off`(양면)
- **스텐실**(Outline): `StencilEnable · StencilFunc=NOTEQUAL · ref=1 · WriteMask=0x00`(읽기 전용) 등

재질은 `SetPass(Kind)` **한 번**으로 이 전부를 정한다(진실의 원천 1곳). 그리기 직전 `MeshPassProcessor` 가 이 묶음을 `PipelineStateSetter` 로 풀어 `glEnable / glDepthMask / glBlendFunc / glStencil*` 호출로 적용한다. (스텐실 외곽선은 *같은 메시를 2번* 그린다 — 1단계 `StencilMaskWrite`로 도장 찍고, 2단계 `OutlineVisible`로 살짝 키운 메시를 도장 안 찍힌 바깥에만 그려 윤곽선.)

**세부 우선순위 — 같은 일반 패스라도 정렬을 3중으로 나눈다** (`SortMultiStage`, `mesh_pass_processor.cpp:48`, `std::stable_sort`):

```
1차: QueueLayer(= Kind 정수)   ← 큰 분류: 불투명2000 → 하늘2500 → 투명3000 → 외곽선4000
2차: 임계값 2500 으로 분기
    ├ 불투명(<2500): program 묶고 → material 묶고 → 가까운 것 먼저(front-to-back)
    └ 투명(≥2500):   깊이만 보고 → 먼 것 먼저(back-to-front)
```

- **불투명은 "같은 셰이더·재질끼리 모아서"** 그린다 → GL 상태 전환 최소화(성능). 그 안에서 **가까운 것 먼저** → 깊이 버퍼가 가까운 면을 먼저 채워 뒤에 가릴 fragment 를 GPU 가 자동으로 건너뜀(z-cull, 성능).
- **반투명은 정반대 "먼 것 먼저"** → 알파 블렌딩은 뒤→앞 순서로 칠해야 색이 맞다(거꾸로면 틀린 색).
- (부호 함정) OpenGL 카메라는 −Z 를 보므로 *가까움 = z 가 덜 음수(큰 값)* → front-to-back 은 `a.depth > b.depth`.

그리고 **같은 종류 안에서의 미세 순서**는 `MeshRenderer::QueueOffset` 으로 조정한다(예: 외곽선을 본체 바로 뒤에). 정리하면 두 축이 직교한다 — **`Kind`(재질이 결정, 큰 분류 + GL 상태) ↔ `QueueOffset`(렌더러가 결정, 인스턴스 미세조정)**.

> `std::sort` 가 아니라 `stable_sort` 인 이유: 우선순위가 같은 cmd 들의 순서를 **항상 동일**하게 유지 → ① z-fighting 깜빡임 차단 ② Actor 트리(DFS) 순서 보존 ③ 골든 이미지 회귀 테스트 결정성.

### (B) 후처리 패스 (PostProcess) — 풀스크린 사각형을 FBO→FBO 로 흘림
스크린 직교 카메라가 **PassComponent 체인**을 처리한다. 이게 §1 에서 말한 "카메라 앞 Rect 에 텍스처 붙여 다시 찍기"다. 후처리는 **패스마다 그림판(FBO)을 하나씩 두고 일렬로 잇는 선형 체인**이다 (핑퐁 아님).

```
sceneFB ─[fog]→ FB1 ─[bloom]→ FB2 ─[vignette]→ FB3 ─ ... ─→ (최종)
   각 단계: 이전 결과 텍스처를 uScene 으로 받아 → 사각형에 칠해 → 다음 FBO 에 출력
```

이 "이전 FBO ↔ 다음 FBO" 연결을 만드는 게 `PassComponent` 다 — **렌더 로직 없는 순수 데이터**로, `InputFB`(읽을 텍스처)와 `OutputFB`(쓸 텍스처) 포인터만 들고 있다 (`pass_component.h:48`). 한 패스의 `OutputFB` 가 다음 패스의 `InputFB` 와 같은 포인터라서 자동으로 줄줄이 엮인다. `uScene`(직전 결과)은 코어가 자동으로 꽂아주고, fog 의 `uDepth`/`uInverseProjection` 같은 특수 입력만 Application 이 재질에 직접 넣는다(`main.cpp:347-349,485`).

### (C) 최종 출력 — 화면(백버퍼)으로
마지막 `ScreenQuadStage` 가 후처리 최종 결과 텍스처를 **진짜 화면(FBO 0)** 에 한 번 붙여 출력한다(`screen_quad_stage.cpp:47`).

> **두 패스의 정체는 같다.** 둘 다 "그림판(FBO)에 무언가를 그린다"이고, 차이는 *무엇을* 그리느냐뿐 — 일반 패스는 **3D Mesh 여러 개**, 후처리 패스는 **화면 사각형 1장**. 실제로 코어의 `DrawCommand` 가 가진 두 종류 `Kind::WorldMesh` / `Kind::ScreenQuad` 가 정확히 이 둘이고, 같은 `MeshPassProcessor::Process` 루프가 둘 다 처리한다.

---

## 5. MultiPass 에 Effekseer·ImGui 를 끼우려 무엇을 고쳤나

§2 의 문제 — 외부 모듈은 우리 그림판/카메라를 모르고 GL 을 오염시킨다 — 를 패스 리스트에 어떻게 녹였는지가 결론이다. **둘은 끼우는 방식이 다르다.**

### 5.1 Effekseer = "패스 하나로 위장시켜 리스트 중간에 삽입"

Effekseer 를 우리 `mStages` 의 **②번 패스**로 끼우기 위해, `IRenderStage` 를 상속한 어댑터 **`ParticleStage`** 를 새로 만들었다 (`apps/_MyApp_/src/VFX/ParticleStage.cpp`). 이 어댑터가 §2 의 세 문제를 직접 해결한다.

1. **그림판을 모름** → 어댑터가 월드 카메라의 그림판(`sceneFB`)을 **대신 bind 해주고** Effekseer 에 그리라고 시킨다. 그래서 파티클이 화면이 아니라 3D 장면 텍스처 **위에**(지우지 않고, NoClear) 얹힌다.
   ```cpp
   auto* rt = mWorldCam->GetTargetRenderTarget();   // = sceneFB
   DeviceContext::Get().BindTarget(*rt);            // 우리가 대신 그림판을 깔아줌
   mVFX->Draw(view, proj);                          // 그 위에 Effekseer 가 그림
   ```
2. **카메라 행렬을 모름** → 우리 `mat4` 의 숫자 16개를 Effekseer 의 `Matrix44` 로 `memcpy` 해서 넘긴다 (`SetCameraMatrix`/`SetProjectionMatrix`, `VFXSystem.cpp:84-98`). 좌표계(RH)가 같아 변환 없이 그대로 통한다.
3. **VAO 의 EBO 오염** → Effekseer 가 그리고 나면 우리가 쓰던 사각형 VAO 의 인덱스 버퍼(EBO) 연결이 깨진다. 그래서 바로 다음 패스(`ScreenQuadStage`)가 매 프레임 자기 사각형의 `ebo->Bind()` 를 **다시 걸어** 복구한다 (`screen_quad_stage.cpp:64-71`). 안 하면 `glDrawElements` 가 깨진다.

부트스트랩(`VFXSystem::Init`)은 **새 GL 컨텍스트를 만들지 않고** 기존 컨텍스트를 공유하도록 `OpenGL3` 모드로 생성하는 게 포인트 (`VFXSystem.cpp:38`).

### 5.2 ImGui = "패스로 안 만들고, 모든 패스가 끝난 뒤 맨 위에 얹기"

ImGui 는 항상 **최상단 UI** 라서 패스 리스트에 넣지 않았다. 대신 `mStages` 순회가 **다 끝난 뒤** `render()` 본문에서 직접 호출한다 (`main.cpp:361-362`).

- **"어디에 그릴지 명시 안 함" 문제를 역이용**: ImGui v1.53 백엔드는 `glBindFramebuffer` 를 호출하지 않아 **"지금 bind 된 그림판"에 그린다.** 그런데 직전 ④번 패스(`ScreenQuadStage`)가 이미 **백버퍼(화면)** 를 bind 해두고 끝냈으므로, ImGui 는 자연히 그 화면 위에 얹힌다. 별도 코드 없이 순서만으로 "맨 위 오버레이"가 된다.
- **결합형 백엔드 대응**: v1.53 은 `ImGui_ImplGlfwGL3` 단일 결합 백엔드라, `ImGui::Render()` 한 줄이 내부 콜백으로 실제 GL 드로우까지 자동 수행한다(현대식 분리형 `RenderDrawData` 호출 불필요). 입력은 sb7 가 GLFW 콜백을 소유하므로 `WantCaptureMouse/Keyboard` 로 게이트해 수동 포워딩한다.
- UI 창들은 `ImGuiLayerStack`(레이어 리스트)이 순서대로 빌드 (PauseButton/PostFXDebug/StateOverlay/VfxSpawn).

### 통합 방식 한눈에

| 모듈 | 끼우는 방식 | 그리는 그림판 | 모듈 측 수정 |
|---|---|---|---|
| **Effekseer** | 패스로 위장(`ParticleStage`) → 리스트 **중간 삽입** | 월드 카메라의 `sceneFB`(NoClear) | 그림판 대신 bind + 카메라 memcpy + EBO 복구 |
| **ImGui** | 패스 아님 → 리스트 **끝난 뒤** 직접 호출 | 직전 패스가 남긴 백버퍼(화면) | 콜백식 `Render()` + 입력 수동 포워딩 |

---

## 6. 한 장 요약 — 한 프레임의 전체 흐름

```
[전제]  카메라마다 앞에 그림판(FBO=텍스처)이 붙어 있다.

① 일반 패스   : 월드카메라 → 3D Mesh 들을 sceneFB(텍스처)에 그림
                 └ 그리기 직전: 조명은 패스당 1회, 재질·행렬은 바뀔 때마다 셰이더로 전송
② Effekseer   : ParticleStage 가 sceneFB 를 대신 bind → 그 위에 파티클 합성(NoClear)
③ 후처리 패스 : 스크린카메라 → "사각형+텍스처"를 FBO→FBO 로 흘림 (fog→bloom→… 선형 체인)
④ 최종 출력   : 후처리 결과를 백버퍼(진짜 화면)로 옮김 (+ EBO 복구)
⑤ ImGui       : 패스가 다 끝난 뒤, 백버퍼 위에 UI 를 얹음
```

**결론**: 모든 렌더링은 "카메라 앞 텍스처 위에 무언가를 그리는" 동일한 동작의 반복이다. 일반 패스는 그 텍스처에 3D Mesh 를, 후처리 패스는 사각형 1장을 그린다. Mesh 는 엔진 네이티브라 그냥 흐름에 태우면 되고, **Effekseer 는 "패스로 위장시켜 흐름 중간에" / ImGui 는 "흐름이 끝난 뒤 맨 위에" 끼워** 같은 그림판 체계 안으로 흡수했다.

---

### 참조 파일
`src/render/`: `scene_renderer.{h,cpp}`(패스 코어·수집·송신) · `mesh_pass_processor.{h,cpp}`(그리기·전환결정) · `property_block_setter.cpp`/`pipeline_state_setter.cpp`/`light_uniform_dispatcher.cpp`(데이터 송신) · `screen_quad_stage.{h,cpp}`(최종 출력·EBO 복구) · `pass_component.h`(후처리 체인) · `render_target.{h,cpp}`/`camera_stage.{h,cpp}`/`device_context.{h,cpp}`
`src/buffer/framebuffer.h`(그림판) · `src/material/pass.h`(GL 상태 기준)
`apps/_MyApp_/src/`: `VFX/{VFXSystem,ParticleStage}.{h,cpp}` · `UI/{UiBootstrap.cpp,ImGuiLayerStack.h}` · `main.cpp`(패스 리스트·프레임 루프)
