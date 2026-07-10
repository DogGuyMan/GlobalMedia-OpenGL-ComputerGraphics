# SP4 — Multi-pass Pipeline + Post-processing Design

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

**날짜**: 2026-05-21
**전제**: SP1 (Shader/Program) + SP2 (RenderContext + RenderTarget) + SP3 (Actor+Component+RenderSystem) + SP3.5 (ResourceRegistry 확장 + CameraComponent) 완료.

## 0. Snapshot

```
SP1 ✅ Shader/Program 리소스 통합
SP2 ✅ RenderContext + Pattern Y + RenderTarget 인터페이스
SP3 ✅ Actor + Component + RenderSystem + RenderQueue + MaterialApplier + ModelSpawner
SP3.5 ✅ ResourceRegistry 5종 자원 + Get() 싱글톤 + Camera 컴포넌트
SP4    ← 본 spec — 멀티패스 + 포스트프로세싱 (FrameBufferTarget 통합)
```

기존 자산:
- `src/buffer/framebuffer.{h,cpp}` — `SJH::Framebuffer` (FBO + RBO 깊이/스텐실 + 외부 텍스처 슬롯). SP2 의 `RenderTarget` 인터페이스 *미상속* 상태.
- `src/buffer/render_target.h` — `RenderTarget` 추상 + `DefaultRenderTarget` (FBO 0). SP2 spec line 906 의 *FrameBufferTarget 자리* 비어 있음.
- `resources/shader/postprocess/*.fs` — 5종 (`blurring`, `gamma`, `invert`, `sharpening`, `sobel`). `postprocess.vs` 는 미작성.
- `resources/shader/{simple,lighting,texture}.{vs,fs}` — 기본 셰이더 3종.

## 1. 목표

1. **F1**: `SJH::Framebuffer` 를 `RenderTarget` 인터페이스로 통합 — SP2 seam 실현.
2. **F2**: `Scene::Camera::SetTargetFramebuffer(Framebuffer*)` — Unity `Camera.targetTexture` 정통.
3. **F3**: `RenderSystem` 이 활성 Camera 단수 → 모든 Camera 직렬 (Unity `Camera.depth` ordering).
4. **F4**: `Mesh::CreateScreenQuad()` — NDC full-screen quad factory.
5. **F5**: PostFX 셰이더 통합 컨벤션 — `uniform sampler2D uScene;`.
6. **F6**: `apps/postfx_demo` — 2 Camera (Scene → SceneFB, PostFX → backbuffer + invert) 시각 검증.

## 2. 핵심 결정

