# SP-RenderStage 완성 — CameraStage 정착 Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `_MyApp_` 데모의 렌더 흐름을 `IRenderStage` stages 컬렉션 순회로 전환. 단일 `CameraStage` 가 World/Screen 카메라를 wrap. `ScreenQuadStage` 도 stages 가 흡수. 시각 결과 100% 동일.

**Architecture:** 단일 `CameraStage : public IRenderStage` 신규. `SceneRenderer::RenderWithCamera` private→public 노출. `Render(rt)` 자동 순회 path 는 `[[deprecated]]` 로 유지 (migrate_demo/audio_demo 호환). main.cpp 가 `std::vector<std::unique_ptr<IRenderStage>>` 컬렉션을 명시 순서로 순회.

**Tech Stack:** C++17, CMake (ninja preset), GCC/Clang debug `-Werror`, GLFW + sb7. 단위 테스트 작성 금지 (사용자 정책 메모리 `no_auto_tests`). 검증은 빌드 0-error + 시각 회귀 (수동).

**Spec:** [`doc/superpowers/specs/2026-05-27-sp-renderstage-camera-stage-design.md`](../specs/2026-05-27-sp-renderstage-camera-stage-design.md)

---

## File Structure

| 파일 | 책임 | 신규/수정 |
|---|---|---|
| `cmake/CXXStandard.cmake` | `-Wno-error=deprecated-declarations` 추가 (Task 1 의 `[[deprecated]]` 가 `-Werror` 로 막히지 않게) | 수정 (+1줄) |
| `<src>/render/scene_renderer.h` | `RenderWithCamera` private → public. `Render(rt)` 위에 `[[deprecated]]` attribute | 수정 (±2줄) |
| `<src>/render/camera_stage.h` | `CameraStage : IRenderStage` 선언 — 단일 카메라 wrap, SceneRenderer 위임 | 신규 |
| `<src>/render/camera_stage.cpp` | `CameraStage::Render` 구현 = `mRenderer->RenderWithCamera(*mCamera)` 위임 + nullptr 가드 | 신규 |
| `src/render/CMakeLists.txt` | `camera_stage.cpp` 를 `sjhopengl_render` STATIC 라이브러리에 등록 | 수정 (+1줄) |
| `apps/_MyApp_/main.cpp` | `mStages` 컬렉션 + `mScreenQuadStagePtr` 비소유 raw 멤버. startup 에서 stages 3개 등록. render() 의 두 줄을 stages loop 로 교체 | 수정 (±25줄) |

Task 순서는 **빌드 가능한 단위**:
1. cmake 옵션 완화 (Task 1 의 `[[deprecated]]` 를 위한 사전)
2. scene_renderer.h 변경 (다른 데모는 경고만 발생)
3. camera_stage.{h,cpp} + CMakeLists (라이브러리 자체는 빌드 OK)
4. main.cpp 전환 (시각 동일)
5. 사용자 시각 회귀 보고

---

## Task 0: `-Wno-error=deprecated-declarations` 사전 완화

**왜 먼저인가**: Task 1 에서 `Render(rt)` 에 `[[deprecated]]` 를 붙이면, migrate_demo / audio_demo 의 `Render(rt)` 호출이 *경고* 를 발생시킨다. 현 CXXStandard.cmake 는 Debug 시 `-Werror` 가 활성이므로 *경고 → 에러* 가 되어 빌드 실패. 이미 정착된 예외 패턴 (`-Wno-error=unused-but-set-variable` 등) 과 동일 줄 추가.

**Files:**
- Modify: `cmake/CXXStandard.cmake:20` 근처 (기존 예외 직후)

- [ ] **Step 1: `cmake/CXXStandard.cmake` 열어 기존 예외 줄 확인**

기존 패턴 (line 14-21 영역):
```cmake
$<$<CONFIG:Debug>:-Wno-error=unused-but-set-variable>
# ...
$<$<CONFIG:Debug>:-Wno-error=pessimizing-move>
"$<$<CONFIG:Debug>:-Wno-error=#warnings>"
```

