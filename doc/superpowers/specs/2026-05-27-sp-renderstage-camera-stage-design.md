# SP-RenderStage 완성 — CameraStage 정착 (선행 SP)

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **작성일**: 2026-05-27
> **선행 결정 문서**: [`doc/design/OptimizeRenderTarget.md`](../../../doc/design/OptimizeRenderTarget.md) §4 (SP-RenderStage 후보), [`doc/handoffs/2026-05-27/2026-05-27-pass-component-2camera-session.md`](../../../doc/design/2026-05-27-pass-component-2camera-session.md) (2-Camera 패턴), [`doc/design/PostFX.md`](../../../doc/design/PostFX.md) (ordered pass list)
> **후속 SP**: ParticleStage (Effekseer→sceneFB 통합) — 본 문서 §8 의 인계 프롬프트로 별도 session 진입
> **대상 데모**: `apps/_MyApp_` 만 (migrate_demo / audio_demo 는 `[[deprecated]]` 경고만 발생, 점진 마이그레이션)

---

## 1. 동기 — 왜 이 SP 가 필요한가

### 1.1 직접 동기 — Effekseer 통합의 *사전 작업*

`apps/_MyApp_/main.cpp:327-333` 의 Effekseer Draw 가 **backbuffer 에 직접 그려져 PostFX 미적용**. 4-엔진 정통 (Unity/Unreal/Godot/Cocos) 은 *파티클을 씬 단계 후, PostFX 전* 에 그려 PostFX 와 동일 처리. 이를 달성하려면 `WorldCamera 렌더` 와 `ScreenCamera (PassComponent) 렌더` *사이에* Effekseer Draw 를 끼울 수 있어야 한다.

### 1.2 현 구조의 한계

`SceneRenderer::Render(rt)` 가 SceneContext.GetCameras() 자동 순회로 *World+Screen 두 카메라를 한 번에* 그림 — *사이 hook 불가*. 카메라 분할 호출이 필요한 모든 통합 (Effekseer / DebugDraw / Skybox 후처리 / Custom Postpass) 이 같은 구조 한계에 막힘.

### 1.3 이미 부분 정착된 패턴 활용

```
<src>/render/render_stage.h     — IRenderStage 추상 (이미 존재)
<src>/render/scene_renderer.h   — SceneRenderer : public IRenderStage (이미 상속)
<src>/render/screen_quad_stage.h — ScreenQuadStage : public IRenderStage (이미 상속)
```

SP-RenderStage 가 **부분 정착** 상태. 본 SP 는 *새 추상 도입이 아니라* 기존 추상의 자연스러운 확장 — 단일 카메라를 IRenderStage 로 wrap 하는 `CameraStage` 정착.

---

## 2. 채택 결정 (확정 6종)

### 결정 1 — 단일 `CameraStage` 클래스

World/Screen 둘 다 *동일 인스턴스로* 표현 — 코드 중복 0. Unity URP `ScriptableRenderPass` 상속 표현 정통.

```cpp
class CameraStage : public IRenderStage {
public:
    CameraStage(SceneRenderer* renderer, Scene::Camera* camera);
    void Render(RenderTarget& /*unused*/) override;   // 카메라 자기 RT 사용
private:
    SceneRenderer* mRenderer;
    Scene::Camera* mCamera;
};
```

### 결정 2 — `SceneRenderer::Render(rt)` 유지 + `[[deprecated]]` attribute

세부 옵션 1b 채택 — *컴파일 경고로 점진적 마이그레이션 유도*. migrate_demo / audio_demo 는 호출 그대로, 경고만 발생. _MyApp_ 만 새 stages path.

### 결정 3 — `SceneRenderer::RenderWithCamera` private → public

`CameraStage::Render` 가 위임할 수 있도록 노출. Cocos2D `Scene::visit` (자동) + `visitNode(node)` (수동) 공존 정통.

### 결정 4 — `ScreenQuadStage` 는 *이미 IRenderStage* → stages 컬렉션이 흡수

