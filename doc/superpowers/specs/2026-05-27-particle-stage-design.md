# ParticleStage 도입 — Effekseer 파티클의 PostFX 체인 통합

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **작성일**: 2026-05-27
> **선행 SP**: [`2026-05-27-sp-renderstage-camera-stage-design.md`](2026-05-27-sp-renderstage-camera-stage-design.md) (CameraStage 정착 완료 — `mStages` 컬렉션 + `IRenderStage` 추상)
> **관련 문서**: [`doc/design/PostFX.md`](../../../doc/design/PostFX.md) (ordered pass list 정통), [`doc/handoffs/2026-05-27/2026-05-27-pass-component-2camera-session.md`](../../../doc/design/2026-05-27-pass-component-2camera-session.md) (2-Camera 패턴)
> **대상 데모**: `apps/_MyApp_` 만 (migrate_demo / audio_demo 는 Effekseer 미사용 — 영향 0)
> **다음 SP**: 미정 — ImGui PostFX 통합 또는 다중 view (분할 화면) 후보

---

## 1. 동기 — 왜 이 SP 가 필요한가

### 1.1 직접 동기 — 파티클의 PostFX 미적용 문제

현재 [`apps/_MyApp_/main.cpp:342-348`](../../../apps/_MyApp_/main.cpp#L342-L348) 의 `VFX.Draw(view, proj)` 가 **stages 순회 종료 후 backbuffer 에 직접 그려짐** — PassComponent 체인 (blur/gamma/invert/sharpening/sobel) 이 *이미 ScreenQuadStage 로 backbuffer 합성을 완료한 뒤* 의 시점이라 **Effekseer 파티클은 PostFX 미적용**.

증상:
- gamma 0.5 시 화면 전체는 어두워지나 *파티클만 원본 밝기*.
- sobel 시 화면 전체는 윤곽선 검출되나 *파티클은 그대로*.
- invert 시 화면 색은 반전되나 *파티클 색은 그대로*.

### 1.2 4-엔진 정통

Unity Particle System / Unreal Niagara / Godot GPUParticles3D / Cocos ParticleSystem3D 모두 **파티클을 씬 단계 후, PostFX 전** 에 그려 PostFX 체인이 파티클까지 처리. 본 SP 는 이 정통을 SJH 엔진에 정착.

### 1.3 선행 SP 의 seam

[`2026-05-27-sp-renderstage-camera-stage-design.md`](2026-05-27-sp-renderstage-camera-stage-design.md) 완성으로 `mStages` 컬렉션이 *명시 순서로 IRenderStage 순회* 패턴이 정착. 본 SP 는 *새 추상 도입이 아니라* 그 컬렉션에 `ParticleStage` element 1 개 insert + 기존 backbuffer Draw 삭제.

---

## 2. 핵심 결정 (확정 5종)

| ID | 결정 | 채택 옵션 | 근거 |
|----|------|----------|------|
| D-1 | ParticleStage 의 sceneFB 식별 방법 | `worldCam->GetTargetRenderTarget()` 매 프레임 동적 도출 | 진실의 원천 단일화 ([`architecture.md §11.5`](../../../.claude/architecture.md)) — sceneFB 인자 별도 명시 시 worldCam 의 RT 와 두 진실의 원천 충돌. resize 시 자동 추적. |
| D-2 | Effekseer Draw 전 GL state 명시 set 여부 | **하지 않음** — Effekseer 자체 setup 신뢰 | Effekseer 의 BeginRendering/EndRendering 이 내부 state 책임. PassComponent 가 다음 stage 에서 자기 state 명시 → 영향 0. YAGNI. |
| D-3 | ParticleStage 의 거주 위치 | Client (`apps/_MyApp_/src/VFX/`) | Engine 코어로 올리면 `SJH::render` 가 game_deps PUBLIC 합류 강제 — leaf Playable 정책 ([`m5-leaf-playables-design`](2026-05-26-m5-leaf-playables-design.md)) 부합. |
| D-4 | SceneRenderer 와의 관계 | 의존성 0 — DeviceContext + VFXSystem 만 | Engine ↔ Client 책임 분리. ParticleStage 가 SceneRenderer 의 internal hook 이 되면 책임 누수 + Client 가 Engine 내부 흐름 강제. |
| D-5 | stages insert 시점 (main.cpp) | `Director::Get().Init()` *직후* 별도 insert (옵션 A) | ParticleStage ctor 가 `Director::Get().VFX()` 호출 → VFXSystem 초기화 의존. Director.Init() 위치 이동 시 Physics 등 다른 의존 회귀 위험 → 최소 변경. |

---

## 3. 변경 후 stages 흐름

### 3.1 Before / After

```
[Before]
mStages 순회:
  [0] CameraStage(worldCam)  → sceneFB clear + 3D WorldMesh
  [1] CameraStage(screenCam) → sceneFB (NoClear) + PassComponent 체인
                               [blur → gamma → invert → sharp → sobel]
                               → mLastSceneOutput = mPostFXFBs[4]
  [2] ScreenQuadStage        → backbuffer 합성 (GetLastSceneOutput())

(stages 외부 — main.cpp:342-348)
  VFX.Draw(view, proj)       → backbuffer 직접 (PostFX 미적용) ← 폐기 대상
  ImGui::Render()            → backbuffer
```

```
[After]
mStages 순회:
  [0] CameraStage(worldCam)             → sceneFB clear + 3D WorldMesh
  [1] ParticleStage(vfx, worldCam)      → sceneFB (NoClear) + Effekseer 합성  ★NEW
  [2] CameraStage(screenCam)            → sceneFB (NoClear) + PassComponent 체인
                                          (PassComponent[0].InputFB = sceneFB
                                           → Effekseer 합성 결과 자동 흡수)
  [3] ScreenQuadStage                   → backbuffer 합성

(stages 외부)
  ImGui::Render()            → backbuffer (그대로)
```

### 3.2 자동 합성 흐름

```
sceneFB 의 color 변천:
  Frame N 시작: (이전 frame 결과 — clear 대기)
  ↓ CameraStage[0] (worldCam)
  Frame N 후: sceneFB.color = WorldMesh 렌더 결과 (3D 캐릭터 + 스테이지)
  ↓ ParticleStage (NoClear)
  Frame N 후: sceneFB.color = WorldMesh + Effekseer 파티클 (alpha blend)
  ↓ CameraStage[1] (screenCam — PassComponent 체인)
  Frame N 후: mPostFXFBs[4].color = PassComponent 5-stage 적용 후 결과
  ↓ ScreenQuadStage
  Frame N 후: backbuffer.color = mPostFXFBs[4] passthrough blit
  ↓ ImGui::Render
  Frame N 후: backbuffer.color = PostFX 결과 + ImGui overlay
```

---

## 4. 변경 명세

### 4.1 파일별 변경표

| # | 파일 | 변경 | 행수 |
|---|---|---|---|
| 1 | `apps/_MyApp_/src/VFX/ParticleStage.h` *(신규)* | `class ParticleStage : public SJH::IRenderStage` 선언 — ctor + Render override + 멤버 2개 | +33 |
| 2 | `apps/_MyApp_/src/VFX/ParticleStage.cpp` *(신규)* | Render 본구현 — nullptr 가드 + `worldCam->GetTargetRenderTarget()` 도출 + `DeviceContext::BindTarget` + `VFXSystem::Draw(view, proj)` | +35 |
| 3 | [`apps/_MyApp_/CMakeLists.txt`](../../../apps/_MyApp_/CMakeLists.txt) | `ParticleStage.cpp` 등록 (`target_sources` 의 `PRIVATE` 목록 합류) | +1 |
| 4 | [`apps/_MyApp_/main.cpp`](../../../apps/_MyApp_/main.cpp) | (a) `#include "apps/_MyApp_/src/VFX/ParticleStage.h"` 추가. (b) `Director::Get().Init()` 직후 `mStages.insert(begin+1, ParticleStage)` 한 줄. (c) 기존 line 342~348 의 `VFX.Draw(view, proj)` 블록 삭제. | ±10 |

### 4.2 새 헤더 — `apps/_MyApp_/src/VFX/ParticleStage.h`

```cpp
#ifndef _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__
#define _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__

#include "<render>/render_stage.h"

namespace SJH::Scene { class Camera; }

namespace TopdownShooter::VFX
{
    class VFXSystem;

    /// @brief Effekseer 파티클을 WorldCamera 의 sceneFB 에 합성하는 렌더 stage.
    /// @details stages 컬렉션의 [worldCam, screenCam] 사이 거주.
    ///          sceneFB 는 worldCam->GetTargetRenderTarget() 으로 매 프레임 동적 도출
    ///          (진실의 원천 단일화 — architecture.md §11.5). resize 자동 추적.
    ///          GL state 는 Effekseer 내부 BeginRendering/EndRendering 이 책임.
    /// @note    Client 거주 — Engine 코어 (SJH::render) 가 game_deps PUBLIC 합류 강제 회피.
    class ParticleStage : public SJH::IRenderStage
    {
      public:
        /// @param vfx       VFXSystem (비소유). nullptr 시 Render 호출 무시.
        /// @param worldCam  Effekseer view/proj 출처 + sceneFB 출처 (비소유). nullptr 시 무시.
        ParticleStage(VFXSystem* vfx, SJH::Scene::Camera* worldCam);

        /// @brief sceneFB(=worldCam->GetTargetRenderTarget()) bind + Effekseer Draw.
        /// @note  target 인자는 사용하지 않음 — Camera 의 RT 사용 (CameraStage 와 동일 패턴).
        void Render(SJH::RenderTarget& target) override;

      private:
        VFXSystem*           mVFX      = nullptr;
        SJH::Scene::Camera*  mWorldCam = nullptr;
    };
}

#endif // _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__
```

### 4.3 새 .cpp — `apps/_MyApp_/src/VFX/ParticleStage.cpp`

```cpp
#include "apps/_MyApp_/src/VFX/ParticleStage.h"
#include "apps/_MyApp_/src/VFX/VFXSystem.h"
#include "render/device_context.h"
#include "src/buffer/render_target.h"
#include "scene/camera.h"
#include <<spdlog>/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::VFX
{
    ParticleStage::ParticleStage(VFXSystem* vfx, SJH::Scene::Camera* worldCam)
        : mVFX(vfx), mWorldCam(worldCam) {}

    void ParticleStage::Render(SJH::RenderTarget& /*target*/)
    {
        if (!mVFX || !mWorldCam)
        {
            spdlog::warn("ParticleStage::Render — vfx/worldCam nullptr — skip.");
            return;
        }

        // 진실의 원천 단일화 — worldCam 의 RT 가 sceneFB (D-1).
        auto* rt = mWorldCam->GetTargetRenderTarget();
        if (!rt)
        {
            spdlog::warn("ParticleStage::Render — worldCam.GetTargetRenderTarget() nullptr — skip.");
            return;
        }

        // sceneFB bind (NoClear — WorldCamera 가 이미 그린 결과 보존).
        SJH::DeviceContext::Get().BindTarget(*rt);

        // Effekseer 자체 GL state setup + Draw (D-2 — state 명시 set 하지 않음).
        const vmath::mat4 view = mWorldCam->GetViewMatrix();
        const vmath::mat4 proj = mWorldCam->GetProjectionMatrix();
        mVFX->Draw(&view[0][0], &proj[0][0]);
    }
}
```

### 4.4 `apps/_MyApp_/CMakeLists.txt` 변경

기존 `target_sources(..._MyApp_ PRIVATE ...)` 블록의 VFX 섹션에 `ParticleStage.cpp` 등록:

```cmake
target_sources(_MyApp_ PRIVATE
    # ... 기존 항목
    apps/_MyApp_/src/VFX/VFXSystem.cpp
    apps/_MyApp_/src/VFX/EffekseerPlayable.cpp
    apps/_MyApp_/src/VFX/ParticleStage.cpp          # NEW
    # ...
)
```

### 4.5 `apps/_MyApp_/main.cpp` 변경 골격

```cpp
// (a) include 추가 (상단)
#include "apps/_MyApp_/src/VFX/ParticleStage.h"

// (b) startup() 의 카메라 stages insert 직후 (기존 line 208~219 의 마지막)
//     기존 코드 그대로 유지 — 카메라 stages 2개만 insert
mStages.insert(mStages.begin(), std::make_unique<SJH::CameraStage>(&mRenderSys, mScreenCam));
mStages.insert(mStages.begin(), std::make_unique<SJH::CameraStage>(&mRenderSys, mCamera));

// === M5 — Director Init === (기존 line 222)
TopdownShooter::Director::Get().Init();
auto &phys = TopdownShooter::Director::Get().Physics();

// (b-2) ParticleStage insert — Director.Init() 직후 (D-5).
//       위치: stages.begin() + 1 (worldCam 뒤, screenCam 앞)
//       최종 stages: [worldCam, ParticleStage, screenCam, ScreenQuadStage]
mStages.insert(
    mStages.begin() + 1,
    std::make_unique<TopdownShooter::VFX::ParticleStage>(
        &TopdownShooter::Director::Get().VFX(), mCamera));

// ... (기존 startup 나머지 로직 — 그대로)

// (c) render() 의 기존 line 342~348 — 전부 삭제 (ParticleStage 가 stages 순회에서 처리)
// 삭제 대상:
//   // === M5 — Effekseer 렌더 (backbuffer 합성 후, swap 전) ===
//   if (mCamera)
//   {
//       vmath::mat4 view = mCamera->GetViewMatrix();
//       vmath::mat4 proj = mCamera->GetProjectionMatrix();
//       TopdownShooter::Director::Get().VFX().Draw(&view[0][0], &proj[0][0]);
//   }
```

---

## 5. ddd Rules 정합성

| Rule | 평가 | 비고 |
|------|------|------|
| **Separation of Concerns** | ✅ | ParticleStage = "Effekseer 를 sceneFB 에 합성" 단일 책임. VFXSystem (Manager/Renderer owner) 과 분리. |
| **Domain-Specific Naming** | ✅ | `ParticleStage` — 4-엔진 정통 명명 (Unity ParticleSystemRenderer / Unreal NiagaraSystemRenderer 대비 stage 용어). |
| **Explicit Side Effects** | ✅ | Render() 가 하는 일이 헤더 doxygen 에 명시 — BindTarget + Effekseer Draw. |
| **Explicit Control Flow** | ✅ | nullptr 가드 → RT 도출 → bind → draw 단순 직선 흐름. early return 패턴. |
| **Library-First** | ✅ | Effekseer + spdlog 활용. 자체 진단/로깅 0. |
| **Function/File Size Limits** | ✅ | 헤더 ~33 줄, cpp ~35 줄. 함수 1 개 (Render) ~15 줄. |
| **Early Return Pattern** | ✅ | nullptr 가드 2 개 모두 early return + warn. |
| **POLA (Principle of Least Astonishment)** | ✅ | CameraStage 와 *동일 시그니처 패턴* — `Render(RenderTarget&)` target 인자 무시 + 내부 RT 사용. 읽는 사람이 "어디 그리지?" 라 묻지 않음 (worldCam->GetTargetRenderTarget()). |
| **Single Source of Truth** | ✅ | D-1 — sceneFB 는 worldCam 의 RT 가 *유일한 진실*. ctor 인자로 별도 받지 않음. |
| **Call-Site Honesty** | ✅ | nullptr 가드 시 spdlog warn 출력 — 호출자가 "왜 안 그려지지?" 디버깅 가능. |

---

## 6. 손대지 않는 것 (Out of Scope)

| 항목 | 이유 |
|---|---|
| **VFXSystem 시그니처 변경** | `Draw(view, proj)` 그대로 사용. RT 바인딩은 ParticleStage 책임. |
| **EffekseerPlayable 변경** | Component 라이프사이클 (spawn/track/finished) 무관. Effekseer Manager 가 보유한 모든 effect 를 ParticleStage 가 한 번에 그림 — 둘은 서로 모름. |
| **migrate_demo / audio_demo** | 둘 다 Effekseer 미사용. 영향 0. |
| **ImGui 위치 이동** | `ImGui::Render()` 가 backbuffer 마지막 — 의도된 설계 (Editor UI 정통, PostFX 미적용). 별도 SP 후보. |
| **SceneRenderer 흡수** | D-4 — ParticleStage 는 Client 거주, SceneRenderer 는 Engine 코어. Engine 의 game_deps PUBLIC 합류 강제 회피. |
| **GL state push/pop 가드** | D-2 — Effekseer 자체 BeginRendering/EndRendering 책임. PassComponent 가 다음 stage 에서 자기 state 명시 set. |
| **VAO/EBO 명시 보호** | `mesh_pass_processor.cpp:228` 의 `ebo->Bind()` 재핀이 자동 가드 (메모리: `vao_ebo_thirdparty_corruption`). |
| **단위 테스트 신규 추가** | 사용자 정책 `no_auto_tests`. 시각 회귀로 검증. |

---

## 7. 검증

### 7.1 시각 회귀 시나리오

| # | 시나리오 | 기대 결과 |
|---|---|---|
| V1 | `_MyApp_` 빌드 + 실행 | 컴파일 + 런타임 정상, spdlog warn 0 (정상 경로) |
| V2 | 좌클릭 → `EffekseerPlayable` 발사 (distortion 파티클) | 파티클이 sceneFB 에 그려져 화면 표시 |
| V3 | gamma 슬라이더 0.5 | 파티클 포함 화면 전체 어두워짐 (변경 전: 파티클은 원본 밝기) |
| V4 | sobel 토글 ON | 파티클도 윤곽선 검출됨 (변경 전: 파티클은 sobel 미적용) |
| V5 | invert 토글 ON | 파티클 색도 반전 (변경 전: 파티클 색 그대로) |
| V6 | blur 토글 ON | 파티클도 블러 처리 |
| V7 | sharpening 토글 ON | 파티클도 샤프닝 처리 |
| V8 | 전체 PostFX OFF (모든 토글 OFF) | 기존과 시각적으로 동일 (sceneFB → ScreenQuadStage 직접 blit) |
| V9 | ImGui 패널 위치 | backbuffer 최상위 (변경 전과 동일, PostFX 미적용) |
| V10 | 창 리사이즈 (드래그) | sceneFB 재생성 → worldCam->SetTargetRenderTarget 갱신 → ParticleStage 가 매 프레임 동적 조회 → 자동 추적 (D-1) |

### 7.2 런타임 경고 로그

| 시나리오 | spdlog 출력 |
|---|---|
| 정상 경로 | (출력 없음) |
| Director.Init() 누락 | `ParticleStage::Render — vfx/worldCam nullptr — skip.` |
| WorldCamera 가 RT 없음 | `ParticleStage::Render — worldCam.GetTargetRenderTarget() nullptr — skip.` |

### 7.3 컴파일/단위 테스트

- `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` — Debug 빌드 성공.
- `cmake --build --preset ninja-release --target _MyApp_` — Release 빌드 성공.
- `migrate_demo` / `audio_demo` — 영향 0, 기존 빌드 결과 동일.

---

## 8. 정통 매핑

| 결정 | Unity | Unreal | Godot | Cocos |
|---|---|---|---|---|
| 파티클의 PostFX 통합 위치 | URP `Render Pass Event = After Rendering Transparents` | Niagara → `BeforePostProcess` blendable | GPUParticles3D → CompositorEffect 자동 흡수 | ParticleSystem3D → `RenderPipeline` 단계 |
| sceneFB 공유 | URP `cameraColorTarget` | `FSceneRenderTargets::GetSceneColor` | `Viewport.render_target_clear_mode = Never` | RenderTexture 공유 |
| Stage 추상 | `ScriptableRenderPass` | `FSceneViewExtension` | `CompositorEffect` | RenderPipelinePass |

---

## 9. 알려진 이슈 / Future Work

### 9.1 Effekseer 가 sceneFB 의 depth 와 occlusion 가능 여부

기존엔 Effekseer 가 backbuffer (depth 없음) 에 그려 occlusion 무관. 새로 sceneFB (depth attachment 보유 가정) 에 그리면 *3D 캐릭터 뒤의 파티클이 가려질 수 있음*.

**검증 시점**: V2 단계 — 의도된 동작이면 보존, 의도 안 됨이면 별도 SP 로 Effekseer 의 depth test off 옵션 도입.

확인 필요: `SJH::Framebuffer::Create(w, h)` 가 depth attachment 자동 생성하는지. (시각 회귀 시 확인 → 별도 spec 으로 정리)

### 9.2 ImGui 의 PostFX 통합

본 SP 영향 0. Editor UI 가 PostFX 미적용이 정통. 게임 UI (HUD) 는 이미 ScreenCamera CullingMask = `Layer::UI | Layer::Screen` 를 통해 PostFX 적용 흐름 안에 들어와 있음.

### 9.3 다중 view (분할 화면 / 미니맵)

미니맵 추가 시 *카메라 N 개 각각 sceneFB 보유 + 공유 PassComponent 체인* 패턴. ParticleStage 는 카메라당 1 개 필요할 수 있음 (or 통합 — 카메라별 view/proj 다중 호출). 별도 SP.

### 9.4 파티클 백엔드 추상화

Effekseer 외 Unity Particle System 포팅 등 백엔드 다중화 시 `IParticleProvider` 인터페이스 도입. 현재 YAGNI.

---

## 10. 결정 로그 요약

| ID | 결정 | 채택 | 영향 |
|----|------|-----|------|
| D-1 | sceneFB 식별 | worldCam->GetTargetRenderTarget() 동적 도출 | 진실의 원천 단일화, resize 자동 추적 |
| D-2 | GL state 명시 set | 하지 않음 (Effekseer 자체 신뢰) | YAGNI, ~3 줄 절약 |
| D-3 | ParticleStage 거주 | Client (`apps/_MyApp_/src/VFX/`) | Engine 코어 game_deps 합류 회피 |
| D-4 | SceneRenderer 의존 | 0 (DeviceContext + VFXSystem 만) | Engine ↔ Client 책임 분리 |
| D-5 | stages insert 시점 | Director.Init() 직후 별도 insert | 최소 변경, Director 초기화 흐름 보존 |

---

## 11. 후속 작업

- `writing-plans` skill 진입 — 본 spec 기반 implementation plan 작성 (`doc/superpowers/plans/2026-05-27-particle-stage-impl.md`).
- Plan 단위 Task 4 개 예상: T1 (ParticleStage.h/.cpp 신규) → T2 (CMakeLists.txt 등록) → T3 (main.cpp 변경 — include + insert + Draw 삭제) → T4 (시각 회귀 검증).

---

**spec 끝**.
