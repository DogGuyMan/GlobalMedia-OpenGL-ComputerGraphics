# PassComponent + 2-Camera Architecture Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `PostFXPass` + `SceneRenderer::SetPostFXChain` 를 폐기하고, `PassComponent`(Component 패턴) + 2-Camera(World Perspective / Screen Orthographic) 아키텍처로 교체한다.

**Architecture:** Screen Camera 의 초기 `activeFB = sceneFB` (World Camera 출력 공유). QueueOffset 0~8999 HUD 는 sceneFB 에 직접 합성. QueueOffset 9000+ PassComponent DrawCommand 발화 시 `activeFB` 가 각 PassFB 로 전환. ScreenQuadStage 는 최종 PassFB 단일 소스를 backbuffer 에 blit.

**Tech Stack:** C++17, OpenGL 4.1, vmath (column-major mat4), GLFW 3.0.4, ImGui v1.53

---

## 파일 변경 목록

| 파일 | 작업 |
|---|---|
| `src/scene/layer.h` | `Layer::Screen = 1ull << 5` 추가 |
| `src/scene/camera.h` | `IsOrthographic`, `OrthoSize` 필드 추가 |
| `src/scene/camera.cpp` | `GetProjectionMatrix()` ortho 분기 추가 |
| `<src>/render/pass_component.h` | **신규** — PassComponent 헤더 온리 |
| `<src>/render/mesh_pass_processor.h` | DrawCommand `Kind` + `inputFB`/`outputFB`/`passMaterial`; MeshPassProcessor `SetScreenQuadMesh`/`GetLastOutputFB` |
| `<src>/render/mesh_pass_processor.cpp` | `Process()` ScreenQuad 분기 + activeFB 전환 |
| `<src>/render/scene_renderer.h` | `SetScreenQuadMesh`, `GetLastSceneOutput` 추가 |
| `<src>/render/scene_renderer.cpp` | `CollectFromActor` PassComponent 수집 추가 |
| `<apps>/_MyApp_/src/UI/PostFXDebugLayer.h` | `PassComponent*` 기반 인터페이스로 전면 교체 |
| `<apps>/_MyApp_/src/UI/PostFXDebugLayer.cpp` | `OnBuildUI` PassComponent::Enabled 토글로 교체 |
| `apps/_MyApp_/main.cpp` | 2-Camera 셋업, PassComponent 체인, render loop 갱신 |
| `<src>/render/scene_renderer.h/.cpp` | PostFX API 폐기 (Task 6 cleanup) |

**변경 없음:** `src/render/screen_quad_stage.h/.cpp` (API 유지, 단일 소스 호출만 변경)

---

## Task 1: Layer::Screen + Camera Orthographic 추가

**Files:**
- Modify: `src/scene/layer.h`
- Modify: `src/scene/camera.h`
- Modify: `src/scene/camera.cpp`

- [ ] **Step 1: `src/scene/layer.h` — Screen 비트 추가**

`DebugDraw = 1ull << 4,` 바로 다음 줄에 추가:

```cpp
Screen    = 1ull << 5,   ///< 32 — PassComponent Actor 전용 (ScreenCamera 전용 레이어)
```

- [ ] **Step 2: `src/scene/camera.h` — IsOrthographic + OrthoSize 추가**

`// Projection 파라미터.` 주석 바로 아래, `float FovYDeg = 45.0f;` 위에 삽입:

```cpp
bool  IsOrthographic = false;  ///< true 이면 ortho 투영 (Screen Camera 전용)
float OrthoSize      = 1.0f;  ///< 반높이 — 가시 범위 [-OrthoSize, OrthoSize]
```

- [ ] **Step 3: `src/scene/camera.cpp` — GetProjectionMatrix ortho 분기**

`GetProjectionMatrix()` 전체를 교체:

```cpp
vmath::mat4 Camera::GetProjectionMatrix() const
{
    if (IsOrthographic)
    {
        // vmath 에 ortho 없음 — 직접 구현 (OpenGL NDC z∈[-1,1], column-major)
        const float l = -OrthoSize * Aspect;
        const float r =  OrthoSize * Aspect;
        const float b = -OrthoSize;
        const float t =  OrthoSize;
        const float n =  NearZ;
        const float f =  FarZ;
        vmath::mat4 m = vmath::mat4::identity();
        m[0][0] =  2.0f / (r - l);
        m[1][1] =  2.0f / (t - b);
        m[2][2] = -2.0f / (f - n);
        m[3][0] = -(r + l) / (r - l);
        m[3][1] = -(t + b) / (t - b);
        m[3][2] = -(f + n) / (f - n);
        return m;
    }
    return vmath::perspective(FovYDeg, Aspect, NearZ, FarZ);
}
```

- [ ] **Step 4: 빌드 확인**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5
```

Expected: 에러 0.

- [ ] **Step 5: Commit**

```bash
git add src/scene/layer.h src/scene/camera.h src/scene/camera.cpp
git commit -m "$(cat <<'EOF'
feat(scene): Layer::Screen + Camera Orthographic 지원

PassComponent 2-Camera 아키텍처 준비.
- layer.h: Screen = 1ull<<5 (PassComponent Actor 전용 레이어)
- camera.h: IsOrthographic + OrthoSize 필드
- camera.cpp: GetProjectionMatrix ortho 분기 (vmath 미지원, 직접 구현)

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: PassComponent 헤더 신규

**Files:**
- Create: `<src>/render/pass_component.h`

`MeshRenderer` 패턴과 동일: `render/` 모듈, `SJH::Scene` 네임스페이스, 헤더 온리.

- [ ] **Step 1: `<src>/render/pass_component.h` 생성**

```cpp
#ifndef __SJH_PASS_COMPONENT_H__
#define __SJH_PASS_COMPONENT_H__

#include "scene/actor.h"  // Component 베이스 (render/ 가 scene/ 에 의존 — MeshRenderer 동일 패턴)

namespace SJH
{
    class Framebuffer;
    class Material;
} // namespace SJH

namespace SJH::Scene
{
    /// @brief 화면 공간 MeshRenderer — ScreenQuad + PostFX 셰이더 조합.
    /// @details
    ///   MeshRenderer 와 본질적으로 동일 — 단지 "화면 공간의 MeshRenderer".
    ///   포인터 공유 = 파이프라인 경계: Pass1.OutputFB == Pass2.InputFB (같은 포인터).
    ///   OpenGL 피드백 루프 없음 보장: 읽는 FB != 쓰는 FB 항상.
    class PassComponent : public Component
    {
      public:
        PassComponent(SJH::Framebuffer *inputFB, SJH::Framebuffer *outputFB, SJH::Material *mat)
            : InputFB(inputFB), OutputFB(outputFB), mMaterial(mat)
        {
        }

        SJH::Framebuffer *const InputFB;   ///< readonly — 이전 패스의 OutputFB 와 포인터 공유
        SJH::Framebuffer *const OutputFB;  ///< writable — 다음 패스의 InputFB 와 포인터 공유
        SJH::Material          *mMaterial;
        bool                    Enabled     = true;
        int                     QueueOffset = 9000;  ///< 월드 지오메트리(0~8999) 이후

        virtual void OnEnter() override  {}
        virtual void OnExit() override   {}
        virtual void Update(float) override {}
    };
} // namespace SJH::Scene

#endif // __SJH_PASS_COMPONENT_H__
```

- [ ] **Step 2: 빌드 확인**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5
```

Expected: 에러 0 (아직 아무도 include 하지 않아 정상).

---

## Task 3: DrawCommand 확장 + MeshPassProcessor ScreenQuad 처리

**Files:**
- Modify: `<src>/render/mesh_pass_processor.h`
- Modify: `<src>/render/mesh_pass_processor.cpp`

- [ ] **Step 1: `mesh_pass_processor.h` — DrawCommand Kind + 새 필드**

헤더 상단 forward declaration 블록 교체:

```cpp
namespace SJH::Scene { class MeshRenderer; }
namespace SJH
{
    class DeviceContext;
    class Framebuffer;
    class Material;
    class Mesh;
```

`DrawCommand` struct 전체 교체:

```cpp
struct DrawCommand
{
    enum class Kind { WorldMesh, ScreenQuad };