`mScreenQuadStage` 멤버 (unique_ptr) 를 stages 컬렉션이 *소유권 인수*. main.cpp 의 별도 멤버 제거.

### 결정 5 — `Effekseer / VFX.Draw` 위치는 *본 SP 에서 손대지 않음*

backbuffer 직접 그리기 (`main.cpp:327-333`) 그대로. **다음 SP (ParticleStage)** 가 흡수. 본 SP 의 *시각 결과 = 기존과 100% 동일*.

### 결정 6 — ImGui 위치는 *본 SP 에서 손대지 않음*

`mImGuiStack.RenderAll(...)` + `ImGui::Render()` 그대로 (stages 외부). ImGui 의 IRenderStage 화는 별도 SP 후보 (현 task 범위 외).

---

## 3. 변경 명세

### 3.1 파일별 변경표

| # | 파일 | 변경 | 행수 |
|---|---|---|---|
| 0 | [`cmake/CXXStandard.cmake`](../../../cmake/CXXStandard.cmake) | Debug `-Werror` 의 예외 추가 — `-Wno-error=deprecated-declarations`. 결정 2 의 `[[deprecated]]` attribute 가 migrate_demo / audio_demo 빌드를 막지 않도록. 기존 `-Wno-error=unused-but-set-variable` / `-Wno-error=pessimizing-move` 와 동일 정착 패턴. | +1 |
| 1 | [`<src>/render/scene_renderer.h`](../../../src/render/scene_renderer.h) | `RenderWithCamera(Scene::Camera&)` private → public. `Render(rt)` 위에 `[[deprecated("Use CameraStage + IRenderStage stages 컬렉션 — Application 이 출차 책임 (4-엔진 정통)")]]` attribute. | ±2 |
| 2 | `<src>/render/camera_stage.h` *(신규)* | `class CameraStage : public IRenderStage` 선언. ctor + Render override + 멤버 2개. | +35 |
| 3 | `<src>/render/camera_stage.cpp` *(신규)* | `Render` 본구현 = `mRenderer->RenderWithCamera(*mCamera)` 단일 위임. nullptr 가드 + spdlog warn. | +20 |
| 4 | [`src/render/CMakeLists.txt`](../../../src/render/CMakeLists.txt) | `camera_stage.cpp` 등록 (`target_sources` 의 `PRIVATE` 목록 합류). | +1 |
| 5 | [`apps/_MyApp_/main.cpp`](../../../apps/_MyApp_/main.cpp) | (a) 멤버 추가: `std::vector<std::unique_ptr<SJH::IRenderStage>> mStages` + 비소유 raw `SJH::ScreenQuadStage* mScreenQuadStagePtr = nullptr`. (b) startup 끝에 `CameraStage(worldCam) → CameraStage(screenCam) → ScreenQuadStage` 3개 등록 — 기존 `mScreenQuadStage` unique_ptr 의 *소유권 stages 로 이전* 시 raw 포인터를 `mScreenQuadStagePtr` 에 캐싱. (c) render() 의 `mRenderSys.Render(...)` + `mScreenQuadStage->Render(...)` 두 줄을 `mScreenQuadStagePtr->SetSources(...)` 1줄 + `for (auto& s : mStages) s->Render(*mDefaultTarget);` 한 줄로 교체. (d) 기존 `std::unique_ptr<ScreenQuadStage> mScreenQuadStage` 멤버 제거. | ±22 |

### 3.2 새 헤더 — `<src>/render/camera_stage.h`

```cpp
#ifndef __SJH_CAMERA_STAGE_H__
#define __SJH_CAMERA_STAGE_H__

#include "<render>/render_stage.h"

namespace SJH::Scene { class Camera; }

namespace SJH
{
    class SceneRenderer;

    /// @brief 단일 Camera 를 IRenderStage 로 래핑.
    /// @details Application 이 카메라 명시 순서를 결정 — Unity URP `ScriptableRenderPass`
    ///          상속 표현 정통. SceneContext.GetCameras() 자동 순회를 사용하지 않는다.
    /// @note    target 인자는 사용하지 않음 — Camera 가 자기 SetTargetRenderTarget() 사용.
    class CameraStage : public IRenderStage
    {
      public:
        CameraStage(SceneRenderer* renderer, Scene::Camera* camera);

        /// @brief SceneRenderer::RenderWithCamera(*mCamera) 위임.
        void Render(RenderTarget& target) override;

      private:
        SceneRenderer* mRenderer;
        Scene::Camera* mCamera;
    };
}

#endif // __SJH_CAMERA_STAGE_H__
```