- [ ] **Step 2: `-Wno-error=deprecated-declarations` 줄 추가**

`-Wno-error=pessimizing-move` 줄 *직후* (line 20 다음) 에 한 줄 삽입:
```cmake
        $<$<CONFIG:Debug>:-Wno-error=deprecated-declarations>
```

(들여쓰기 = 8 칸 — 기존 패턴 그대로. tab vs space 는 파일 기존 스타일 따름.)

- [ ] **Step 3: 빌드 검증 — 현 상태에서 영향 0**

```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 기존과 동일하게 성공. 새 플래그는 *경고를 에러로 만들지 않을 뿐* 이라 현재 deprecated 경고가 없으면 동작 변화 0.

- [ ] **Step 4: Commit**

```bash
git add cmake/CXXStandard.cmake
git commit -m "$(cat <<'EOF'
[build] -Wno-error=deprecated-declarations 추가 — Debug -Werror 예외

SP-RenderStage 의 Render(rt) [[deprecated]] attribute 도입 사전 작업.
migrate_demo / audio_demo 가 점진적 마이그레이션될 수 있도록 경고만 발생시키고
빌드 실패는 방지.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 1: `scene_renderer.h` — `RenderWithCamera` public + `Render(rt)` deprecated

**Files:**
- Modify: `<src>/render/scene_renderer.h` (현재 +29~+49 영역)

- [ ] **Step 1: `Render(rt)` 위에 `[[deprecated]]` attribute 추가**

현재 (line 27-29):
```cpp
        /// @brief SceneContext 의 Camera 컬렉션을 순회하며 직렬 렌더 후 PostFX 체인 실행.
        /// @param defaultTarget IRenderStage 인터페이스 준수용 — 내부에서는 Camera 및 PostFX 전용 RT 사용.
        void Render(RenderTarget& defaultTarget) override;
```

다음으로 교체:
```cpp
        /// @brief SceneContext 의 Camera 컬렉션을 순회하며 직렬 렌더 후 PostFX 체인 실행.
        /// @param defaultTarget IRenderStage 인터페이스 준수용 — 내부에서는 Camera 및 PostFX 전용 RT 사용.
        /// @deprecated Use CameraStage + IRenderStage stages 컬렉션 — Application 이 출차 책임 (4-엔진 정통).
        [[deprecated("Use CameraStage + IRenderStage stages 컬렉션 — Application 이 출차 책임 (4-엔진 정통)")]]
        void Render(RenderTarget& defaultTarget) override;
```

- [ ] **Step 2: `RenderWithCamera` 를 private → public 으로 이동**

현재 (line 41-43):
```cpp
    private:
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat, uint64_t cullingMask);
        void RenderWithCamera(Scene::Camera& cam);
```

다음으로 교체 (public 섹션 안 — `GetLastSceneOutput()` 뒤, `private:` 앞):
```cpp
        /// @brief 명시된 단일 Camera 에 대해 1패스 렌더 — CameraStage 가 위임 호출.
        /// @details SceneContext.GetCameras() 자동 순회를 우회하는 외부 진입점.
        ///          Camera::GetTargetRenderTarget() nullptr 이면 warn+skip.
        void RenderWithCamera(Scene::Camera& cam);

    private:
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat, uint64_t cullingMask);
```

(즉 `RenderWithCamera` 선언 행을 private 섹션에서 *삭제* 하고 public 섹션에 *추가*.)

- [ ] **Step 3: 빌드 검증 — 라이브러리 + 활성 데모 모두 빌드**

```bash
cmake --build --preset ninja --target _MyApp_
```

Expected:
- _MyApp_ 빌드 성공. main.cpp:318 의 `mRenderSys.Render(*mDefaultTarget);` 가 deprecated 경고 발생하나 -Werror=deprecated-declarations 제외로 *경고만*.
- migrate_demo / audio_demo 도 같은 경고 발생 가능 (이 시점에는 미빌드).