    // 공통
    Kind  kind       = Kind::WorldMesh;
    int   queueLayer = 2000;
    float depth      = 0.0f;  ///< view-space z (back-to-front)

    // WorldMesh 전용
    const Scene::MeshRenderer *meshRenderer = nullptr;  ///< SSoT — program/mesh/material/actor 경유
    vmath::mat4                modelMatrix  = vmath::mat4::identity();

    // ScreenQuad 전용 (PassComponent)
    Framebuffer *inputFB      = nullptr;  ///< 읽기 소스 — uScene 바인딩
    Framebuffer *outputFB     = nullptr;  ///< 쓰기 대상 + activeFB 갱신
    Material    *passMaterial = nullptr;  ///< ScreenQuad 셰이더
};
```

`MeshPassProcessor` 클래스 전체 교체:

```cpp
class MeshPassProcessor
{
public:
    void Submit(const DrawCommand& cmd) { mItems.push_back(cmd); }
    void Clear()
    {
        mItems.clear();
        mLastOutputFB = nullptr;  // 프레임마다 리셋
    }
    std::size_t Size() const { return mItems.size(); }

    void SetScreenQuadMesh(Mesh *mesh) { mScreenQuadMesh = mesh; }

    /// @brief 마지막 ScreenQuad(PassComponent) 의 outputFB.
    /// @return nullptr = 이번 프레임 PassComponent 없음 — caller 가 sceneFB fallback.
    const Framebuffer *GetLastOutputFB() const { return mLastOutputFB; }

    void SortMultiStage();