### 3.3 새 .cpp — `<src>/render/camera_stage.cpp`

```cpp
#include "<render>/camera_stage.h"
#include "<render>/scene_renderer.h"
#include "scene/camera.h"
#include <<spdlog>/spdlog.h>

namespace SJH
{
    CameraStage::CameraStage(SceneRenderer* renderer, Scene::Camera* camera)
        : mRenderer(renderer), mCamera(camera) {}

    void CameraStage::Render(RenderTarget& /*target*/)
    {
        if (!mRenderer || !mCamera)
        {
            spdlog::warn("CameraStage::Render — renderer/camera nullptr — skip.");
            return;
        }
        mRenderer->RenderWithCamera(*mCamera);
    }
}
```

### 3.4 main.cpp 변경 골격

```cpp
// 멤버
std::vector<std::unique_ptr<SJH::IRenderStage>> mStages;
SJH::ScreenQuadStage*                           mScreenQuadStagePtr = nullptr;  // 비소유 raw — stages 가 owner
// 제거: std::unique_ptr<SJH::ScreenQuadStage> mScreenQuadStage;

// startup() 끝에 stages 등록 (Camera Component 의 OnEnter 자동 등록 직후)
{
    auto* worldCam  = mCameraActor->GetComponent<SJH::Scene::Camera>();
    auto* screenCam = mScreenCamActor->GetComponent<SJH::Scene::Camera>();

    mStages.push_back(std::make_unique<SJH::CameraStage>(&mRenderSys, worldCam));
    mStages.push_back(std::make_unique<SJH::CameraStage>(&mRenderSys, screenCam));

    auto sqOwned        = std::make_unique<SJH::ScreenQuadStage>(/* 기존 ctor 인자 */);
    mScreenQuadStagePtr = sqOwned.get();   // raw 캐싱 (dynamic_cast 회피)
    // ScreenQuadStage 의 기존 셋업 (mesh / material / 등) 그대로 이주.
    mStages.push_back(std::move(sqOwned));
}

// render() — 기존 두 줄을 한 줄로 (raw 포인터로 SetSources, dynamic_cast 회피)
// 변경 전:
//   mRenderSys.Render(*mDefaultTarget);
//   {
//       auto* out = mRenderSys.GetLastSceneOutput();
//       mScreenQuadStage->SetSources({out ? out : mSceneFB.get()});
//       mScreenQuadStage->Render(*mDefaultTarget);
//   }
// 변경 후:
{
    // ScreenQuadStage 의 SetSources 갱신은 *stages 순회 직전* 에 — raw 캐시 사용.
    {
        auto* out = mRenderSys.GetLastSceneOutput();
        mScreenQuadStagePtr->SetSources({out ? out : mSceneFB.get()});
    }
    for (auto& s : mStages) s->Render(*mDefaultTarget);
}

// ⚠️ SetSources/GetLastSceneOutput 의 *시점* — stages 순회 시작 전.
//    이유: WorldCamera 가 sceneFB 그리기 완료 후 → ScreenCamera 가 PassComponent 체인 실행 →
//          PassComponent 마지막 출력이 mRenderSys.GetLastSceneOutput() 에 갱신 →
//          그 다음 ScreenQuadStage 가 backbuffer 합성.
//    GetLastSceneOutput() 은 *지난 프레임* 값이므로 prev-frame stale 가능 — §7.1 의
//    known issue. 기존 동작 그대로 보존 (본 SP 에서 건드리지 않음).
```

### 3.5 시각 흐름 (변경 후)