확인 명령 (전체 데모 한 번 검증):
```bash
cmake --build --preset ninja --target _MyApp_ migrate_demo audio_demo
```

Expected: 3 데모 모두 빌드 성공 + `warning: 'Render' is deprecated: Use CameraStage...` 경고 발생.

- [ ] **Step 4: Commit**

```bash
git add <src>/render/scene_renderer.h
git commit -m "$(cat <<'EOF'
[refactor] SceneRenderer: RenderWithCamera public + Render(rt) [[deprecated]]

SP-RenderStage 완성 Task 1 — CameraStage 가 RenderWithCamera 를 위임 호출할 수
있도록 public 노출. Render(rt) 자동 순회 path 는 호환을 위해 유지하나 deprecated
attribute 로 점진 마이그레이션 유도. migrate_demo / audio_demo 는 경고만 발생.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: `CameraStage` 신규 + CMakeLists 등록

**Files:**
- Create: `<src>/render/camera_stage.h`
- Create: `<src>/render/camera_stage.cpp`
- Modify: `src/render/CMakeLists.txt`

- [ ] **Step 1: `<src>/render/camera_stage.h` 작성**

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
        /// @param renderer 위임 대상 (비소유). nullptr 시 Render 호출 무시.
        /// @param camera   렌더할 카메라 (비소유). nullptr 시 Render 호출 무시.
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

- [ ] **Step 2: `<src>/render/camera_stage.cpp` 작성**

```cpp
#include "<render>/camera_stage.h"
#include "<render>/scene_renderer.h"
#include "scene/camera.h"
#include <<spdlog>/spdlog.h>

