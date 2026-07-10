# PassComponent 2-Camera 아키텍처 전환 — 세션 변경 이력

> **작성일**: 2026-05-27  
> **커밋**: `57f5779 [refactor] : 2 camera`  
> **브랜치**: `game/module/well_drawexample`  
> **대상 독자**: 다음 세션의 Claude agent / 디버깅 담당자  
> **plan**: [`doc/superpowers/plans/2026-05-27-pass-component-impl.md`](../../doc/superpowers/plans/2026-05-27-pass-component-impl.md)

---

## 1. 동기 — 무엇을 왜 바꿨는가

### Before (이전 패턴)

`PostFXPass` struct + `SceneRenderer::SetPostFXChain/RunPostFXChain` 방식.

```
PostFXPass {
    Material* Material;
    Framebuffer* OutputFB;     // InputFB 는 멤버 없음 — 체인 순서가 암묵적 배선
    bool Enabled;
}

SceneRenderer::RunPostFXChain() 가 OutputFB 를 체인 순서대로 이전 단계 입력으로 넘겨 줌.
ScreenQuadStage::GetActiveFXOutput() 으로 최종 FB 를 받아 backbuffer 에 합성.
```

**문제**: SceneRenderer 내부에 PostFX 체인 로직이 침투. PostFX 추가/제거 시 SceneRenderer API 변경 필요. `PostFXDebugLayer` 가 `SceneRenderer&` 에 직접 의존.

### After (신규 패턴) — 4 엔진 정통

```
PassComponent : Component   (Actor 에 부착, Layer::Screen)
  + InputFB* const          (읽기 소스)
  + OutputFB* const         (쓰기 대상)
  + Material* mMaterial
  + bool Enabled
  + int QueueOffset = 9000  (월드 지오메트리 이후)

Screen Camera (Ortho, CullingMask = Layer::Screen)
  → PassComponent 를 자동 수집, QueueOffset 순으로 실행
```

SceneRenderer 는 PassComponent 를 **일반 DrawCommand 와 동일 파이프라인**으로 처리. 별도 체인 로직 없음.

---

## 2. 렌더 데이터 흐름 (After)

```
startup() FB 배선:
  mSceneFB ──────────────────────────────────────────────────────┐
  PassActor_blurring:   InputFB=mSceneFB    OutputFB=mPostFXFBs[0]
  PassActor_gamma:      InputFB=mPostFXFBs[0]  OutputFB=mPostFXFBs[1]
  PassActor_invert:     InputFB=mPostFXFBs[1]  OutputFB=mPostFXFBs[2]
  PassActor_sharpening: InputFB=mPostFXFBs[2]  OutputFB=mPostFXFBs[3]
  PassActor_sobel:      InputFB=mPostFXFBs[3]  OutputFB=mPostFXFBs[4]

per-frame 렌더 순서:
  [1] SceneRenderer::Render(*mDefaultTarget)
      → WorldCamera (CullingMask = Default|Player|Enemy|DebugDraw)
          BeginFrame(mSceneFB)
          → CollectFromActor → DrawCommand::WorldMesh 제출
          → MeshPassProcessor::Process() → 월드 지오메트리 mSceneFB 에 기록
      → ScreenCamera (Ortho, CullingMask = UI|Screen)
          BeginFrame(mSceneFB)   ← 같은 sceneFB 공유 (HUD가 씬에 직접 합성)
          → CollectFromActor → PassComponent → DrawCommand::ScreenQuad 제출
          → MeshPassProcessor::Process() 에서 ScreenQuad 분기:
              [blurring]   BeginFrame(mPostFXFBs[0])  uScene←mSceneFB.color    draw
              [gamma]      BeginFrame(mPostFXFBs[1])  uScene←mPostFXFBs[0].color draw
              [invert]     BeginFrame(mPostFXFBs[2])  uScene←mPostFXFBs[1].color draw
              [sharpening] BeginFrame(mPostFXFBs[3])  uScene←mPostFXFBs[2].color draw
              [sobel]      BeginFrame(mPostFXFBs[4])  uScene←mPostFXFBs[3].color draw
              mLastOutputFB = mPostFXFBs[4]
          → SceneRenderer::mLastSceneOutput = mPostFXFBs[4]

  [2] ScreenQuadStage::Render(*mDefaultTarget)
      → SetSources({mLastSceneOutput ?? mSceneFB})
      → passthrough 셰이더로 backbuffer 에 blit
```