```
[Frame N]
  for (auto& s : mStages) s->Render(*mDefaultTarget):
    [0] CameraStage(worldCam)
        → SceneRenderer::RenderWithCamera(worldCam)
        → BeginFrame(sceneFB) + Clear + 3D WorldMesh 렌더
    [1] CameraStage(screenCam)
        → SceneRenderer::RenderWithCamera(screenCam)
        → NoClear: BindTarget(sceneFB) + PassComponent 체인 실행
    [2] ScreenQuadStage
        → SetSources({GetLastSceneOutput() ?? sceneFB})
        → backbuffer 에 passthrough blit

  [기존 그대로]
  VFX.Draw(view, proj)        ← backbuffer 직접 (다음 SP 에서 흡수)
  ImGui::Render()
```

**시각 결과**: stages 순회 = 기존 `Render(rt)` 자동 순회 + 기존 `mScreenQuadStage->Render` 와 *완전 동일*. Effekseer 위치 불변.

---

## 4. 손대지 않는 것 (SP 경계)

- ✅ **migrate_demo / audio_demo**: `Render(rt)` 호출 그대로. `[[deprecated]]` 컴파일 경고만 발생.
- ✅ **SceneContext.GetCameras() 자동 등록**: Camera Component `OnEnter` 자동 등록 유지. _MyApp_ 가 *stages path 만 사용* → 자동 순회 path 가 *호출되지 않음* (Render(rt) 미호출).
- ✅ **Camera / PassComponent / ScreenQuadStage / SceneRenderer 내부 로직**: 변경 0.
- ✅ **VFX / Effekseer 위치**: backbuffer 직접 (다음 SP).
- ✅ **ImGui 위치**: stages 외부 (별도 SP 후보).

---

## 5. 회귀 검증

| 항목 | 방법 |
|---|---|
| 시각 회귀 | `_MyApp_` 실행 — distortion 파티클 + WASD 이동 + 좌클릭 발사 시퀀스 + ImGui 토글 — 변경 전과 동일 시각 확인 |
| PostFX 토글 | ImGui 의 PostFXDebugLayer 에서 gamma / invert / blur / sobel / sharpening 각각 토글 — 기존 동작 동일 |
| 컴파일 경고 | migrate_demo / audio_demo 빌드 시 `[[deprecated]]` 경고 발생 확인 (에러 아님) |
| 단위 테스트 | 본 SP 에서 신규 작성 없음 (사용자 정책 `no_auto_tests` 정착) |

---

## 6. 정통 매핑

| 결정 | Unity | Unreal | Godot | Cocos |
|---|---|---|---|---|
| `IRenderStage` 추상 | `ScriptableRendererFeature` | `FSceneRenderer` | `CompositorEffect` | Custom Render Pipeline Pass |
| `CameraStage` 단일 카메라 wrap | `ScriptableRenderPass` 상속 | `FSceneViewExtension` | `Camera3D` per-view | Camera per-pass |
| `[[deprecated]]` 점진 정착 | URP/HDRP 전환 시 BuiltIn 경고 | Material BlendableLocation deprecation | Compositor 도입 시 | 정통 동일 |

---

## 7. 알려진 이슈 / Future Work

### 7.1 `GetLastSceneOutput()` 의 *prev-frame stale*

§3.4 의 SetSources 갱신 시점이 *stages 순회 직전* — 즉 `mRenderSys.GetLastSceneOutput()` 은 *지난 프레임* 의 PassComponent 마지막 출력. 첫 프레임 / 카메라 enable 변경 직후 1 프레임 stale 가능.

**해결 옵션** (Future):
- ScreenQuadStage 가 *stages 컬렉션 내부에서* 자기 SetSources 를 지연 결정 — SceneRenderer 의 lastSceneOutput 직접 조회 API.
- 또는 CameraStage::Render 직후 callback 으로 ScreenQuadStage 의 sources 갱신.

본 SP 에서 *건드리지 않음* — 현재 동작 (이미 prev-frame stale 였음) 그대로 보존.

### 7.2 main.cpp 의 ScreenQuadStage 추상 누수 — 채택안: raw 포인터 캐싱