| ID | 결정 | 근거 |
|---|---|---|
| **D-1** | `Framebuffer : public RenderTarget` (public inheritance) | SP2 spec line 906 의 seam 실현. polymorphism — `RenderContext::BeginFrame(RenderTarget&)` 가 동일 코드로 default/FBO 둘 다 처리. |
| **D-2** | `Framebuffer::Create(int w, int h)` 새 factory — 내부 texture + 내부 RBO 자동 생성. 기존 `Create(TexturePtr)` 도 유지 (외부 텍스처 공유 시나리오). | 2 factory family — 내부 자족 + 외부 공유 모두 지원 |
| **D-3** | `Framebuffer::Bind()` = `glBindFramebuffer(mFBO) + glViewport(0,0,w,h)`. `GetSize()` = color attachment size | `RenderTarget` 인터페이스 contract 일치 |
| **D-4** | `Camera::mTargetFB` (`Framebuffer*` 비소유 관찰자) + Set/Get. nullptr = default backbuffer | Unity `Camera.targetTexture` 1:1 매핑 |
| **D-5** | Multi-pass = *씬 트리의 모든 Camera 컴포넌트 수집 + depth 정렬 + 직렬 실행*. `Director::SetActiveCamera` 는 *기본 카메라 hint* 로 유지 (단일 카메라 시나리오 호환) | Unity `Camera.depth` 정통. SP3.5 호환 |
| **D-6** | `Camera::mDepth` (`int`) — 정렬 키. 작은 값 먼저 (Scene → PostFX 순서) | Unity 정통 |
| **D-7** | `Mesh::CreateScreenQuad()` — 위치 `(-1,-1)~(1,1)` 4 vertex + UV `(0,0)~(1,1)` + 2 triangle indices. position 만, no normal | NDC 직접 사용 — view/proj 무관 |
| **D-8** | PostFX vs (`postprocess.vs`) 신설 — `gl_Position = vec4(aPos, 0, 1)`, `vUV = aUV`. uModel/uView/uProj 미사용 | 5개 fs 공유 |
| **D-9** | PostFX fs 의 sampler uniform 명 = `uScene` (Unity `_MainTex` 식) | 컨벤션 단일화. 기존 fs 5종 필요시 수정 |
| **D-10** | PostFX 는 *Material 재사용* — sampler unit + program 만 다름. 별도 Pass/Effect 클래스 안 만듦 | ddd Library-First, 학습 프로젝트 단순성 |
| **D-11** | Framebuffer owner = App/Chapter (UPtr 멤버). ResourceRegistry 안 건드림 | Cocos/Unity/Unreal 정통. SP3.5 컨벤션의 *예외* 가 아니라 *자원 카테고리가 다름* (사용자 컨텐츠 vs 캐시 자원) |
| **D-12** | Window resize 대응 *학습 프로젝트 관용으로 미지원* — startup 시점 size 고정 | 단순성. SP4.5 후보 |
| **D-13** | RenderSystem 의 `mQueue` 재사용 (Camera 마다 Clear → Submit → Sort → Flush). 추가 인스턴스 안 만듦 | Cocos `Renderer` 정통 — 큐는 패스 간 공유 |
| **D-14** | `Director::SetActiveCamera` 는 *주 카메라 hint* 로 의미 변경 — 단일 카메라 시나리오 (SP3.5 ecs_demo) 호환 위해 유지. Multi-camera 시나리오는 *씬 트리 traversal* 우선 | 후방 호환성 |
| **D-15** | `Camera::SetCullingMask(uint32_t)` + `Actor::SetLayer(uint32_t)` — Unity `Camera.cullingMask` 정통. RenderSystem 이 `cam.mask & actor.layer` 비트 AND 로 필터링 | Multi-pass 시 카메라별 가시성 분리 필수 (self-sampling UB 회피). Cocos `Node::setCameraMask` 와 동일 비트마스크 원리. 32 layer 지원. 기본 Actor.layer = 1 (비트 0), Camera.mask = ~0u (전부) → SP3.5 호환 보장 |

## 3. 아키텍처 변경

### 3.1 모듈 의존 그래프

기존:
```
RenderTarget (SP2) ← DefaultRenderTarget (SP2)
Framebuffer (SP3 시점 standalone — RenderTarget 미상속)
```

SP4 적용 후:
```
RenderTarget (SP2)
  ├─ DefaultRenderTarget (SP2)
  └─ Framebuffer (SP4 통합 — public 상속)
```

### 3.2 모듈 위치 결정

| 항목 | 위치 | 근거 |
|---|---|---|
| `Framebuffer` 본체 | `src/buffer/framebuffer.{h,cpp}` 유지 (기존 위치) | 기존 코드 보존, GL FBO 자원 — buffer 모듈 일관 |
| `RenderTarget` 인터페이스 | `src/buffer/render_target.h` (SP2 위치 유지) | 인터페이스는 render 가 owner |
| 의존 방향 | `SJH::buffer` → `SJH::render` (Framebuffer 가 RenderTarget 인터페이스 사용) | SP3 의 material → render 순환을 피한 것처럼 단방향 |

