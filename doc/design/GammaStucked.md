# GammaStucked — 2-Camera + PassComponent 버그 디버깅 기록

> 발생 커밋: `57f5779 [refactor] : 2 camera`
> 베이스 커밋: `9e82e33 [dev] : Post FX 통합`
> 수정 날짜: 2026-05-27

---

## 개요

`57f5779` 리팩터에서 두 가지 독립적인 렌더링 버그가 발생했다.

| # | 증상 | 영향 범위 |
|---|------|----------|
| Bug 1 | MeshRenderer 전체가 화면에서 사라짐 | WorldCamera 가 그린 3D 씬 전체 |
| Bug 2 | PassComponent 비활성화 순간 `mSceneFB` 가 정지 화면처럼 동결 | Gamma off 시 체인 전체 갱신 중단 |

두 버그 모두 **2-카메라 구조가 같은 `mSceneFB` 를 공유**하는 설계와 **PassComponent `*const` 포인터 제약**에서 비롯되었다.

---

## Bug 1 — MeshRenderer 가 사라짐

### 관찰

- Effekseer 파티클 / ImGui 는 정상 렌더됨
- Actor 트리에 MeshRenderer 가 있는 모든 오브젝트가 사라짐
- `9e82e33` 에서 체크아웃하면 즉시 복구됨

### 원인 분석

`57f5779` 는 단일 카메라에서 **WorldCamera + ScreenCamera** 2-카메라 구조로 전환했다. 두 카메라 모두 같은 `mSceneFB` 를 렌더 타겟으로 지정 (의도된 설계 — ScreenCamera 가 World 출력 위에 HUD/PassComponent 를 합성).

`SceneRenderer::Render` 는 카메라 목록을 등록 순서대로 순회하며 `RenderWithCamera` 를 호출한다. 당시 `RenderWithCamera` 는 조건 없이 `rc.BeginFrame(*rt)` 를 호출했다.

```
DeviceContext::BeginFrame(RenderTarget &target):
  BindTarget(target)
  glClear(COLOR | DEPTH | STENCIL)   ← 무조건 clear
  SetDepthTest(true)
  SetBlend(true)
```

따라서 프레임 흐름:

```
[1] WorldCamera  → BeginFrame(mSceneFB) → clear → 3D 씬 그리기   ✓
[2] ScreenCamera → BeginFrame(mSceneFB) → clear ← [1] 출력 소거!
                                        → PassComponent 그리기
```

ScreenCamera 의 `BeginFrame` 이 WorldCamera 출력을 덮어쓴 것이 원인.

### 검토한 오답

> "ScreenCamera 에 별도 FB 를 주면 된다"

설계 요구사항: **ScreenCamera 는 반드시 `mSceneFB` 를 사용해야 한다** (World 출력 위에 HUD 합성). FB 를 바꾸면 합성이 깨진다. 이 접근은 기각.

### 수정 내용

#### `src/scene/camera.h`

`NoClear` 플래그 추가. ScreenCamera 전용으로 "이전 카메라 RT 출력을 보존한 채 바인딩만 수행"하는 모드를 활성화한다.

```cpp
// 추가된 필드
/// RenderWithCamera 진입 시 BeginFrame(clear) 대신 BindTarget+state 만 수행.
/// ScreenCamera 전용 — WorldCamera 출력을 보존한 채 합성.
bool NoClear = false;
```

#### `src/render/scene_renderer.cpp` — `RenderWithCamera`

`BeginFrame` 단일 경로에서 `NoClear` 분기로 변경:

```cpp
// 변경 전
rc.BeginFrame(*rt);

// 변경 후
if (cam.NoClear)
{
    rc.BindTarget(*rt);      // clear 없이 RT 바인딩 + state 만
    rc.SetDepthTest(true);
    rc.SetBlend(true);
}
else
{
    rc.BeginFrame(*rt);      // 기존 경로 (WorldCamera)
}
```

#### `apps/_MyApp_/main.cpp`

ScreenCamera 생성 후 `NoClear = true` 설정:

```cpp
auto *screenCam = screenCamActor->GetComponent<SJH::Scene::Camera>();
screenCam->IsOrthographic = true;
screenCam->OrthoSize      = 1.0f;
screenCam->NoClear        = true;  // WorldCamera 출력 보존 — clear 없이 합성
screenCam->SetCullingMask(SJH::Scene::Layer::UI | SJH::Scene::Layer::Screen);
screenCam->SetTargetRenderTarget(mSceneFB.get());
```

---

## Bug 2 — PassComponent 비활성화 시 화면 동결

### 관찰

- ImGui 에서 Gamma PassComponent 의 `Enabled` 를 false 로 끄는 순간 `mSceneFB` 의 최종 출력이 정지됨
- 이후 프레임이 갱신되지 않고, 끈 시점의 스냅샷이 그대로 유지됨
- 다시 `Enabled = true` 로 켜면 즉시 복구됨

### 원인 분석

PassComponent 체인 구조:

```
mSceneFB → [blurring] → postFXFBs[0]
         → [gamma]    → postFXFBs[1]
         → [invert]   → postFXFBs[2]
         → [sharp]    → postFXFBs[3]
         → [sobel]    → postFXFBs[4]
```

각 `PassComponent` 의 `InputFB` / `OutputFB` 는 **`*const` 포인터** 로 생성 후 재배선 불가:

```cpp
class PassComponent : public Component {
public:
    SJH::Framebuffer *const InputFB;    // 불변
    SJH::Framebuffer *const OutputFB;   // 불변
    bool Enabled = true;
    // ...
};
```

당시 `CollectFromActor` 는 `Enabled == false` 인 PassComponent 를 **submit 자체를 skip** 했다:

```cpp
// 변경 전 (문제 코드)
if (pc->Enabled && pc->InputFB && pc->OutputFB && pc->mMaterial)
{
    // Enabled 가 false 이면 이 블록 전체 skip
    // → OutputFB 는 그 프레임 아무도 쓰지 않음 → stale 유지
    DrawCommand cmd; /* ... */ mProcessor.Submit(cmd);
}
```

Gamma 가 꺼지면 `postFXFBs[1]` 은 갱신되지 않는다. 그러나 Invert 의 `InputFB` 는 여전히 `postFXFBs[1]` 을 가리키므로, Invert 는 지난 프레임의 Gamma 출력(정지 이미지)을 읽어 blit 한다. 이후 Sharp → Sobel 도 동일하게 stale 데이터를 증폭시킨다.

### 제약 확인 — `*const` 포인터는 재배선 불가

"Gamma 가 꺼지면 Gamma 의 `InputFB(postFXFBs[0])` 를 직접 Invert 의 `InputFB` 로 교체하면 된다" 는 접근은 불가능하다. `PassComponent::InputFB` 가 `*const` 이므로 생성 후 포인터 자체를 변경할 수 없다.

따라서 **포인터를 바꾸는 게 아니라, disabled 패스도 항상 `InputFB → OutputFB` 를 갱신**해야 한다. 이 때 쓰는 셰이더가 "아무 가공 없이 복사만 하는 passthrough" — 이를 **Bypass-Rewire 패턴**이라 한다.

### 수정 내용

#### `src/render/mesh_pass_processor.h`

bypass material 필드 + setter 추가:

```cpp
// 추가된 public 메서드
/// @brief disabled PassComponent 의 bypass blit 에 사용할 passthrough material.
/// @details cmd.passMaterial == nullptr 일 때 이 material 로 inputFB → outputFB blit.
void SetBypassMaterial(Material *mat) { mBypassMat = mat; }

// 추가된 private 필드
Material *mBypassMat = nullptr;
```

#### `src/render/mesh_pass_processor.cpp` — `Process()`

`cmd.passMaterial == nullptr` 을 bypass 신호로 해석:

```cpp
// 변경 전
if (!cmd.inputFB || !cmd.outputFB || !cmd.passMaterial || !mScreenQuadMesh)
    continue;
auto *prog = cmd.passMaterial->GetProgram();
// ...
cmd.passMaterial->Properties.Textures["uScene"] = { ... };
rc.UseProgram(*prog);
PropertyBlockSetter::Set(rc, cmd.passMaterial->Properties, *prog);

// 변경 후
if (!cmd.inputFB || !cmd.outputFB || !mScreenQuadMesh)
    continue;
// passMaterial == nullptr → disabled 패스 bypass: passthrough blit
Material *effectiveMat = cmd.passMaterial ? cmd.passMaterial : mBypassMat;
if (!effectiveMat) continue;
auto *prog = effectiveMat->GetProgram();
// ...
effectiveMat->Properties.Textures["uScene"] = { ... };
rc.UseProgram(*prog);
PropertyBlockSetter::Set(rc, effectiveMat->Properties, *prog);
```