    void Process(DeviceContext& rc,
                 const vmath::mat4& viewMat,
                 const vmath::mat4& projMat);

private:
    std::vector<DrawCommand> mItems;
    Mesh              *mScreenQuadMesh = nullptr;
    const Framebuffer *mLastOutputFB   = nullptr;
};
```

- [ ] **Step 2: `mesh_pass_processor.cpp` — include + Process ScreenQuad 분기**

include 목록에 추가:

```cpp
#include "<buffer>/framebuffer.h"   // PassComponent ScreenQuad — BeginFrame(Framebuffer&)
```

`Process` 함수 전체 교체 (WorldMesh 로직 완전 보존, ScreenQuad 분기 추가):

```cpp
void MeshPassProcessor::Process(DeviceContext& rc,
                        const vmath::mat4& viewMat,
                        const vmath::mat4& projMat)
{
    const Program*       lastProg = nullptr;
    const Material*      lastMat  = nullptr;
    PipelineStateSetter  stateSetter;

    for (const auto& cmd : mItems)
    {
        // ── ScreenQuad (PassComponent) ────────────────────────────────────────
        if (cmd.kind == DrawCommand::Kind::ScreenQuad)
        {
            if (!cmd.inputFB || !cmd.outputFB || !cmd.passMaterial || !mScreenQuadMesh)
                continue;
            auto *prog = cmd.passMaterial->GetProgram();
            if (!prog)
                continue;

            rc.BeginFrame(*cmd.outputFB);
            rc.SetDepthTest(false);
            rc.SetBlend(false);

            cmd.passMaterial->Properties.Textures["uScene"] = {
                cmd.inputFB->GetColorAttachment().get(), 0};

            rc.UseProgram(*prog);
            PropertyBlockSetter::Set(rc, cmd.passMaterial->Properties, *prog);

            rc.BindVAO(mScreenQuadMesh->GetVAO());
            // VAO 오염 가드 — Effekseer/Box2D 가 EBO 를 덮어쓸 수 있음
            if (auto ebo = mScreenQuadMesh->GetIndexBuffer())
                ebo->Bind();
            rc.DrawIndexed(mScreenQuadMesh->GetIndexCount());

            rc.SetDepthTest(true);
            mLastOutputFB = cmd.outputFB;
            // FB 전환 후 program/material 상태 초기화 — 다음 WorldMesh 가 재바인딩
            lastProg = nullptr;
            lastMat  = nullptr;
            continue;
        }

        // ── WorldMesh ─────────────────────────────────────────────────────────
        if (!cmd.meshRenderer) continue;
        const Material* material = cmd.meshRenderer->Material;
        const Mesh*     mesh     = cmd.meshRenderer->Mesh;
        if (!material || !mesh) continue;
        const Program*  program  = material->GetProgram();
        if (!program) continue;

        if (program != lastProg) {
            rc.UseProgram(*program);
            if (program->GetLocation(Const::UNI_VIEW) >= 0)
                Uniforms::SetMat4(*program, Const::UNI_VIEW, viewMat);
            if (program->GetLocation(Const::UNI_PROJ) >= 0)
                Uniforms::SetMat4(*program, Const::UNI_PROJ, projMat);
            lastProg = program;
            lastMat  = nullptr;
        }

        if (material != lastMat) {
            PropertyBlockSetter::Set(rc, material->Properties, *program);
            lastMat = material;
        }

        const Pass::PipelineState passState = Pass::DefaultPipelineStateOf(material->GetPass());
        stateSetter.Set(passState);

        if (program->GetLocation(Const::UNI_MODEL) >= 0)
            Uniforms::SetMat4(*program, Const::UNI_MODEL, cmd.modelMatrix);
        rc.BindVAO(mesh->GetVAO());
        rc.DrawIndexed(mesh->GetIndexCount());
    }

    stateSetter.RestoreDefaults();
}
```

- [ ] **Step 3: 빌드 확인**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5
```

Expected: 에러 0.

- [ ] **Step 4: Commit**

```bash
git add <src>/render/mesh_pass_processor.h <src>/render/mesh_pass_processor.cpp
git commit -m "$(cat <<'EOF'
feat(render): DrawCommand Kind::ScreenQuad + MeshPassProcessor activeFB 전환

- DrawCommand: Kind enum + inputFB/outputFB/passMaterial (ScreenQuad 전용)
- MeshPassProcessor: SetScreenQuadMesh + GetLastOutputFB
- Process: ScreenQuad 분기 BeginFrame(outputFB) + uScene 바인딩 + activeFB 전환

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: SceneRenderer PassComponent 수집 + 새 API 추가

**Files:**
- Modify: `<src>/render/scene_renderer.h`
- Modify: `<src>/render/scene_renderer.cpp`

**주의:** PostFX API 는 이 Task 에서 *유지*. main.cpp 가 아직 사용하므로 Task 6 에서 제거.

- [ ] **Step 1: `scene_renderer.h` — 새 public 메서드 추가**

`SetPostFXChain` 선언 위에 추가:

```cpp
/// @brief ScreenQuad mesh 지정 — PassComponent DrawCommand 처리 시 사용.
void SetScreenQuadMesh(Mesh *mesh);

/// @brief 이번 프레임 PassComponent 체인의 마지막 출력 FB.
/// @return nullptr = PassComponent 없음 — caller 가 sceneFB fallback.
const Framebuffer *GetLastSceneOutput() const { return mLastSceneOutput; }
```

`private` 섹션에 멤버 추가:

```cpp
const Framebuffer *mLastSceneOutput = nullptr;
```

- [ ] **Step 2: `scene_renderer.cpp` — 구현 추가**

include 추가:

```cpp
#include "<render>/pass_component.h"
```

`SetPostFXChain` 구현 위에 추가:

```cpp
void SceneRenderer::SetScreenQuadMesh(Mesh *mesh)
{
    mProcessor.SetScreenQuadMesh(mesh);
}
```

`Render()` 함수 첫 줄에 초기화 추가:

```cpp
void SceneRenderer::Render(RenderTarget & /*defaultTarget*/)
{
    mLastSceneOutput = nullptr;  // 매 프레임 초기화
    // ...기존 cameras.empty() 체크 등 그대로 유지...
```

`RenderWithCamera()` 의 `mProcessor.Process(rc, viewMat, projMat);` 직후에 추가:

```cpp
if (auto *lastFB = mProcessor.GetLastOutputFB())
    mLastSceneOutput = lastFB;
```

`CollectFromActor()` 에서 MeshRenderer 수집 블록 직후 PassComponent 수집 추가:

```cpp
// ScreenQuad — PassComponent 수집 (신규)
if (auto *pc = actor.GetComponent<Scene::PassComponent>())
{
    if (pc->Enabled && pc->InputFB && pc->OutputFB && pc->mMaterial)
    {
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

- [ ] **Step 3: 빌드 확인**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5
```

Expected: 에러 0.

- [ ] **Step 4: Commit**

```bash
git add <src>/render/scene_renderer.h <src>/render/scene_renderer.cpp
git commit -m "$(cat <<'EOF'
feat(render): SceneRenderer PassComponent 수집 + GetLastSceneOutput

- CollectFromActor: PassComponent -> Kind::ScreenQuad DrawCommand 제출
- SetScreenQuadMesh: MeshPassProcessor 에 quad mesh 전달
- GetLastSceneOutput: 마지막 PassFB 반환 (ScreenQuadStage 소스용)

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: PostFXDebugLayer + main.cpp 2-Camera 통합 교체

**Files:**
- Modify: `<apps>/_MyApp_/src/UI/PostFXDebugLayer.h`
- Modify: `<apps>/_MyApp_/src/UI/PostFXDebugLayer.cpp`
- Modify: `apps/_MyApp_/main.cpp`

**주의:** 이 Task 전체를 완료해야 빌드 통과 (PostFXDebugLayer + main.cpp 동시 교체).

- [ ] **Step 1: `PostFXDebugLayer.h` 전체 교체**

```cpp
#ifndef __MYAPP_POSTFX_DEBUG_LAYER_H__
#define __MYAPP_POSTFX_DEBUG_LAYER_H__

#include "apps/_MyApp_/src/UI/IImGuiLayer.h"
#include "<render>/pass_component.h"
#include <string>
#include <vector>

namespace TopdownShooter::UI
{
    struct PassDebugEntry
    {
        std::string                Name;
        SJH::Scene::PassComponent *Component;  ///< 비소유
    };

    class PostFXDebugLayer : public IImGuiLayer
    {
      public:
        PostFXDebugLayer(std::vector<PassDebugEntry> passes, float &gamma);

        ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Editor; }
        void           OnBuildUI() override;

      private:
        std::vector<PassDebugEntry> mPasses;
        float                      &mGamma;
    };
} // namespace TopdownShooter::UI

#endif // __MYAPP_POSTFX_DEBUG_LAYER_H__
```

- [ ] **Step 2: `PostFXDebugLayer.cpp` 전체 교체**

```cpp
#include "<UI>/PostFXDebugLayer.h"
#include "material/material.h"
#include <imgui.h>

namespace TopdownShooter::UI
{
    PostFXDebugLayer::PostFXDebugLayer(std::vector<PassDebugEntry> passes, float &gamma)
        : mPasses(std::move(passes)), mGamma(gamma)
    {
    }

    void PostFXDebugLayer::OnBuildUI()
    {
        ImGui::Begin("PostFX Debug");

        for (auto &entry : mPasses)
        {
            if (!entry.Component)
                continue;

            ImGui::Checkbox(entry.Name.c_str(), &entry.Component->Enabled);

            if (entry.Name == "gamma" && entry.Component->Enabled)
            {
                if (ImGui::SliderFloat("gamma##val", &mGamma, 0.1f, 2.5f))
                {
                    if (entry.Component->mMaterial)
                        entry.Component->mMaterial->Properties.Floats["gamma"] = mGamma;
                }
            }
        }

        ImGui::End();
    }
} // namespace TopdownShooter::UI
```

- [ ] **Step 3: `main.cpp` — include 교체**

`#include "<render>/postfx_pass.h"` 제거.
`#include "<render>/pass_component.h"` 추가.

- [ ] **Step 4: `main.cpp` — private 멤버 교체**

제거:
```cpp
std::vector<SJH::PostFXPass>        mPostFXPasses;
SJH::Mesh                          *mCachedQuadMesh = nullptr;
```

추가 (mPostFXFBs 는 그대로 유지):
```cpp
std::vector<SJH::Scene::PassComponent *> mPassComponents;
SJH::Scene::Actor                       *mScreenCamActor = nullptr;
SJH::Scene::Camera                      *mScreenCam      = nullptr;
```

- [ ] **Step 5: `main.cpp` — startup() 카메라 셋업 블록 교체**

기존 단일 Camera 셋업:
```cpp
auto camActor = SJH::Scene::CreateCameraActor("MainCamera", 45.0f, aspect, 0.1f, 100.0f);
camActor->GetTransform().Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
camActor->GetTransform().EulerRot  = vmath::vec3(-45.0f, 0.0f, 0.0f);
auto *cam = camActor->GetComponent<SJH::Scene::Camera>();
auto *camCtrl = camActor->AddComponent<Controller::TargetFollowableCameraController>();
camCtrl->SetMouseInput(&mMouse)
    .SetCamera(cam)
    .SetUp();
cam->SetTargetRenderTarget(mSceneFB.get());

mCameraActor = dir.Root().AddChild(std::move(camActor));
mCamera      = cam;
```

를 다음 전체로 교체:

```cpp
// ── World Camera (Perspective) — 3D 월드 → sceneFB ─────────────────────────
auto worldCamActor = SJH::Scene::CreateCameraActor("WorldCamera", 45.0f, aspect, 0.1f, 100.0f);
worldCamActor->GetTransform().Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
worldCamActor->GetTransform().EulerRot  = vmath::vec3(-45.0f, 0.0f, 0.0f);
auto *worldCam = worldCamActor->GetComponent<SJH::Scene::Camera>();
auto *camCtrl  = worldCamActor->AddComponent<Controller::TargetFollowableCameraController>();
camCtrl->SetMouseInput(&mMouse).SetCamera(worldCam).SetUp();
worldCam->SetCullingMask(SJH::Scene::Layer::Default  |
                         SJH::Scene::Layer::Player   |
                         SJH::Scene::Layer::Enemy    |
                         SJH::Scene::Layer::DebugDraw);
worldCam->SetTargetRenderTarget(mSceneFB.get());
mCameraActor = dir.Root().AddChild(std::move(worldCamActor));
mCamera      = worldCam;

// ── Screen Camera (Orthographic) — HUD + PassComponent 체인 ────────────────
// 초기 activeFB = sceneFB (World Camera 출력 공유) — HUD 가 씬에 직접 합성.
auto screenCamActor = SJH::Scene::CreateCameraActor("ScreenCamera", 45.0f, aspect, -1.0f, 1.0f);
auto *screenCam     = screenCamActor->GetComponent<SJH::Scene::Camera>();
screenCam->IsOrthographic = true;
screenCam->OrthoSize      = 1.0f;
screenCam->SetCullingMask(SJH::Scene::Layer::UI | SJH::Scene::Layer::Screen);
screenCam->SetTargetRenderTarget(mSceneFB.get());

// ── PassComponent 체인 (blur→gamma→invert→sharpening→sobel) ─────────────────
mRenderSys.SetScreenQuadMesh(quadMesh);
mPassComponents.clear();
SJH::Framebuffer *prevFB = mSceneFB.get();  // 첫 패스 InputFB = sceneFB

for (std::size_t i = 0; i < kPostFXDefs.size(); ++i)
{
    const auto &def     = kPostFXDefs[i];
    const auto  progKey = std::string("postfx_") + def.Name;
    const auto  matKey  = std::string("mat_pass_") + def.Name;

    auto *prog = reg.CreateProgram(progKey, kPostFXVertFile, def.FragFile);
    if (!prog)
    {
        spdlog::error("[PassComponent] 셰이더 로드 실패: {}", def.FragFile);
        continue;
    }

    auto *mat = reg.CreateSharedMaterial(matKey);
    mat->SetProgram(prog);
    if (std::string(def.Name) == "gamma")
        mat->Properties.Floats["gamma"] = mGamma;

    mPostFXFBs[i] = SJH::Framebuffer::Create(fbW, fbH);
    if (!mPostFXFBs[i])
    {
        spdlog::error("[PassComponent] FB 생성 실패: {}", def.Name);
        continue;
    }

    auto passActor = std::make_unique<SJH::Scene::Actor>(std::string("PassActor_") + def.Name);
    passActor->SetLayer(SJH::Scene::Layer::Screen);

    auto *pc = passActor->AddComponent<SJH::Scene::PassComponent>(
        prevFB, mPostFXFBs[i].get(), mat);
    mPassComponents.push_back(pc);

    prevFB = mPostFXFBs[i].get();  // 다음 패스 InputFB = 이번 OutputFB (포인터 공유)

    screenCamActor->AddChild(std::move(passActor));
}

mScreenCamActor = dir.Root().AddChild(std::move(screenCamActor));
mScreenCam      = screenCam;
```

- [ ] **Step 6: `main.cpp` — BuildPostFXChain 메서드 제거**

`private:` 섹션의 `void BuildPostFXChain(...)` 메서드 전체 삭제.

- [ ] **Step 7: `main.cpp` — render() ScreenQuadStage 소스 교체**

기존:
```cpp
auto *fxOut = mRenderSys.GetActiveFXOutput();
mScreenQuadStage->SetSources({fxOut ? fxOut : mSceneFB.get()});
```

교체:
```cpp
auto *out = mRenderSys.GetLastSceneOutput();
mScreenQuadStage->SetSources({out ? out : mSceneFB.get()});
```

- [ ] **Step 8: `main.cpp` — render() resize 블록 Screen Camera Aspect 갱신 추가**

기존:
```cpp
if (mCamera)
    mCamera->Aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
mSceneFB = SJH::Framebuffer::Create(fbW, fbH);
if (mCamera)
    mCamera->SetTargetRenderTarget(mSceneFB.get());
```

교체 (mScreenFB 재생성 후 Screen Camera RT + PassComponent InputFB 갱신):
```cpp
if (mCamera)
    mCamera->Aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
if (mScreenCam)
    mScreenCam->Aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
mSceneFB = SJH::Framebuffer::Create(fbW, fbH);
if (mCamera)
    mCamera->SetTargetRenderTarget(mSceneFB.get());
if (mScreenCam)
    mScreenCam->SetTargetRenderTarget(mSceneFB.get());
```

**참고:** resize 시 mPostFXFBs 도 재생성이 필요하지만, 현재 PassComponent 의 InputFB/OutputFB 가 `const` 포인터이므로 resize 후 PassComponent 재생성 또는 포인터 업데이트 구조가 필요함. 현 단계에서는 resize 미지원 (창 크기 고정 운영)으로 처리. Task 7 수용 기준 체크리스트에서 확인.

- [ ] **Step 9: `main.cpp` — onResize() Screen Camera Aspect 추가**

기존 `onResize()` 에서:
```cpp
if (mCamera)
    mCamera->Aspect = static_cast<float>(w) / static_cast<float>(h);
```

다음 줄 추가:
```cpp
if (mScreenCam)
    mScreenCam->Aspect = static_cast<float>(w) / static_cast<float>(h);
```

- [ ] **Step 10: `main.cpp` — startup() ImGui 스택 PostFXDebugLayer 교체**

기존:
```cpp
mImGuiStack.Push(std::make_unique<UI::PostFXDebugLayer>(
    mRenderSys, mPostFXPasses, mCachedQuadMesh, mGamma));
```

교체:
```cpp
std::vector<UI::PassDebugEntry> debugEntries;
for (std::size_t i = 0; i < kPostFXDefs.size(); ++i)
{
    if (i < mPassComponents.size())
        debugEntries.push_back({kPostFXDefs[i].Name, mPassComponents[i]});
}
mImGuiStack.Push(std::make_unique<UI::PostFXDebugLayer>(
    std::move(debugEntries), mGamma));
```

- [ ] **Step 11: `main.cpp` — shutdown() 정리 추가**

`mCameraActor = nullptr;` 옆에:
```cpp
mCameraActor      = nullptr;
mScreenCamActor   = nullptr;
mScreenCam        = nullptr;
mPassComponents.clear();
```

- [ ] **Step 12: 빌드 확인**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | grep -E "error:" | head -20
```

Expected: 에러 0.

---

## Task 6: SceneRenderer PostFX API 폐기 (Cleanup)

**Files:**
- Modify: `<src>/render/scene_renderer.h`
- Modify: `<src>/render/scene_renderer.cpp`

main.cpp 가 더 이상 PostFX API 를 사용하지 않으므로 안전하게 제거.

- [ ] **Step 1: `scene_renderer.h` PostFX API 제거**

제거 항목:
- `#include "<render>/postfx_pass.h"` include
- `void SetPostFXChain(std::vector<PostFXPass> chain, Mesh* quadMesh);` 선언
- `void ClearPostFXChain();` 선언
- `const Framebuffer* GetActiveFXOutput() const { return mLastActiveFXOutput; }` 선언
- private: `void RunPostFXChain(const Framebuffer& sceneInput);` 선언
- private: `std::vector<PostFXPass> mPostFXChain;` 멤버
- private: `Mesh* mQuadMesh = nullptr;` 멤버
- private: `const Framebuffer* mLastActiveFXOutput = nullptr;` 멤버

- [ ] **Step 2: `scene_renderer.cpp` PostFX 구현 제거**

제거 항목:
- `SceneRenderer::SetPostFXChain(...)` 함수 전체
- `SceneRenderer::ClearPostFXChain()` 함수 전체
- `SceneRenderer::RunPostFXChain(...)` 함수 전체
- `Render()` 의 PostFX 체인 호출 블록 (mLastActiveFXOutput + RunPostFXChain 부분)

`Render()` 에서 제거할 블록:
```cpp
// 제거:
mLastActiveFXOutput = nullptr;
if (!mPostFXChain.empty() && mQuadMesh)
{
    for (auto *cam : cameras)
    {
        if (!cam->IsEnabled()) continue;
        if (auto *fb = dynamic_cast<Framebuffer *>(cam->GetTargetRenderTarget()))
        {
            RunPostFXChain(*fb);
            break;
        }
    }
}
```

- [ ] **Step 3: 빌드 확인**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5
```

Expected: 에러 0.

- [ ] **Step 4: Commit (Task 5 + 6 통합)**

```bash
git add <src>/render/scene_renderer.h <src>/render/scene_renderer.cpp \
        <apps>/_MyApp_/src/UI/PostFXDebugLayer.h \
        <apps>/_MyApp_/src/UI/PostFXDebugLayer.cpp \
        apps/_MyApp_/main.cpp
git commit -m "$(cat <<'EOF'
refactor(render+app): PassComponent 2-Camera 아키텍처 전환

PostFXPass + SetPostFXChain 폐기 → PassComponent Actor + Screen Camera 교체.
- WorldCamera(Perspective→sceneFB) + ScreenCamera(Ortho, activeFB=sceneFB 공유)
- PassComponent 체인: Blur→Gamma→Invert→Sharpening→Sobel (Layer::Screen)
- ScreenQuadStage: SetSources({lastPassFB}) 단일 소스
- PostFXDebugLayer: PassComponent::Enabled 토글 + gamma 슬라이더 재작성
- SceneRenderer: SetPostFXChain/RunPostFXChain/ClearPostFXChain/GetActiveFXOutput 제거

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: 빌드 + 동작 검증

- [ ] **Step 1: 클린 빌드**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -10
```

Expected: `[100%] Linking CXX executable _MyApp_` + 에러 0.

- [ ] **Step 2: 실행**

```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

- [ ] **Step 3: 수용 기준 체크**

```
[ ] 창 열림 — 3D 탑다운 씬 정상 렌더
[ ] 5개 PostFX 셰이더 적용 (씬 + HUD 포함 블러)
[ ] F1 → PostFX Debug 창 토글 (종료 버튼 항상 표시)
[ ] PostFX Debug 체크박스 5개 — Enabled 토글 즉시 반영
[ ] gamma 슬라이더 — 값 변경 시 씬 밝기 즉시 반영
[ ] 종료 버튼 클릭 → 창 닫힘
[ ] WASD 이동 + 마우스 드래그 카메라 정상
[ ] 마우스 좌클릭 → VFX + 사운드
[ ] G 키 → Damaged 이벤트
[ ] BGM 재생 중
[ ] Assertion / Crash 없음
```

- [ ] **Step 4: 폐기 확인**

```
[ ] SceneRenderer 에 SetPostFXChain 없음
[ ] SceneRenderer 에 RunPostFXChain 없음
[ ] SceneRenderer 에 GetActiveFXOutput 없음
[ ] main.cpp 에 mPostFXPasses / mCachedQuadMesh 없음
[ ] main.cpp 에 BuildPostFXChain 없음
```