---

## 3. 변경 파일 상세

### 3.1 `src/scene/layer.h`

`Layer::Screen = 1ull << 5` 추가.

```cpp
enum class Layer : uint64_t {
    Default   = 1ull << 0,
    Player    = 1ull << 1,
    Enemy     = 1ull << 2,
    UI        = 1ull << 3,
    DebugDraw = 1ull << 4,
    Screen    = 1ull << 5,   // ← NEW: PassComponent Actor 전용
    All       = ~0ull,
};
```

**디버깅**: WorldCamera 의 CullingMask 에 `Layer::Screen` 이 포함되면 PassComponent 가 WorldCamera 에서도 수집되어 WorldMesh 경로로 진입 → `meshRenderer==nullptr` 체크로 skip 되지만, 정렬이 뒤섞임. WorldCamera 의 `SetCullingMask` 에 Screen 이 빠져 있는지 확인.

---

### 3.2 `src/scene/camera.h` / `camera.cpp`

**추가 필드**:

```cpp
bool  IsOrthographic = false;
float OrthoSize      = 1.0f;   // 반높이 [-OrthoSize, OrthoSize]
```

**`GetProjectionMatrix()` ortho 분기** — `vmath::ortho()` 에는 `m[3][2]` 부호 버그 있음 (sb7 immutable 정책 — 수정 불가). OpenGL column-major 직접 구현:

```cpp
vmath::mat4 Camera::GetProjectionMatrix() const {
    if (IsOrthographic) {
        const float l = -OrthoSize * Aspect, r =  OrthoSize * Aspect;
        const float b = -OrthoSize,          t =  OrthoSize;
        const float n = NearZ,               f = FarZ;
        vmath::mat4 m = vmath::mat4::identity();
        m[0][0] =  2.0f / (r - l);
        m[1][1] =  2.0f / (t - b);
        m[2][2] = -2.0f / (f - n);
        m[3][0] = -(r + l) / (r - l);
        m[3][1] = -(t + b) / (t - b);
        m[3][2] = -(f + n) / (f - n);   // vmath::ortho 는 이 부호가 반대 → 직접 구현 이유
        return m;
    }
    return vmath::perspective(FovYDeg, Aspect, NearZ, FarZ);
}
```

**Screen Camera 설정** (main.cpp startup):

```cpp
screenCam->IsOrthographic = true;
screenCam->OrthoSize      = 1.0f;
screenCam->NearZ          = -1.0f;   // ← 주의: ortho 는 NearZ 음수 가능
screenCam->FarZ           =  1.0f;
screenCam->SetCullingMask(SJH::Scene::Layer::UI | SJH::Scene::Layer::Screen);
screenCam->SetTargetRenderTarget(mSceneFB.get());   // WorldCamera 와 동일 FB 공유
```

**디버깅**: Screen Camera 가 sceneFB 대신 nullptr 을 타겟으로 하면 SceneRenderer 가 warn+skip. `GetTargetRenderTarget()` 반환값 확인.

---

### 3.3 `src/render/pass_component.h` ← **신규 파일**

```cpp
#ifndef __SJH_PASS_COMPONENT_H__
#define __SJH_PASS_COMPONENT_H__
#include "scene/actor.h"

namespace SJH { class Framebuffer; class Material; }

namespace SJH::Scene {
    class PassComponent : public Component {
    public:
        PassComponent(Framebuffer* inputFB, Framebuffer* outputFB, Material* mat)
            : InputFB(inputFB), OutputFB(outputFB), mMaterial(mat) {}

        Framebuffer* const InputFB;    // readonly — const 포인터: 생성 후 교체 불가
        Framebuffer* const OutputFB;   // writable — 동일 제약
        Material*          mMaterial;
        bool               Enabled     = true;
        int                QueueOffset = 9000;

        virtual void OnEnter()  override {}
        virtual void OnExit()   override {}
        virtual void Update(float) override {}
    };
}
#endif
```

**중요 제약**: `InputFB`/`OutputFB` 는 `const` 포인터 멤버 → 생성 후 FB 교체 불가. 창 리사이즈 시 PassComponent Actor 를 소유한 부모 Actor 전체를 재생성하거나 `mPostFXFBs` 배열을 고정 크기로 유지하는 방법으로 우회. 현재 main.cpp 는 리사이즈 시 PassComponent 를 재생성하지 않는다 (창 고정 크기 운영).