#### `src/render/scene_renderer.h`

`Material` forward declaration + `SetBypassMaterial` 퍼블릭 API 추가:

```cpp
// 변경 전
namespace SJH { class RenderTarget; class Framebuffer; class Mesh; }

// 변경 후
namespace SJH { class RenderTarget; class Framebuffer; class Mesh; class Material; }
```

```cpp
// 추가된 public 메서드
/// @brief disabled PassComponent 의 bypass blit material 지정.
void SetBypassMaterial(Material *mat);
```

#### `src/render/scene_renderer.cpp`

`SetBypassMaterial` 구현 + `CollectFromActor` 수집 로직 변경:

```cpp
// 추가된 구현
void SceneRenderer::SetBypassMaterial(Material *mat)
{
    mProcessor.SetBypassMaterial(mat);
}
```

```cpp
// CollectFromActor — PassComponent 수집 변경 전
if (pc->Enabled && pc->InputFB && pc->OutputFB && pc->mMaterial)
{
    DrawCommand cmd;
    cmd.passMaterial = pc->mMaterial;
    mProcessor.Submit(cmd);
}

// CollectFromActor — PassComponent 수집 변경 후
if (pc->InputFB && pc->OutputFB)   // Enabled 체크 제거
{
    DrawCommand cmd;
    // nullptr = bypass signal (disabled → passthrough blit)
    cmd.passMaterial = (pc->Enabled && pc->mMaterial) ? pc->mMaterial : nullptr;
    mProcessor.Submit(cmd);
}
```

#### `apps/_MyApp_/main.cpp`

startup 에서 bypass material 생성 및 등록:

```cpp
mRenderSys.SetScreenQuadMesh(quadMesh);
{
    auto *bypassMat = reg.CreateSharedMaterial("mat_bypass_passthrough");
    bypassMat->SetProgram(passthroughProg);  // 기존 passthrough 셰이더 재사용
    mRenderSys.SetBypassMaterial(bypassMat);
}
```

---

## 수정 후 렌더 흐름

### Bug 1 수정 후 — 2-카메라 렌더 순서

```
[Frame N]
  WorldCamera  → BindTarget(mSceneFB) + Clear + 3D 씬 그리기
  ScreenCamera → BindTarget(mSceneFB) [NoClear: clear 없음] + PassComponent 체인 실행
```

### Bug 2 수정 후 — Gamma disabled 시 체인

```
Gamma disabled:
  mSceneFB
    → [blurring]     active   → postFXFBs[0] (정상 갱신)
    → [gamma]        BYPASS   → postFXFBs[1] (passthrough: [0] 그대로 복사, 매 프레임 갱신)
    → [invert]       active   → postFXFBs[2] (정상 갱신)
    → [sharpening]   active   → postFXFBs[3] (정상 갱신)
    → [sobel]        active   → postFXFBs[4] (정상 갱신)
```

체인이 끊기지 않으며, disabled 패스는 입력을 그대로 출력으로 통과시킨다.

---

## 변경 파일 요약

| 파일 | 변경 내용 |
|------|----------|
| `src/scene/camera.h` | `NoClear` 플래그 추가 |
| `src/render/scene_renderer.h` | `Material` forward decl + `SetBypassMaterial()` 추가 |
| `src/render/scene_renderer.cpp` | `RenderWithCamera` NoClear 분기 + `SetBypassMaterial` 구현 + `CollectFromActor` bypass 신호 수정 |
| `src/render/mesh_pass_processor.h` | `mBypassMat` 필드 + `SetBypassMaterial()` 추가 |
| `src/render/mesh_pass_processor.cpp` | `Process()` ScreenQuad 경로에서 `effectiveMat` fallback 처리 |
| `apps/_MyApp_/main.cpp` | `screenCam->NoClear = true` + bypass material 생성/등록 |