namespace SJH
{
    CameraStage::CameraStage(SceneRenderer* renderer, Scene::Camera* camera)
        : mRenderer(renderer), mCamera(camera)
    {
    }

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

- [ ] **Step 3: `src/render/CMakeLists.txt` 에 `camera_stage.cpp` 등록**

현재 (line 1-10 영역):
```cmake
add_library(sjhopengl_render STATIC
    device_context.cpp
    render_target.cpp           # vtable home TU — virtual class 의 ODR 보장 (header-only 만으로 부족)
    render_stage.cpp            # vtable home TU — IRenderStage virtual class (SP5)
    mesh_pass_processor.cpp
    pipeline_state_setter.cpp   # SP-PipelineSetter — GL state machine 분리 (Applier 패턴)
    property_block_setter.cpp   # SP-PropertyBlockSetter — 옛 MaterialApplier rename + 시그니처 정밀화
    scene_renderer.cpp
    light_uniform_dispatcher.cpp  # Phase 2 — SendLightUniforms SceneRenderer 에서 분리
    screen_quad_stage.cpp         # SP-UniversalRenderTarget Phase A — N FBO ->backbuffer 합성
)
```

`screen_quad_stage.cpp` 줄 *직후* 한 줄 추가:
```cmake
    camera_stage.cpp              # SP-RenderStage 완성 — 단일 Camera 를 IRenderStage 로 wrap
```

(들여쓰기 = 4 spaces — 기존 패턴 그대로.)

- [ ] **Step 4: 빌드 검증 — 라이브러리 + 데모 모두**

```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 성공. CameraStage 가 아직 main.cpp 에서 사용되지 않지만 *unused class 경고는 발생 안 함* (클래스 정의는 unused 가 아님).

- [ ] **Step 5: Commit**

```bash
git add <src>/render/camera_stage.h <src>/render/camera_stage.cpp src/render/CMakeLists.txt
git commit -m "$(cat <<'EOF'
[feat] CameraStage : IRenderStage 도입 — 단일 카메라 wrap

SP-RenderStage 완성 Task 2 — Application 이 카메라 명시 순서를 결정할 수 있도록
단일 카메라를 IRenderStage 로 wrap. SceneRenderer::RenderWithCamera 위임만 수행.
nullptr 가드 + spdlog warn. 본 task 에서는 도입만 — 다음 task 가 main.cpp 에서 사용.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: `apps/_MyApp_/main.cpp` — stages 컬렉션 패턴 전환

**Files:**
- Modify: `apps/_MyApp_/main.cpp` — include, 멤버, startup, render, shutdown 5 영역

- [ ] **Step 1: include 추가**

main.cpp 의 SJH render 관련 include 영역 (대략 `#include "<render>/scene_renderer.h"` 같은 줄들 근처) 에 다음 두 줄 추가 (이미 있으면 skip):
```cpp
#include "<render>/camera_stage.h"
#include <memory>
#include <vector>
```

확인:
```bash
grep -n "camera_stage.h\|<memory>\|<vector>" apps/_MyApp_/main.cpp
```

`<memory>` / `<vector>` 가 이미 다른 경로로 포함됐을 수 있음. include 가 *중복되지 않게* — 이미 있으면 `camera_stage.h` 만 추가.

- [ ] **Step 2: 멤버 영역 (main.cpp:458-463) 변경**

현재 (line 458-463):
```cpp
    private:
        // ── 멤버 ────────────────────────────────────────────────────────────────────
        SJH::SceneRenderer              mRenderSys;
        SJH::RenderTargetUPtr           mDefaultTarget;
        SJH::FramebufferUPtr            mSceneFB;
        std::unique_ptr<SJH::ScreenQuadStage> mScreenQuadStage;
```

다음으로 교체:
```cpp
    private:
        // ── 멤버 ────────────────────────────────────────────────────────────────────
        SJH::SceneRenderer              mRenderSys;
        SJH::RenderTargetUPtr           mDefaultTarget;
        SJH::FramebufferUPtr            mSceneFB;

        // SP-RenderStage 완성 — Application 이 stages 컬렉션을 명시 순서로 순회.
        std::vector<std::unique_ptr<SJH::IRenderStage>> mStages;
        SJH::ScreenQuadStage*                           mScreenQuadStagePtr = nullptr;  // 비소유 raw — owner = mStages
```

(`mScreenQuadStage` unique_ptr 멤버 *삭제*.)

- [ ] **Step 3: startup() 안 ScreenQuadStage 생성 부분 변경**

현재 (line 118-119):
```cpp
            mScreenQuadStage = std::make_unique<SJH::ScreenQuadStage>(*passthroughProg, *quadMesh);
            mScreenQuadStage->SetSources({mSceneFB.get()});
```

다음으로 교체:
```cpp
            // ── ScreenQuadStage 생성 — 소유권 stages 컬렉션으로 이전 ──────────────
            // 이 시점에 카메라 멤버는 아직 valid 아님 (line 143 / 203 에서 대입).
            // 그래서 ScreenQuadStage 만 먼저 mStages 에 push, 카메라 stages 는 Step 4 에서
            // *앞에* insert 하여 최종 [worldCam, screenCam, ScreenQuadStage] 순서 정착.
            auto screenQuadOwned = std::make_unique<SJH::ScreenQuadStage>(*passthroughProg, *quadMesh);
            screenQuadOwned->SetSources({mSceneFB.get()});      // 초기 sources — sceneFB fallback
            mScreenQuadStagePtr = screenQuadOwned.get();        // raw 캐시 — render() 의 SetSources 갱신용
            mStages.push_back(std::move(screenQuadOwned));
```

- [ ] **Step 4: startup() — 카메라 stages 등록**

정확 위치: `main.cpp:203` 의 `mScreenCam = screenCam;` *직후*. 이 시점에 멤버 `mCamera` (line 143 대입) + `mScreenCam` (line 203 대입) 둘 다 valid.

현재 (line 202-203 영역):
```cpp
            mScreenCamActor = dir.Root().AddChild(std::move(screenCamActor));
            mScreenCam      = screenCam;
```

*직후* 에 다음 블록 추가:
```cpp

            // ── stages 컬렉션 — World → Screen → ScreenQuad 순 (SP-RenderStage) ─
            // ScreenQuadStage 는 Step 3 에서 이미 mStages 에 push 된 상태.
            // 카메라 stages 를 *ScreenQuadStage 앞* 에 insert — 최종 순서:
            //   [0] worldCam → sceneFB 에 WorldMesh
            //   [1] screenCam → PassComponent 체인 (NoClear)
            //   [2] ScreenQuadStage → backbuffer 합성
            mStages.insert(
                mStages.begin(),
                std::make_unique<SJH::CameraStage>(&mRenderSys, mScreenCam));
            mStages.insert(
                mStages.begin(),
                std::make_unique<SJH::CameraStage>(&mRenderSys, mCamera));
```

*insert 순서 주의*: `screenCam` 을 먼저 insert → `[screenCam, ScreenQuadStage]`, 그 다음 `worldCam` 을 앞에 insert → `[worldCam, screenCam, ScreenQuadStage]`. 의도된 최종 순서와 일치.

- [ ] **Step 5: render() 변경 — Render(rt) + ScreenQuadStage 두 줄을 stages loop 한 줄로**

현재 (line 318-325 영역):
```cpp
            // 씬 렌더 (SceneFB) + PostFX 체인 (intermediate FBs).
            mRenderSys.Render(*mDefaultTarget);

            // ScreenQuadStage — PassComponent 마지막 출력 또는 SceneFB fallback ->backbuffer.
            {
                auto *out = mRenderSys.GetLastSceneOutput();
                mScreenQuadStage->SetSources({out ? out : mSceneFB.get()});
                mScreenQuadStage->Render(*mDefaultTarget);
            }
```

다음으로 교체:
```cpp
            // ── stages 컬렉션 순회 — World → Screen → ScreenQuad ─────────────────
            // ScreenQuadStage 의 sources 는 *stages 순회 직전* 갱신 (지난 프레임 PassComponent 출력).
            {
                auto *out = mRenderSys.GetLastSceneOutput();
                mScreenQuadStagePtr->SetSources({out ? out : mSceneFB.get()});
            }
            for (auto& s : mStages)
                s->Render(*mDefaultTarget);
```

- [ ] **Step 6: shutdown() 안 mScreenQuadStage 참조 제거 (있을 시)**

확인:
```bash
grep -n "mScreenQuadStage" apps/_MyApp_/main.cpp
```

Expected output: 매치 0건 (모두 `mScreenQuadStagePtr` 또는 `mStages` 로 전환됨). 만약 `mScreenQuadStage->...` 또는 `mScreenQuadStage.reset()` 등 잔존하면 삭제 또는 `mStages.clear()` 로 대체.

shutdown() 안 (line 340-356 영역) 에서 `mDefaultTarget.reset()` 직전에 `mStages.clear()` 한 줄 추가 권장 (RAII 자동 정리되나 명시):
```cpp
            mStages.clear();
            mDefaultTarget.reset();
```

- [ ] **Step 7: 빌드 검증**

```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 성공. 본 task 후 `_MyApp_` 가 stages 컬렉션 패턴 사용 — `mRenderSys.Render(...)` 호출이 *_MyApp_ 에서는* 사라짐 (deprecated 경고 0).

migrate_demo / audio_demo 는 여전히 `Render(rt)` 호출 → deprecated 경고만.

- [ ] **Step 8: Commit**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "$(cat <<'EOF'
[refactor] _MyApp_: stages 컬렉션 패턴 전환 — CameraStage + ScreenQuadStage

SP-RenderStage 완성 Task 3 — Application 이 World/Screen 카메라 + ScreenQuadStage
를 std::vector<unique_ptr<IRenderStage>> 로 명시 순서 순회. mScreenQuadStage
unique_ptr 멤버를 mStages 에 흡수, raw 캐시는 SetSources 호출용.

시각 결과 = 변경 전과 100% 동일 (단순 path 재배열). Effekseer / ImGui /
migrate_demo / audio_demo 영향 0.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: 시각 회귀 보고 (사용자 수동)

**왜 자동화 안 하나**: 사용자 정책 메모리 `no_auto_tests` — 단위 테스트 자동 작성 금지. 시각 회귀는 사람 눈 검증.

- [ ] **Step 1: 빌드 + 실행**

```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

- [ ] **Step 2: 시각 회귀 체크리스트 (사용자)**

다음 항목들이 변경 *전* 과 동일하게 동작하는지 확인:

1. **씬 렌더링** — 캐릭터 sprite + 배경 정상 표시
2. **WASD 이동** — 캐릭터 4 방향 이동, dt 기반 등속
3. **카메라 follow** — 캐릭터 추적
4. **좌클릭 발사** — distortion 파티클 (Effekseer) + Fmod Laser + FmodStudio Slash 시퀀스 (※ 본 SP 는 Effekseer 위치를 *변경 안 함* — backbuffer 직접 그대로)
5. **G 키** — TweenShake 로그 + FmodStudio Damaged 사운드
6. **F1** — ImGui 토글
7. **ImGui PostFXDebugLayer** — gamma / invert / blur / sobel / sharpening 각각 on/off — 기존과 동일 시각 효과
8. **창 리사이즈** — viewport 갱신 (Retina HiDPI 호환)

- [ ] **Step 3: migrate_demo / audio_demo 빌드 회귀**

```bash
cmake --build --preset ninja --target migrate_demo audio_demo
```

Expected: 빌드 성공 + `warning: 'Render' is deprecated: Use CameraStage...` 경고 발생 (에러 아님).

- [ ] **Step 4: 사용자 보고**

시각 회귀 OK + 컴파일 경고 매트릭스 확인되면 SP 완료. 변경된 5 파일 (cmake/CXXStandard.cmake / scene_renderer.h / camera_stage.{h,cpp} / CMakeLists.txt / main.cpp) 의 git log 가 4 commit (Task 0~3) 으로 정착.

---

## Self-Review (작성 시점 점검 결과)

**1. Spec coverage**
- spec §2 결정 1 (단일 CameraStage) — Task 2 의 camera_stage.h/cpp ✓
- spec §2 결정 2 ([[deprecated]] 1b) — Task 1 ✓ + Task 0 (build 호환) ✓
- spec §2 결정 3 (RenderWithCamera public) — Task 1 Step 2 ✓
- spec §2 결정 4 (ScreenQuadStage 흡수) — Task 3 Step 3+4 ✓
- spec §2 결정 5 (Effekseer 손대지 않음) — Task 4 Step 2 항목 #4 명시 ✓
- spec §2 결정 6 (ImGui 손대지 않음) — Task 4 Step 2 항목 #6 명시 ✓
- spec §3.1 변경 명세표 6 파일 (Task 0~3 의 cmake/CXXStandard.cmake + 5 파일) = plan 의 6 파일 ✓
- spec §7.1 prev-frame stale — Task 3 Step 5 의 주석 "지난 프레임 PassComponent 출력" 으로 명시 ✓
- spec §7.2 raw 포인터 캐싱 — Task 3 Step 2~5 전반 ✓

**2. Placeholder scan**
- "TBD" / "TODO" / "implement later" — 없음 ✓
- 모든 step 에 *완전한 코드 블록* — Task 1~3 모두 ✓
- exact 파일 경로 + line 번호 컨텍스트 — 모두 ✓

**3. Type consistency**
- `CameraStage(SceneRenderer*, Scene::Camera*)` ctor — Task 2 선언 + Task 3 사용 일치 ✓
- `mScreenQuadStagePtr` 명명 — Task 3 멤버 추가 + render() 사용 일치 ✓
- `mStages` 명명 — 통일 ✓
- `Render(RenderTarget& target)` override 시그니처 — IRenderStage 와 일치 ✓

---