---

### 3.4 `src/render/mesh_pass_processor.h`

**DrawCommand 에 Kind enum 및 ScreenQuad 전용 필드 추가**:

```cpp
struct DrawCommand {
    enum class Kind { WorldMesh, ScreenQuad };   // ← NEW

    // 공통
    Kind  kind       = Kind::WorldMesh;
    int   queueLayer = 2000;
    float depth      = 0.0f;

    // WorldMesh 전용
    const Scene::MeshRenderer* meshRenderer = nullptr;
    vmath::mat4                modelMatrix  = vmath::mat4::identity();

    // ScreenQuad 전용 (PassComponent)  ← NEW
    Framebuffer* inputFB      = nullptr;
    Framebuffer* outputFB     = nullptr;
    Material*    passMaterial = nullptr;
};
```

**MeshPassProcessor 에 추가된 API**:

```cpp
void SetScreenQuadMesh(Mesh* mesh);          // startup 에서 1회 호출
const Framebuffer* GetLastOutputFB() const;  // ScreenCamera 렌더 후 마지막 PassFB
void Clear();                                // mItems.clear() + mLastOutputFB = nullptr
```

**SortMultiStage 정렬 호환성**: ScreenQuad 의 `meshRenderer == nullptr` → 정렬 람다의 `aMat/bMat/aProg/bProg` 가 nullptr → `std::less<>` 비교에서 nullptr 은 정의된 total order 로 안전하게 처리. queueLayer=9000+ 이므로 WorldMesh 뒤에 자연 정렬됨.

---

### 3.5 `src/render/mesh_pass_processor.cpp`

**Process() ScreenQuad 분기** (95~127 라인):

```cpp
if (cmd.kind == DrawCommand::Kind::ScreenQuad) {
    if (!cmd.inputFB || !cmd.outputFB || !cmd.passMaterial || !mScreenQuadMesh)
        continue;
    auto* prog = cmd.passMaterial->GetProgram();
    if (!prog) continue;

    rc.BeginFrame(*cmd.outputFB);
    rc.SetDepthTest(false);
    rc.SetBlend(false);

    // uScene = inputFB 의 color attachment
    cmd.passMaterial->Properties.Textures["uScene"] = {
        cmd.inputFB->GetColorAttachment().get(), 0};

    rc.UseProgram(*prog);
    PropertyBlockSetter::Set(rc, cmd.passMaterial->Properties, *prog);

    rc.BindVAO(mScreenQuadMesh->GetVAO());
    if (auto ebo = mScreenQuadMesh->GetIndexBuffer()) ebo->Bind();  // VAO EBO 오염 가드
    rc.DrawIndexed(mScreenQuadMesh->GetIndexCount());

    rc.SetDepthTest(true);
    mLastOutputFB = cmd.outputFB;
    lastProg = nullptr;   // FB 전환 후 WorldMesh 가 program 재바인딩하도록 리셋
    lastMat  = nullptr;
    continue;
}
```

**디버깅 체크리스트**:
1. `mScreenQuadMesh == nullptr` → `mRenderSys.SetScreenQuadMesh(quadMesh)` 호출 여부 확인 (startup)
2. `cmd.passMaterial->GetProgram() == nullptr` → `mat->SetProgram(prog)` 가 호출됐는지 확인
3. ScreenQuad 후 WorldMesh 가 검은색으로 렌더 → `lastProg/lastMat = nullptr` 리셋이 빠진 경우

---

### 3.6 `src/render/scene_renderer.h` / `scene_renderer.cpp`

**추가**:

```cpp
// .h
void SetScreenQuadMesh(Mesh* mesh);
const Framebuffer* GetLastSceneOutput() const { return mLastSceneOutput; }

// private
const Framebuffer* mLastSceneOutput = nullptr;
```

**제거** (구 PostFX API 전부 삭제):

```
- #include "render/postfx_pass.h"
- void SetPostFXChain(std::vector<PostFXPass> chain)
- void ClearPostFXChain()
- const Framebuffer* GetActiveFXOutput() const
- void RunPostFXChain(RenderTarget&)
- std::vector<PostFXPass> mPostFXChain
- Mesh* mQuadMesh
- const Framebuffer* mLastActiveFXOutput
```