stages 컬렉션은 IRenderStage 추상만 알아야 하나, main.cpp 가 SetSources 호출을 위해 *ScreenQuadStage 구체 타입* 을 알아야 함.

**채택 (§3.4 반영)**: `SJH::ScreenQuadStage* mScreenQuadStagePtr = nullptr` 비소유 raw 멤버. unique_ptr 의 `.get()` 으로 캐싱. dynamic_cast 회피 + 명시성.

**대안 (Future)**: ScreenQuadStage 가 자기 sources 를 *자기 책임* 으로 폴링 (SceneRenderer 의존성 부여) — 더 깊은 리팩토링, 본 SP 범위 밖.

### 7.3 ImGui 의 IRenderStage 화

ImGui Render 도 *stages 의 한 요소* 로 표현 가능 (`class ImGuiStage : public IRenderStage`). 단 현재 ImGui 가 *backbuffer 그대로 그려서 PostFX 미적용* 이 의도된 설계 (Editor UI 정통). 별도 SP 후보.

### 7.4 migrate_demo / audio_demo 마이그레이션

`[[deprecated]]` 경고가 발생하나 폐기 일정 미정. 본 SP 의 *다음 다음* SP 후보 — *Render(rt) 완전 폐기* 시 두 데모 일괄 마이그레이션.

---

## 8. 다음 SP 인계 프롬프트 — ParticleStage (Effekseer→sceneFB 통합)

> **본 SP 의 구현이 끝나고 시각 회귀 확인된 후, 별도 Claude Agent 가 이어받을 task 의 prompt. 아래 내용을 그대로 복사해 새 Claude session 에 전달**.

---