**대안 분석**:
- Option A: Framebuffer 를 `src/render/` 로 이동 — 인터페이스와 같은 모듈
- Option B: `RenderTarget` 인터페이스를 `src/buffer/` 로 이동 — 본체와 같은 모듈
- Option C: 현 위치 유지 + buffer → render 의존 추가 (**채택**)

C 가 옳음:
- Framebuffer 는 *GL buffer 자원* — 버퍼/이미지 모듈 일관
- RenderTarget 인터페이스는 *렌더 흐름의 일부* — render 가 owner
- buffer 가 render 의 인터페이스만 사용 → 순환 의존 없음 (buffer는 render 의 cpp 미사용)

## 4. Task 분할

### 의존 그래프

```
T1 (Framebuffer ← RenderTarget) ─┐
                                  ├→ T6 (postfx_demo)
T2 (Camera::SetTargetFramebuffer) ┤
                                  │
T3 (RenderSystem multi-Camera) ───┤
                                  │
T4 (Mesh::CreateScreenQuad) ──────┤
                                  │
T5 (postprocess.vs + uScene fs) ──┘
                                  ↓
                              T7 (smoke + commit)
```

### T1 — `Framebuffer : public RenderTarget`

**파일**: `<src>/buffer/framebuffer.h`, `<src>/buffer/framebuffer.cpp`, `src/buffer/CMakeLists.txt`

**변경**:
1. `class Framebuffer : public RenderTarget` (기존 `Framebuffer` 클래스에 public 상속 추가)
2. `void Bind() override` — `glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer) + glViewport(0, 0, w, h)`. 기존 `Bind() const` 는 `Bind()` (non-const) 로 변경 — virtual override 일관성.
3. `Size GetSize() const override` — color attachment 의 width/height. Texture 가 size 보관하므로 `mColorAttachment->GetWidth()` 등 활용.
4. **새 factory**: `static FramebufferUPtr Create(int w, int h)` — 내부 텍스처 자동 생성. RGBA8 + depth/stencil RBO.
5. **CMakeLists.txt**: `target_link_libraries(sjhopengl_buffer ... PUBLIC SJH::render)` 추가 (RenderTarget 인터페이스 노출).

**의존 검증**:
- `RenderTarget` 인터페이스 — `src/buffer/render_target.h:12-18` ✓
- `Texture::GetWidth/Height` 또는 비슷한 getter — 없으면 추가 (Texture 가 width/height 보관 여부 확인)

### T2 — `Scene::Camera::SetTargetFramebuffer`

**파일**: `src/scene/camera.h`

**변경**:
1. `class Framebuffer;` forward decl
2. `void SetTargetFramebuffer(Framebuffer* fb) { mTargetFB = fb; }` inline
3. `Framebuffer* GetTargetFramebuffer() const { return mTargetFB; }` inline
4. `Framebuffer* mTargetFB = nullptr;` private 멤버
5. `void SetDepth(int d) { mDepth = d; }` / `int GetDepth() const { return mDepth; }` — Unity `Camera.depth` 정통
6. `int mDepth = 0;` 멤버

CMakeLists.txt 변경 없음 (forward decl 만).

### T3 — `RenderSystem` 다중 Camera 지원

**파일**: `<src>/render/render_system.h`, `<src>/render/render_system.cpp`

**변경**:
1. `Render()` 인자 없는 overload 본문 변경:
   - 기존: `auto* cam = Director::Get().GetActiveCamera(); ... Render(view, proj);`
   - 신규: Actor 트리에서 *모든 Camera* 수집 (`CollectCameras`) + depth 정렬 + 각각 `RenderWithCamera(cam)` 호출
2. private 메서드 추가:
   - `void CollectCameras(const Scene::Actor& actor, std::vector<Camera*>& out);`
   - `void RenderWithCamera(Camera& cam);` — target FB 바인딩 (있으면 FBO, 없으면 DefaultRenderTarget) + Actor 트리에서 MeshRenderer 수집 + Queue Flush