**CollectFromActor 에 PassComponent 수집 추가** (scene_renderer.cpp 123~136 라인):

```cpp
if (auto* pc = actor.GetComponent<Scene::PassComponent>()) {
    if (pc->Enabled && pc->InputFB && pc->OutputFB && pc->mMaterial) {
        DrawCommand cmd;
        cmd.kind         = DrawCommand::Kind::ScreenQuad;
        cmd.queueLayer   = pc->QueueOffset;
        cmd.inputFB      = pc->InputFB;
        cmd.outputFB     = pc->OutputFB;
        cmd.passMaterial = pc->mMaterial;
        mProcessor.Submit(cmd);
    }
}
```

**RenderWithCamera 에서 mLastSceneOutput 갱신** (scene_renderer.cpp 93~94 라인):

```cpp
mProcessor.Process(rc, viewMat, projMat);
if (auto* lastFB = mProcessor.GetLastOutputFB())
    mLastSceneOutput = lastFB;
```

---

### 3.7 `apps/_MyApp_/src/UI/PostFXDebugLayer.h` / `.cpp`

**Before**:

```cpp
PostFXDebugLayer(SceneRenderer& sys,
                 std::vector<PostFXPass>& passes,
                 Mesh* quadMesh, float& gamma);
```

**After** — SceneRenderer 비의존:

```cpp
struct PassDebugEntry {
    std::string             Name;
    SJH::Scene::PassComponent* Component;  // 비소유
};

class PostFXDebugLayer : public IImGuiLayer {
public:
    PostFXDebugLayer(std::vector<PassDebugEntry> passes, float& gamma);
    // ...
private:
    std::vector<PassDebugEntry> mPasses;
    float& mGamma;
};
```

**OnBuildUI**:

```cpp
void PostFXDebugLayer::OnBuildUI() {
    ImGui::Begin("PostFX Debug");
    for (auto& entry : mPasses) {
        if (!entry.Component) continue;
        ImGui::Checkbox(entry.Name.c_str(), &entry.Component->Enabled);
        if (entry.Name == "gamma" && entry.Component->Enabled) {
            if (ImGui::SliderFloat("gamma##val", &mGamma, 0.1f, 2.5f))
                if (entry.Component->mMaterial)
                    entry.Component->mMaterial->Properties.Floats["gamma"] = mGamma;
        }
    }
    ImGui::End();
}
```

---

### 3.8 `apps/_MyApp_/main.cpp` — 주요 변경 요약

**멤버 변경**:

```cpp
// 제거
// std::vector<SJH::PostFXPass>  mPostFXPasses;
// SJH::Mesh*                    mCachedQuadMesh;

// 추가
std::array<SJH::FramebufferUPtr, 5>      mPostFXFBs;      // 체인 FB 5개
std::vector<SJH::Scene::PassComponent*>  mPassComponents;  // 비소유
SJH::Scene::Actor*   mScreenCamActor = nullptr;
SJH::Scene::Camera*  mScreenCam      = nullptr;
```

**startup 핵심 배선**:

```cpp
// 1. WorldCamera
worldCam->SetCullingMask(Layer::Default | Layer::Player | Layer::Enemy | Layer::DebugDraw);
worldCam->SetTargetRenderTarget(mSceneFB.get());

// 2. ScreenCamera
screenCam->IsOrthographic = true;
screenCam->OrthoSize = 1.0f;
screenCam->SetCullingMask(Layer::UI | Layer::Screen);
screenCam->SetTargetRenderTarget(mSceneFB.get());  // 같은 FB 공유

// 3. PassComponent 체인
mRenderSys.SetScreenQuadMesh(quadMesh);
SJH::Framebuffer* prevFB = mSceneFB.get();
for (int i = 0; i < 5; ++i) {
    mPostFXFBs[i] = SJH::Framebuffer::Create(fbW, fbH);
    auto passActor = make_unique<Actor>("PassActor_" + name);
    passActor->SetLayer(Layer::Screen);
    auto* pc = passActor->AddComponent<PassComponent>(prevFB, mPostFXFBs[i].get(), mat);
    mPassComponents.push_back(pc);
    prevFB = mPostFXFBs[i].get();
    screenCamActor->AddChild(move(passActor));   // ← ScreenCamera Actor 의 자식으로!
}
dir.Root().AddChild(move(screenCamActor));
```