```
[ROLE]
OpenGL Computer Graphics 프로젝트 (C++17 CMake) 의 `_MyApp_` 탑다운 슈터 게임의 렌더링 전문 Claude Agent. 4-엔진 정통 (Unity/Unreal/Godot/Cocos) 의 파티클-PostFX 흐름을 SJH 엔진에 정착시키는 작업.

[TASK]
Effekseer 파티클 렌더를 SJH PostFX 체인 안으로 통합 — 현재 `apps/_MyApp_/main.cpp` 의 backbuffer 직접 Draw 를 폐기하고, sceneFB 에 그려 PassComponent (blur/gamma/invert/sharpening/sobel) 가 적용되게.

4-엔진 정통 = 파티클은 *씬 단계 후, PostFX 전*. 새 `ParticleStage : IRenderStage` 를 도입하여 stages 컬렉션의 *WorldCamera 와 ScreenCamera 사이* 에 삽입.

[CONTEXT 선행 결정]
선행 SP (SP-RenderStage 완성) 가 완료된 상태. main.cpp 는 이미 `std::vector<std::unique_ptr<SJH::IRenderStage>> mStages` 컬렉션 + `for (auto& s : mStages) s->Render(*mDefaultTarget);` 순회 패턴 채택. CameraStage 가 단일 카메라를 IRenderStage 로 wrap.

현 stages 순서:
  [0] CameraStage(worldCam)    → sceneFB 에 WorldMesh
  [1] CameraStage(screenCam)   → sceneFB 의 PassComponent 체인 실행
  [2] ScreenQuadStage          → backbuffer 합성
  (이후 stages 밖) VFX.Draw → backbuffer 직접 ← 폐기 대상
  (이후 stages 밖) ImGui::Render → backbuffer

목표 stages 순서:
  [0] CameraStage(worldCam)    → sceneFB 에 WorldMesh
  [1] ParticleStage(vfx, worldCam, sceneFB) → sceneFB 에 Effekseer 합성 (NEW)
  [2] CameraStage(screenCam)   → sceneFB 의 PassComponent 체인 (Effekseer 합성 결과 입력으로 받음)
  [3] ScreenQuadStage          → backbuffer 합성
  (이후 stages 밖) ImGui::Render

[FIRST READ — 반드시 순서대로]
1. `.claude/CLAUDE.md`
2. `<.claude>/Anti-HallucinationBehavioralCalibrationSystem.md`
3. `doc/superpowers/specs/2026-05-27-sp-renderstage-camera-stage-design.md` (선행 SP — 완료 가정)
4. `doc/design/PostFX.md` (ordered pass list 정통)
5. `doc/handoffs/2026-05-27/2026-05-27-pass-component-2camera-session.md` (2-Camera 패턴)
6. `doc/EngineAPI.md` §3.10 (SceneRenderer / IRenderStage)
7. `apps/_MyApp_/src/VFX/VFXSystem.{h,cpp}` (현 Effekseer Manager/Renderer owner)
8. `apps/_MyApp_/src/VFX/EffekseerPlayable.{h,cpp}` (trigger 추상)
9. `apps/_MyApp_/main.cpp` (현 통합 상태 — 특히 VFX.Draw 위치)

[WORK SCOPE — 구체]
1. 새 파일 `apps/_MyApp_/src/VFX/ParticleStage.{h,cpp}` (Client 거주 — leaf Playable 정책 부합. Engine 코어로 올리면 game_deps PUBLIC 합류 강제).
2. ParticleStage ctor 시그니처 후보:
     ParticleStage(VFXSystem* vfx, Scene::Camera* worldCam, Framebuffer* sceneFB)
   - Render(RenderTarget&) override:
     a. DeviceContext::Get().BindTarget(*mSceneFB)   ← sceneFB 강제 바인딩
     b. mWorldCam->GetView/ProjMatrix() → memcpy → Effekseer::Matrix44
     c. mVFX->Draw(...)
   - sceneFB 의 NoClear (clear 안 함 — WorldCamera 가 이미 그린 결과 보존)
3. main.cpp 변경:
   - mStages 의 worldCam stage 와 screenCam stage 사이에 ParticleStage insert
   - 기존 main.cpp:327-333 의 backbuffer VFX.Draw 라인 *삭제*
4. VAO/EBO 가드 점검 — 메모리 `vao_ebo_thirdparty_corruption` 패턴. ParticleStage Render 후 ScreenCamera stage 의 첫 PassComponent 가 자기 VAO/EBO 재바인딩하므로 *자동 가드*. 별도 코드 불필요. 단 의심되는 경우 ParticleStage Render 시작에 BindVAO(0) 한 줄로 명시 보호 가능.

[브레인스토밍 결정 필요]
1. ParticleStage 의 view 매트릭스 — WorldCamera 만 보내면 Effekseer 가 *씬 좌표계* 로 그림 (정상). ScreenCamera 의 ortho 매트릭스를 보내면 *씬 카메라 무시* — 사용 금지.
2. sceneFB 바인딩 책임 — ParticleStage 안 vs main.cpp 외부. ParticleStage 안이 정통 (자기 RT 자기 책임).
3. SceneRenderer 와의 관계 — ParticleStage 는 *SceneRenderer 의존성 0* (직접 DeviceContext + VFXSystem 만). ParticleStage 는 Client 거주이고 SceneRenderer 는 Engine 코어 — 의존 0 이 깔끔.

[검증]
- _MyApp_ 실행 — distortion 파티클이 *PostFX 적용된 모습* (gamma 0.5 시 어두워지고, sobel 시 윤곽선 검출됨) 확인.
- 기존 PostFX 토글 매트릭스 확인 (각 PostFX 단계 on/off 시 파티클도 일관 처리)
- ImGui 위치 그대로 (backbuffer 마지막).

[금기]
- SceneRenderer 안에 ParticleStage 흡수 — 책임 분리 위반. ParticleStage 는 *Application 의 stages 컬렉션 element*.
- VFXSystem 클래스 시그니처 변경 — Draw(view, proj) 그대로 사용. RT 바인딩은 ParticleStage 책임.
- migrate_demo / audio_demo 변경 — 본 task 영향 0.

[브레인스토밍 + spec + plan + 구현 흐름]
brainstorming skill → <doc>/superpowers/specs/2026-05-XX-particle-stage-design.md → writing-plans → 구현.
```

---

**spec 끝**.