3. 기존 `Render(view, proj)` 와 `CollectFromActor` 는 *테스트/하위 호환용* 유지

**Camera 0 케이스**: spdlog::warn + early return (SP3.5 동작 유지)

### T4 — `Mesh::CreateScreenQuad`

**파일**: `src/object/mesh.h`, `src/object/mesh.cpp`

**변경**:
1. `static MeshUPtr CreateScreenQuad()` 신설 — NDC 위치 + UV.
2. Vertex 레이아웃: position (vec3) + uv (vec2) — `simple` 셰이더와 동일 레이아웃.
3. Geometry: 4 vertices `(-1,-1,0)/(1,-1,0)/(1,1,0)/(-1,1,0)`, UV `(0,0)/(1,0)/(1,1)/(0,1)`, indices `[0,1,2, 0,2,3]`.

### T5 — `postprocess.vs` + fs 통합

**파일**: `resources/shader/postprocess/postprocess.vs` (신규)

```glsl
#version 410 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;

out vec2 vUV;

void main() {
    gl_Position = vec4(aPos, 1.0);   // NDC 직접
    vUV = aUV;
}
```

**기존 fs 5종 점검**:
- 사용자가 정한 컨벤션 `uniform sampler2D uScene;` 로 통일.
- 기존 fs 가 다른 이름 (예: `tex`, `screenTexture`) 쓰면 수정.

### T6 — `apps/postfx_demo` 챕터

**파일**: `<apps>/postfx_demo/main.cpp`, `apps/postfx_demo/CMakeLists.txt`, `apps/postfx_demo/resources/shaders/` (심볼릭 또는 복사)

**구조**:

```cpp
class postfx_demo : public sb7::application
{
    void startup() override {
        // 1) 자원 (ResourceRegistry 위탁)
        auto& reg = ResourceRegistry::Get();
        auto* simpleProg = reg.CreateProgram("simple", ...);          // Scene Box용
        auto* postfxProg = reg.CreateProgram("postfx_invert",
                                              "resources/shaders/postprocess.vs",
                                              "resources/shaders/postprocess/invert.fs");
        auto* boxMesh   = reg.RegisterMesh("box", Mesh::CreateBox());
        auto* quadMesh  = reg.RegisterMesh("screen_quad", Mesh::CreateScreenQuad());
        auto* boxMat    = reg.CreateMaterial("box");
        boxMat->SetProgram(simpleProg);
        auto* postfxMat = reg.CreateMaterial("postfx_invert");
        postfxMat->SetProgram(postfxProg);

        // 2) Framebuffer — App 보유 (Option C)
        mSceneFB = Framebuffer::Create(info.windowWidth, info.windowHeight);

        // 3) postfxMat 의 diffuse 슬롯에 SceneFB 의 color attachment 바인딩
        postfxMat->SetResolvedTextures(mSceneFB->GetColorAttachment().get(), 0);
        // 또는 별도 SetSceneTexture API (Material 확장 필요시)

        // 4) Scene 구성
        auto& dir = Director::Get();

        // Box Actor — SceneCamera 가 본다
        auto box = std::make_unique<Actor>("Box");
        box->AddComponent<MeshRenderer>(boxMesh, boxMat);
        dir.Root().AddChild(std::move(box));

        // Quad Actor — PostFXCamera 가 본다
        auto quad = std::make_unique<Actor>("ScreenQuad");
        quad->AddComponent<MeshRenderer>(quadMesh, postfxMat);
        dir.Root().AddChild(std::move(quad));

        // SceneCamera (depth=0, targetFB = SceneFB)
        auto sceneCam = std::make_unique<Actor>("SceneCamera");
        sceneCam->GetTransform().Translate = vmath::vec3(0, 0, 5);
        auto* sc = sceneCam->AddComponent<Camera>(45.0f, aspect, 0.1f, 100.0f);
        sc->SetDepth(0);
        sc->SetTargetFramebuffer(mSceneFB.get());
        dir.Root().AddChild(std::move(sceneCam));

        // PostFXCamera (depth=1, targetFB = nullptr = backbuffer)
        auto fxCam = std::make_unique<Actor>("PostFXCamera");
        auto* fc = fxCam->AddComponent<Camera>(45.0f, aspect, 0.1f, 100.0f);
        fc->SetDepth(1);
        // SetTargetFramebuffer 호출 안 함 → nullptr → backbuffer
        dir.Root().AddChild(std::move(fxCam));

        dir.Enter();
    }

    void render(double t) override {
        Director::Get().Update(...);
        mRenderSys.Render();  // SP3.5 의 인자 없는 overload — Multi-Camera 자동 직렬
    }

private:
    FramebufferUPtr   mSceneFB;     // App 보유 (Option C)
    RenderSystem      mRenderSys;
    double            mLastTime = 0.0;
};
```