**render 핵심 흐름**:

```cpp
mRenderSys.Render(*mDefaultTarget);  // WorldCamera + ScreenCamera (PassComponent 실행)

auto* out = mRenderSys.GetLastSceneOutput();
mScreenQuadStage->SetSources({out ? out : mSceneFB.get()});
mScreenQuadStage->Render(*mDefaultTarget);   // 최종 passthrough → backbuffer
```

**onResize 처리** (리사이즈 시 PassComponent FB 는 재생성 안 함 — 현재 제약):

```cpp
mDefaultTarget = make_unique<DefaultRenderTarget>(w, h);
if (mCamera)    mCamera->Aspect    = (float)w / h;
if (mScreenCam) mScreenCam->Aspect = (float)w / h;
// mSceneFB 재생성 없음, mPostFXFBs 재생성 없음
// → 창 크기 변경 시 해상도 불일치 발생 가능 (현재 고정 크기 운영)
```

---

## 4. 불변 조건 (Invariants)

| # | 조건 | 위반 시 증상 |
|---|---|---|
| I1 | `PassComponent::InputFB != OutputFB` | GL 피드백 루프 → 렌더 오류 / 드라이버 경고 |
| I2 | 체인 배선: `pass[i].OutputFB == pass[i+1].InputFB` | 이전 패스 결과가 다음 패스에 전달 안 됨 → 검은 화면 |
| I3 | 첫 패스 InputFB == sceneFB | WorldCamera 결과가 PostFX 입력으로 들어가야 함 |
| I4 | ScreenCamera 의 `GetTargetRenderTarget() != nullptr` | SceneRenderer warn+skip → ScreenQuad 전혀 실행 안 됨 |
| I5 | PassActor 의 Layer == `Layer::Screen` | WorldCamera CullingMask 에서 제외되지 않으면 WorldMesh 경로 진입 |
| I6 | `mRenderSys.SetScreenQuadMesh(mesh)` 호출됨 | `mScreenQuadMesh == nullptr` → ScreenQuad cmd 모두 skip → 검은 화면 |
| I7 | `mLastOutputFB` 가 `Clear()` 에서 nullptr 로 리셋 | 이전 프레임 FB 가 현재 프레임 `GetLastSceneOutput()` 에 노출 |

---

## 5. 삭제된 API (deprecated)

아래 심볼은 코드베이스에서 **완전 제거됨** — 절대 재도입 금지.

```
SceneRenderer::SetPostFXChain(...)
SceneRenderer::ClearPostFXChain()
SceneRenderer::RunPostFXChain(...)
SceneRenderer::GetActiveFXOutput()
SceneRenderer::mPostFXChain
SceneRenderer::mQuadMesh
SceneRenderer::mLastActiveFXOutput
PostFXPass struct (src/render/postfx_pass.h 는 파일 잔존, include 없음)
```

`postfx_pass.h` 는 삭제되지 않았지만 어떤 `.cpp` / `.h` 에서도 `#include` 하지 않음. 파일 참조 시 이미 폐기된 디자인임을 인지.

---

## 6. 알려진 제약 및 미해결 이슈

### 6.1 PassComponent FB 리사이즈 미지원

`mPostFXFBs` 5개는 startup 시 생성된 크기(초기 fbW×fbH)로 고정. `onResize` / `render` 의 리사이즈 감지 로직이 `mSceneFB` 와 Camera aspect 만 갱신하고 `mPostFXFBs` 를 재생성하지 않음.

**증상**: 창 크기 변경 시 PostFX 에서 해상도 불일치로 스트레칭/크로핑 발생.

**해결 방안 (미구현)**:
1. 리사이즈 감지 시 PassActor(ScreenCamActor 의 자식들)를 모두 소멸 후 재생성 (PassComponent const 포인터 제약 때문에 필요)
2. 또는 FB를 포인터로 캡슐화해 핫스왑 가능하게 설계 (설계 변경 필요)

### 6.2 `postfx_pass.h` 잔존

`src/render/postfx_pass.h` 파일이 삭제되지 않고 남아있음. 미래 세션에서 혼란 방지를 위해 삭제하거나 `[DEPRECATED]` 주석 추가 권장.