**주의** — Quad Actor 는 *PostFXCamera 만* 봐야 하고, Box 는 *SceneCamera 만* 봐야 함. 현재 RenderSystem 은 모든 Camera 가 모든 MeshRenderer 를 그림 (depth 정렬만). 옵션:
- **D-15 (추가 결정)**: *layer/mask 시스템* 미도입. 학습 데모에서는 *Quad 가 NDC* 라서 Scene FBO 에 그려져도 깊이 0 이라 보이지 않거나 가장 앞에 보임. Sm tester needs verify.
- **간단한 해결**: Quad Actor 를 PostFXCamera 의 자식으로 부착 — SP4 의 *카메라 부착 Actor* 패턴. 그러나 RenderSystem 이 그걸 알아야.
- **권장**: T6 에서 시각 검증 후 결정. 일단 *전체 Actor 가 모든 Camera 에 보임* 으로 시작 (Quad 가 Box 를 덮는 결과).

### T7 — smoke + 시각 검증 + 커밋

`cd build_ninja/apps/postfx_demo && timeout 5 ./postfx_demo` — crash 없으면 PASS. 시각 검증 (오렌지 박스 → invert 결과) 은 인간 책임.

## 5. ddd Rules 14 검증

| # | 원칙 | 결과 |
|---|---|---|
| 1 SoC | Framebuffer = FBO 자원, Camera = view+target slot, RenderSystem = traversal+pass coordination, RenderQueue = sort/flush | ✅ |
| 2 POLA | `SetTargetFramebuffer(nullptr)` = backbuffer (Unity 정통). 다중 Camera 시 depth 정렬 (Unity Camera.depth). | ✅ |
| 3 CQS | GetTargetFramebuffer query / SetTargetFramebuffer command 분리. | ✅ |
| 4 Explicit Side Effects | Framebuffer::Bind() 가 명시 glBindFramebuffer + glViewport. RenderContext::BeginFrame 통과. | ✅ |
| 5 Functional Core | Camera GetView/Proj pure. RenderSystem 의 CollectCameras pure. RenderWithCamera 만 imperative. | ✅ |
| 6 Domain Naming | `Camera.targetTexture` → `SetTargetFramebuffer`. `Camera.depth` → `SetDepth`. Unity 1:1. | ✅ |
| 7 Library-First | SJH::Framebuffer 재사용. RenderTarget 인터페이스 재사용. PostFX = Material 재사용 (별도 Pass 클래스 안 만듦). | ✅ |
| 8 Early Return | Camera 0 시 warn+skip. target nullptr → default backbuffer fallback. | ✅ |
| 9 File Size | 새 파일: postprocess.vs, postfx_demo/{main.cpp, CMakeLists.txt}. 기존 framebuffer.h 50줄 → 70줄 (수용). | ✅ |
| 10 SSoT | Framebuffer owner = App 명시. Camera target = slot. RenderSystem = single render entry. | ✅ |
| 11 Cohesion | 각 클래스 단일 책임 유지. | ✅ |
| 12 Loose Coupling | Framebuffer ← RenderTarget polymorphism. RenderContext 는 RenderTarget 타입만 인식. | ✅ |
| 13 Tell Don't Ask | SetTargetFramebuffer Tell. GetView Ask — FCIS query 예외. | ✅ |
| 14 SRP | Camera 의 `targetFramebuffer 슬롯` 추가가 SRP 위반인가? — Unity 정통 패턴이고, Camera 가 "어디에 그릴지" 결정하는 것은 자연스러운 책임. ✅ |