### 6.3 ScreenQuadStage 의 SetSources fallback

```cpp
auto* out = mRenderSys.GetLastSceneOutput();
mScreenQuadStage->SetSources({out ? out : mSceneFB.get()});
```

PassComponent 가 하나도 없거나 전부 disabled 이면 `GetLastSceneOutput()` 이 nullptr → mSceneFB fallback → PostFX 없이 직접 blit. 의도된 동작.

### 6.4 VAO EBO 오염 가드

`ebo->Bind()` 재핀 코드는 Effekseer/Box2D 가 바인딩된 VAO의 EBO를 덮어쓰는 서드파티 오염 방지용. 제거 금지.

---

## 7. 디버깅 FAQ

### Q: 화면이 전부 검은색이다

체크 순서:
1. `mRenderSys.SetScreenQuadMesh(quadMesh)` 가 startup에서 호출됐는가?
2. `screenCam->SetTargetRenderTarget(mSceneFB.get())` 가 호출됐는가?
3. PassComponent 의 `InputFB != nullptr && OutputFB != nullptr && mMaterial != nullptr` 인가?
4. `mMaterial->GetProgram() != nullptr` — `mat->SetProgram(prog)` 가 호출됐는가?
5. 셰이더 컴파일 오류 여부 — spdlog `[error]` 로그 확인.

### Q: PostFX 가 적용되지 않고 원본 씬만 보인다

- `GetLastSceneOutput()` 이 nullptr 반환 → `mLastSceneOutput` 이 갱신 안 됨
  - ScreenCamera 가 SceneContext 에 등록됐는가? `OnEnter()` 가 호출됐는가?
  - `screenCamActor` 를 `dir.Root().AddChild()` 했는가?
- PassComponent 의 `Enabled == false` 인가?
- `Layer::Screen` 비트가 ScreenCamera CullingMask 에 없는가?

### Q: PostFX Debug ImGui 체크박스가 없다

`mPassComponents` 가 비어 있거나 `kPostFXDefs.size()` 와 크기 불일치. startup 의 체인 생성 루프에서 `prog == nullptr` 또는 `mPostFXFBs[i] == nullptr` 로 `continue` 된 경우. spdlog 에 `[PassComponent] 셰이더 로드 실패` 또는 `FB 생성 실패` 가 있는지 확인.

### Q: gamma 슬라이더가 적용되지 않는다

```cpp
// PostFXDebugLayer::OnBuildUI 에서:
entry.Component->mMaterial->Properties.Floats["gamma"] = mGamma;
```

`entry.Component->mMaterial` 이 nullptr 이거나 `Properties.Floats["gamma"]` 를 읽는 셰이더가 `gamma` uniform 이름을 맞게 사용하는지 확인 (`resources/shader/postprocess/gamma.fs`).

### Q: 피드백 루프 GL 오류가 발생한다

`cmd.inputFB == cmd.outputFB` → I1 위반. 체인 배선에서 `prevFB` 가 올바르게 갱신됐는지 확인:

```cpp
SJH::Framebuffer* prevFB = mSceneFB.get();
for (int i = 0; i < N; ++i) {
    pc = AddComponent<PassComponent>(prevFB, mPostFXFBs[i].get(), mat);
    prevFB = mPostFXFBs[i].get();   // ← 반드시 OutputFB 로 갱신
}
```

---

## 8. 관련 문서

| 문서 | 위치 | 내용 |
|---|---|---|
| PostFX 설계 지침 (구 패턴 분석) | [`doc/design/PostFX.md`](PostFX.md) | PostFXPass 설계 배경 + 4엔진 정통 분석 |
| Render refactor 세션 메모 | [`doc/design/2026-05-26-render-refactor-session.md`](2026-05-26-render-refactor-session.md) | 이전 세션 결정 체인 |
| EngineAPI 레퍼런스 | [`doc/EngineAPI.md`](../EngineAPI.md) | SceneRenderer / Camera 공개 API |
| 진행 보고서 | [`doc/topdown-shooter-progress.md`](../topdown-shooter-progress.md) | PassComponent 2-Camera 섹션 |
| 메모리 | `pass_component_postfx_pattern.md` | PassComponent 패턴 요약 + 생성 절차 |
| 메모리 | `camera_depth_postfx_misuse.md` | Camera.Depth PostFX 재활용 금기 근거 |