## 6. Out of Scope (재확인)

| 항목 | 처리 |
|---|---|
| Window resize 대응 | SP4.5 후보 |
| Layer/mask 시스템 (특정 Camera 가 특정 Actor 만 그리기) | SP5 후보 — Unity Camera.cullingMask |
| 멀티 attachment FBO (G-buffer) | 미지원 — SP5 후보 |
| HDR / floating point color attachment | 미지원 |
| MSAA / RBO multisample | 미지원 |
| Async pass execution (compute shader) | 영구 미지원 (학습 프로젝트) |

## 7. 결정 로그 (요약)

| ID | 결정 | 옵션/근거 |
|---|---|---|
| D-1 | Framebuffer ← RenderTarget public inheritance | SP2 seam 실현 |
| D-2 | Framebuffer 2 factory (Create(w,h) + Create(TexturePtr)) | 내부 자족 + 외부 공유 모두 |
| D-3 | Bind() = glBindFramebuffer + glViewport | RenderTarget contract |
| D-4 | Camera::SetTargetFramebuffer slot | Unity Camera.targetTexture |
| D-5 | Multi-pass = 씬 트리의 모든 Camera 직렬 | Unity Camera.depth |
| D-6 | Camera::mDepth 정렬 키 | Unity 정통 |
| D-7 | Mesh::CreateScreenQuad NDC factory | view/proj 무관 |
| D-8 | postprocess.vs 신설 | 5 fs 공유 |
| D-9 | uniform sampler2D uScene 컨벤션 | Unity _MainTex 식 |
| D-10 | PostFX = Material 재사용 | Library-First |
| D-11 | Framebuffer owner = App | Cocos/Unity/Unreal 정통 |
| D-12 | Window resize 미지원 | 학습 프로젝트 관용 |
| D-13 | RenderSystem mQueue 재사용 (패스 간 Clear) | Cocos Renderer |
| D-14 | Director::SetActiveCamera = 주 카메라 hint (호환성) | SP3.5 ecs_demo 무변경 보장 |
| D-15 | Camera::cullingMask + Actor::layer (uint32_t 비트마스크) | Unity 정통. self-sampling UB 회피. SP4 multi-pass 필수 |

## 8. SP5+ 후속 작업 (Out of Scope)

- Camera.cullingMask (Unity 정통) — layer 비트마스크
- ImGui 통합 — postfx 효과 실시간 토글
- HDR 색상 + tone mapping
- Bloom / DOF / motion blur
- G-buffer + deferred rendering
- Shadow mapping (별도 큰 SP)

---

## 9. 진행 순서

| 단계 | 작업 | 소요 |
|---|---|---|
| 1 | T1 Framebuffer ← RenderTarget (구현 + buffer/CMakeLists.txt SJH::render PUBLIC 추가) | 1시간 |
| 2 | T2 Camera::SetTargetFramebuffer + Depth slot | 15분 |
| 3 | T3 RenderSystem 다중 Camera traversal | 1시간 |
| 4 | T4 Mesh::CreateScreenQuad | 30분 |
| 5 | T5 postprocess.vs 신설 + 기존 fs 의 sampler 이름 통일 | 30분 |
| 6 | T6 apps/postfx_demo 챕터 | 1시간 |
| 7 | T7 smoke + commit | 30분 |
| **합계** | | **5시간** |
