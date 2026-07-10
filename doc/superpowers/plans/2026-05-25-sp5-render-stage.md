# SP5 — IRenderStage 추상 + Layer 시스템 정착 Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `SJH::IRenderStage` 추상 + `Layer` 비트 시스템 정착 — `SceneRenderer` 가 추상 구체 1호로 합류, `tweeny_demo` 가 Camera 우회 부채 청산 후 정통 패턴 진입.

**Architecture:** 직교 축 분리 — `RenderTarget` ("어디에") + `IRenderStage` ("무엇을") + `Pass::Kind` (한 stage *내부* Queue Layer). `Layer = enum class : uint64_t` (FSM 컨벤션 일관). Application 의 `mStages` non-owning vector 가 호출 순서 명시.

**Tech Stack:** C++17, CMake 3.14+, OpenGL 4.1 Core, vmath, sb7. 본 SP 외부 의존 없음.

**Spec:** [doc/superpowers/specs/2026-05-25-sp5-render-stage-design.md](../specs/2026-05-25-sp5-render-stage-design.md)

**테스트 정책:** [no_auto_tests](../../memory/no_auto_tests.md) — 단위 테스트 자동 추가 금지. 본 plan 의 검증 모드 = *빌드 PASS + 시각 회귀* (3 데모).

---

## Pre-Flight Findings (사전 검증 결과)

본 plan 작성 직전 수행된 grep 으로 spec §4.10 의 ambiguity (후보 A vs B) 가 해소됨:

```
<apps>/migrate_demo/main.cpp:43   #include "common/layer.h"
<apps>/migrate_demo/main.cpp:140  sceneCamActor->SetLayer(SJH::LAYER_SCENE);
<apps>/migrate_demo/main.cpp:146  sceneCam->CullingMask = SJH::LAYER_SCENE;
<apps>/effekseer_demo/demo1/main.cpp:10  #include "common/layer.h"
<apps>/effekseer_demo/demo1/main.cpp:52  camActor->SetLayer(SJH::LAYER_SCENE);
```

**결론**: 후보 A — `src/common/layer.h` 가 *실제 사용 중*. 본 plan 의 **Task 7** 가 spec §10.1 의 "삭제" 가 아니라 **deprecated alias 로 변환** (데모 영향 0). 미래 별 SP (예: SP-LayerMigration) 가 데모 마이그레이션 + alias 삭제. spec D-9 의 *옵션 분기* 중 *2번째 옵션* 채택.

---

## Summary

| # | Task | 변경 파일 | 핵심 결과 | 검증 |
|---|---|---|---|---|
| 1 | Layer enum class 신설 | `src/scene/layer.h` (NEW) | `enum class Layer : uint64_t` + 비트 연산자 | `SJH::scene` build PASS |
| 2 | Actor type uint32→uint64 + Layer overload | `src/scene/actor.h` | `mLayer` uint64, `SetLayer(Layer)` overload | 모든 데모 build PASS (auto promotion) |
| 3 | Camera type uint32→uint64 + scene_renderer.cpp cast | `src/scene/camera.h`, `<src>/render/scene_renderer.cpp:236` | `CullingMask` uint64, `SetCullingMask(Layer)` overload | 모든 데모 build PASS |
| 4 | IRenderStage 추상 신설 | `src/render/render_stage.{h,cpp}` (NEW), `src/render/CMakeLists.txt` | `class IRenderStage` + vtable home TU | `SJH::render` build PASS |
| 5 | SceneRenderer : IRenderStage 상속 | `<src>/render/scene_renderer.h` | 상속 선언 + `override` 키워드 | `SJH::render` build PASS |
| 6 | tweeny_demo Overlay Camera 마이그레이션 | `<apps>/tweeny_demo/main.cpp` | SceneCamera Actor + `mStages` vector + onResize broadcast | `tweeny_demo` build + 시각 회귀 |
| 7 | src/common/layer.h → deprecated alias 변환 | `src/common/layer.h` (MOD) | 옛 `LAYER_SCENE` 가 새 enum 위 wrapper | migrate_demo + effekseer_demo build PASS |
| 8 | 3 데모 시각 회귀 + 종합 검증 | (변경 없음) | tweeny + migrate + _MyApp_ 정상 동작 | 사용자 시각 확인 |

**Task 의존**: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 (순차). Task 6 의 tweeny 마이그레이션 후 Task 7 (alias 변환) 가 다른 데모를 망가뜨리지 않는지 검증.

**Invariant (불변식)**:
- 매 task 종료 시 *모든 데모 build PASS*. 한 task 가 build break 면 *그 task 안에서* 해결 (다음 task 진입 금지).
- `git status` 가 *현 task 의 변경만* 표시 (다른 working tree 변경 격리).

---

## Task 1: `SJH::Scene::Layer` enum class 신설

**Files:**
- Create: `src/scene/layer.h`

**Note**: `src/scene/CMakeLists.txt` 는 *수정 불요* — `target_include_directories(... PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)` 가 *디렉토리 단위* 노출이라 신규 헤더 자동 포함.

- [ ] **Step 1: 빈 파일 생성 (Write tool 사전요건)**

```bash
touch src/scene/layer.h
```

- [ ] **Step 2: layer.h 내용 작성**

```cpp
#ifndef __SJH_SCENE_LAYER_H__
#define __SJH_SCENE_LAYER_H__

#include <cstdint>

namespace SJH::Scene
{
    /// @brief Actor 가시성 비트 약속 — Unity LayerMask 정통, FSM uint64 enum 컨벤션과 일관.
    /// @details Actor.SetLayer(Layer) 의 비트 + Camera.CullingMask 의 AND 검사.
    ///          uint64 → 64 비트 (Unity 32 보다 풍부). FSM StateMachine 의 enum class 비트 패턴과 동일.
    ///          새 비트 추가 시 본 파일에 *모든 비트 자리 검토 후* 진입.
    enum class Layer : uint64_t
    {
        Default   = 1ull << 0,   ///< 1  — 모든 Actor 기본
        Player    = 1ull << 1,   ///< 2  — 미래 게임 로직 (예약)
        Enemy     = 1ull << 2,   ///< 4  — 미래 게임 로직 (예약)
        UI        = 1ull << 3,   ///< 8  — 본 SP 가 자리 정의 (미래 UI Camera 진입자가 사용)
        DebugDraw = 1ull << 4,   ///< 16 — 미래 DebugDrawStage (예약)

        All       = ~0ull,       ///< Camera 기본 mask — 모든 layer 매치
    };

    /// @brief 비트 OR — 여러 layer 결합 (`Layer::Default | Layer::UI`).
    constexpr Layer operator|(Layer a, Layer b) noexcept
    {
        return static_cast<Layer>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
    }

    /// @brief 비트 AND — cullingMask 검사. 결과 uint64 (0 이면 매치 안 됨).
    constexpr uint64_t operator&(Layer a, Layer b) noexcept
    {
        return static_cast<uint64_t>(a) & static_cast<uint64_t>(b);
    }

    /// @brief Layer → uint64 명시 변환 (호환 호출용).
    constexpr uint64_t ToBits(Layer l) noexcept { return static_cast<uint64_t>(l); }
}

#endif // __SJH_SCENE_LAYER_H__
```

- [ ] **Step 3: 빌드 검증 — SJH::scene 컴파일 unit 영향 없음 확인**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_scene 2>&1 | tail -5
```

Expected: `[1/1] Linking CXX static library ... libsjhopengl_scene.a` 또는 `ninja: no work to do.` (헤더-only 라 .cpp 변경 0).

- [ ] **Step 4: Commit**

```bash
git add src/scene/layer.h
git commit -m "feat(scene): SJH::Scene::Layer enum class (uint64) 신설

- Default/Player/Enemy/UI/DebugDraw 5 비트 자리 예약
- operator| / operator& / ToBits 비트 연산자 자유 함수
- FSM StateMachine 의 uint64 enum class 컨벤션과 일관

SP5 Task 1."
```

---

## Task 2: Actor type uint32 → uint64 + Layer overload

**Files:**
- Modify: `src/scene/actor.h:97-98` (`SetLayer/GetLayer` 시그니처) + `:115` (`mLayer` type)

- [ ] **Step 1: actor.h 의 layer 헤더 include + mLayer/SetLayer/GetLayer 수정**

Edit `src/scene/actor.h` 의 line 1-13 (include 영역) 에 layer.h 추가:

```diff
  #include "object/transform.h"
+ #include "scene/layer.h"
  #include <cassert>
```

Edit line 97-98 (SetLayer/GetLayer):

```diff
- void     SetLayer(uint32_t layer) { mLayer = layer; }
- uint32_t GetLayer() const         { return mLayer; }
+ void     SetLayer(uint64_t layer) { mLayer = layer; }
+ void     SetLayer(SJH::Scene::Layer l) { mLayer = SJH::Scene::ToBits(l); }
+ uint64_t GetLayer() const         { return mLayer; }
```

Edit line 115 (mLayer 멤버):

```diff
- uint32_t    mLayer   = 1u;   // 기본 layer (비트 0) — 모든 Camera 의 기본 mask(~0u) 와 매치.
+ uint64_t    mLayer   = SJH::Scene::ToBits(SJH::Scene::Layer::Default); // SP5 — Layer::Default = 비트 0
```

- [ ] **Step 2: 전체 빌드 검증 — uint32 → uint64 promotion 호환 확인**

Run:
```bash
cmake --build --preset ninja --target tweeny_demo migrate_demo _MyApp_ effekseer_demo 2>&1 | tail -15
```

Expected: 4 데모 모두 build PASS. `SetLayer(SJH::LAYER_SCENE)` (uint32 변수) 호출은 *자동 promotion* 으로 작동.

- [ ] **Step 3: Commit**

```bash
git add src/scene/actor.h
git commit -m "refactor(scene): Actor.mLayer uint32 → uint64 + SetLayer(Layer) overload

- mLayer / SetLayer / GetLayer 의 type uint32_t → uint64_t
- SetLayer(Layer) type-safe overload 추가 (raw uint 호환 유지)
- 기본값 Layer::Default 명시 사용
- 옛 호출처는 auto promotion 으로 영향 0

SP5 Task 2."
```

---

## Task 3: Camera type uint32 → uint64 + scene_renderer.cpp cast 보정

**Files:**
- Modify: `src/scene/camera.h:5` (cstdint 주석) + `:41` (CullingMask type) + `SetCullingMask` overload 추가
- Modify: `<src>/render/scene_renderer.cpp:236` (0u → 0ull)

- [ ] **Step 1: camera.h 수정 — CullingMask type + SetCullingMask overload**

Edit `src/scene/camera.h:5` (include 영역에 layer.h 추가):

```diff
- #include <cstdint>       // uint32_t for cullingMask (SP4 D-15)
+ #include <cstdint>       // uint64_t for cullingMask (SP5 D-5)
+ #include "scene/layer.h"
```

Edit line 41 (CullingMask 멤버):

```diff
- uint32_t CullingMask = ~0u; // Unity Camera.cullingMask — 기본 모든 layer.
+ uint64_t CullingMask = SJH::Scene::ToBits(SJH::Scene::Layer::All); // Unity Camera.cullingMask — 기본 모든 layer.
```

CullingMask 멤버 직후에 SetCullingMask overload 2 개 추가:

```cpp
        /// @brief 비트마스크 직접 주입 (옛 호환).
        void SetCullingMask(uint64_t mask)             { CullingMask = mask; }
        /// @brief type-safe Layer overload (SP5).
        void SetCullingMask(SJH::Scene::Layer l)       { CullingMask = SJH::Scene::ToBits(l); }
```

- [ ] **Step 2: scene_renderer.cpp:236 의 비교 literal 보정**

Edit `<src>/render/scene_renderer.cpp:236`:

```diff
- const bool visibleToCamera = (cullingMask & actor.GetLayer()) != 0u;
+ const bool visibleToCamera = (cullingMask & actor.GetLayer()) != 0ull;
```

- [ ] **Step 3: 전체 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target tweeny_demo migrate_demo _MyApp_ effekseer_demo 2>&1 | tail -15
```

Expected: 4 데모 build PASS.

- [ ] **Step 4: Commit**

```bash
git add src/scene/camera.h <src>/render/scene_renderer.cpp
git commit -m "refactor(scene/render): Camera.CullingMask uint32 → uint64 + SetCullingMask(Layer)

- CullingMask type uint32_t → uint64_t
- SetCullingMask(Layer) / SetCullingMask(uint64_t) 2 overload 추가
- scene_renderer.cpp:236 의 0u → 0ull (uint64 비교 일관)
- 옛 직접 멤버 접근 (cam->CullingMask = ...) 은 자동 promotion

SP5 Task 3."
```

---

## Task 4: IRenderStage 추상 신설

**Files:**
- Create: `<src>/render/render_stage.h`
- Create: `<src>/render/render_stage.cpp`
- Modify: `src/render/CMakeLists.txt:1-8` (add `render_stage.cpp` to STATIC sources)

- [ ] **Step 1: 빈 파일 생성**

```bash
touch <src>/render/render_stage.h <src>/render/render_stage.cpp
```

- [ ] **Step 2: render_stage.h 작성**

```cpp
#ifndef __SJH_IRENDER_STAGE_H__
#define __SJH_IRENDER_STAGE_H__

namespace SJH
{
    class RenderTarget;

    /// @brief 최상위 렌더 단계 추상 — Unity ScriptableRendererFeature / Unreal FSceneRenderer 정통.
    /// @details Application 의 render() 가 보유한 IRenderStage* vector 를 순서대로 호출.
    ///          각 stage 는 *직교 책임* — Scene / PostFX / ImGui / Skybox / DebugDraw 등.
    ///          `Material::Pass::Kind` 와 *다른 레이어* — Pass 는 한 stage *내부* Queue 분류.
    class IRenderStage
    {
    public:
        virtual ~IRenderStage() = default;

        /// @brief 매 프레임 — Application 이 backbuffer 를 주입.
        /// @details 구체가 자기 안에서 DeviceContext::BeginFrame(target) 호출 책임.
        virtual void Render(RenderTarget& target) = 0;

        /// @brief Window resize broadcast — 자기 internal FBO 등 sync 용. 기본 no-op.
        /// @note SceneRenderer 는 미사용 (Camera 가 매 프레임 target.GetSize 동적 조회).
        virtual void OnResize(int /*w*/, int /*h*/) {}
    };
}

#endif // __SJH_IRENDER_STAGE_H__
```

- [ ] **Step 3: render_stage.cpp 작성 (vtable home TU)**

```cpp
// vtable home TU — header-only abstract 의 ODR 보장 (다른 .cpp 들이
// vtable 의 외부 정의를 한 곳에서 받음). 본 파일은 *내용 없음* 이 정답.
#include "<render>/render_stage.h"
```

- [ ] **Step 4: CMakeLists.txt 수정 — render_stage.cpp 추가**

Edit `src/render/CMakeLists.txt:1-8`:

```diff
  add_library(sjhopengl_render STATIC
      device_context.cpp
      render_target.cpp           # vtable home TU — virtual class 의 ODR 보장 (header-only 만으로 부족)
+     render_stage.cpp            # vtable home TU — IRenderStage virtual class (SP5)
      mesh_pass_processor.cpp
      pipeline_state_setter.cpp   # SP-PipelineSetter — GL state machine 분리 (Applier 패턴)
      property_block_setter.cpp   # SP-PropertyBlockSetter — 옛 MaterialApplier rename + 시그니처 정밀화
      scene_renderer.cpp
  )
```

- [ ] **Step 5: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_render 2>&1 | tail -5
```

Expected: `render_stage.cpp.o` 빌드 + libsjhopengl_render.a re-link.

- [ ] **Step 6: Commit**

```bash
git add <src>/render/render_stage.h <src>/render/render_stage.cpp src/render/CMakeLists.txt
git commit -m "feat(render): SJH::IRenderStage 추상 신설 (SP5)

- 최상위 렌더 단계 인터페이스 — Render(RT&) 필수 + OnResize(int,int) no-op 기본
- vtable home TU 별도 분리 (ODR 보장)
- Unity ScriptableRendererFeature / Unreal FSceneRenderer 정통 매핑
- Material::Pass::Kind 와 직교 (Pass = 한 stage 내부 Queue, Stage = 최상위 단계)

SP5 Task 4."
```

---

## Task 5: SceneRenderer : public IRenderStage 상속

**Files:**
- Modify: `<src>/render/scene_renderer.h` (include + 상속 + override 키워드)

- [ ] **Step 1: scene_renderer.h 의 include 추가 + 상속 + override 표시**

Edit `<src>/render/scene_renderer.h:1-5` 의 include 영역:

```diff
  #include "<render>/mesh_pass_processor.h"
+ #include "<render>/render_stage.h"
  #include <cstdint>
```

Edit line 25-33 의 class 선언:

```diff
- class SceneRenderer
+ class SceneRenderer : public IRenderStage
  {
  public:
      ...
-     void Render(RenderTarget& defaultTarget);
+     void Render(RenderTarget& defaultTarget) override;

      /// @brief 명시 view/proj — 단위 테스트 + 디버그용 (CameraComponent 우회).
      /// @details 기존 인터페이스 보존 — CameraComponent 없이 임의 view/proj 직접 주입 가능.
      void Render(RenderTarget& defaultTarget,
                  const vmath::mat4& viewMat, const vmath::mat4& projMat);
```

`OnResize` 는 IRenderStage 의 기본 no-op 사용 — `SceneRenderer` 가 override 안 함 (Camera 가 매 frame `target.GetSize` 동적 조회).

- [ ] **Step 2: 빌드 검증 — 시그니처 일치 + 상속 작동**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_render 2>&1 | tail -5
```

Expected: `scene_renderer.cpp.o` 빌드 PASS. 만약 `'override' has no member` 같은 에러 나오면 IRenderStage 의 시그니처와 SceneRenderer 의 Render 시그니처 *완전 일치 검증* (특히 const/&).

- [ ] **Step 3: 전체 데모 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target tweeny_demo migrate_demo _MyApp_ effekseer_demo 2>&1 | tail -10
```

Expected: 4 데모 PASS (SceneRenderer 의 외부 인터페이스 변화 0).

- [ ] **Step 4: Commit**

```bash
git add <src>/render/scene_renderer.h
git commit -m "refactor(render): SceneRenderer : public IRenderStage 상속 (SP5)

- IRenderStage 의 첫 구체 합류 — Render(RT&) override 표시
- 기능 변화 0 (시그니처 동일, override 키워드 추가만)
- Render(RT&, view, proj) 우회 overload 는 유지 (단위 테스트/디버그용)
- OnResize 는 IRenderStage 의 기본 no-op 사용 (Camera 가 RT.GetSize 동적 조회)

SP5 Task 5."
```

---

## Task 6: tweeny_demo Overlay Camera 마이그레이션

**Files:**
- Modify: `<apps>/tweeny_demo/main.cpp` (startup() 의 SceneCamera Actor, render() 의 vector loop, onResize() 추가)

- [ ] **Step 1: main.cpp 의 include 영역 확인 + 추가**

기존 include 위에 추가:

```diff
+ #include "<render>/render_stage.h"
+ #include "scene/compound_actor.h"   // CreateCameraActor
+ #include "scene/layer.h"
+ #include <vector>
```

(이미 include 된 것은 중복 방지.)

- [ ] **Step 2: tweeny_application 클래스에 mStages 멤버 추가**

`SJH::SceneRenderer mRenderSys;` 멤버 직후에 추가:

```cpp
        std::vector<SJH::IRenderStage*> mStages;   // 호출 순서, non-owning
```

- [ ] **Step 3: startup() 의 끝 (Director::Enter() 직전) 에 SceneCamera Actor + mStages 셋업 삽입**

`dir.Enter();` 호출 *직전* 에 추가:

```cpp
        // === SP5 — SceneCamera Actor 도입 (옛 Render(RT, I, I) 우회 청산) ===
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);

        auto camActor = SJH::Scene::CreateCameraActor("SceneCamera",
            /*fov*/45.0f, aspect, /*near*/0.1f, /*far*/100.0f);
        camActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 5.0f);
        auto* cam = camActor->GetComponent<SJH::Scene::Camera>();
        cam->SetCullingMask(SJH::Scene::Layer::Default);   // 명시 — UI 비트 제외
        cam->SetTargetFramebuffer(nullptr);                // backbuffer
        dir.Root().AddChild(std::move(camActor));
        dir.SetActiveCamera(cam);

        mStages.push_back(&mRenderSys);
```

- [ ] **Step 4: render() 함수의 우회 overload 호출 제거 + vector loop 으로 교체**

`mRenderSys.Render(*mDefaultTarget, I, I);` 같은 *Camera 우회 호출* 라인을 찾아 다음으로 교체:

```diff
- mRenderSys.Render(*mDefaultTarget, /*viewMat*/I, /*projMat*/I);
+ for (auto* s : mStages) s->Render(*mDefaultTarget);
```

(만약 `I` 매트릭스 선언 라인이 위에 있다면 그것도 제거.)

- [ ] **Step 5: onResize override 추가 — broadcast 패턴**

기존 `onResize` 가 있다면 끝에 `for (auto* s : mStages) s->OnResize(w, h);` 추가. 없다면 신규 작성:

```cpp
        void onResize(int w, int h) override
        {
            sb7::application::onResize(w, h);
            mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(w, h);
            for (auto* s : mStages) s->OnResize(w, h);   // broadcast
        }
```

- [ ] **Step 6: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target tweeny_demo 2>&1 | tail -10
```

Expected: `tweeny_demo` link PASS.

- [ ] **Step 7: 시각 회귀 검증 — 사용자 실행**

Run:
```bash
cd build_ninja/apps/tweeny_demo && ./tweeny_demo
```

Expected:
- 행/열 quad 패턴 *시각 동일* (M1 의 결과와 같은 ping-pong 트위닝).
- 창 resize 시 viewport 정상 갱신.
- 콘솔: `SceneRenderer::Render — 씬 트리에 Camera 컴포넌트 없음` warn *0건* (SceneCamera Actor 가 수집됨).

만약 화면이 비어 보이면 — SceneCamera 의 Transform.Translate 가 quad 영역을 *벗어남* 가능. tweeny_demo 의 quad Actor 들의 좌표를 확인 후 cam.Translate 조정 (예: `(0, 0, 10)`).

- [ ] **Step 8: Commit**

```bash
git add <apps>/_MyApp_/../tweeny_demo/main.cpp  # 정확한 경로: <apps>/tweeny_demo/main.cpp
git commit -m "feat(tweeny_demo): SceneCamera Overlay + IRenderStage 패턴 (SP5)

- SceneCamera Actor 도입 (depth=0, cullingMask=Layer::Default, targetFB=nullptr)
- Render(RT, I, I) 우회 overload 사용 → IRenderStage::Render(RT) 정통 호출
- std::vector<IRenderStage*> mStages 비소유 호출 순서
- onResize() broadcast (vector loop)
- 행/열 quad 패턴 시각 동일

SP5 Task 6."
```

---

## Task 7: `src/common/layer.h` → deprecated alias 변환

**Files:**
- Modify: `src/common/layer.h` (옛 constexpr 를 *새 enum 위의 wrapper* 로)

**Pre-flight 결과 반영**: spec §4.10 의 *후보 A 확정* (migrate_demo + effekseer_demo 가 사용 중). *삭제 대신 alias 유지* — 데모 영향 0. 미래 별 SP 가 데모 마이그레이션 + 본 파일 삭제.

- [ ] **Step 1: src/common/layer.h 의 옛 LAYER_SCENE 을 새 enum 의 alias 로 변환**

Edit `src/common/layer.h` 의 line 18 부근 (정확한 위치는 파일 확인 후 결정):

```diff
+ #include "scene/layer.h"
+
  namespace SJH
  {
-     constexpr uint32_t LAYER_SCENE = 1u;
+     /// @deprecated SP5 — `SJH::Scene::Layer::Default` 사용 권장.
+     ///             옛 호출처 (migrate_demo, effekseer_demo) 호환 유지용 alias.
+     ///             별 SP (SP-LayerMigration) 에서 데모 마이그레이션 후 본 파일 삭제 예정.
+     constexpr uint64_t LAYER_SCENE = static_cast<uint64_t>(SJH::Scene::Layer::Default);
  }
```

(만약 `src/common/layer.h` 가 다른 상수도 보유한다면 — 그것들도 deprecated alias 로 변환 또는 그대로 유지.)

- [ ] **Step 2: 데모 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target migrate_demo effekseer_demo 2>&1 | tail -10
```

Expected: 2 데모 build PASS. `SetLayer(SJH::LAYER_SCENE)` 호출은 `SJH::LAYER_SCENE` 이 uint64 라 *자동 호환*.

만약 `cam->CullingMask = SJH::LAYER_SCENE;` (migrate_demo:146) 가 *type 불일치* 경고 — Task 3 의 CullingMask 도 uint64 라 OK.

- [ ] **Step 3: Commit**

```bash
git add src/common/layer.h
git commit -m "refactor(common): LAYER_SCENE deprecated alias 로 변환 (SP5)

- src/common/layer.h 의 constexpr uint32 LAYER_SCENE 가
  SJH::Scene::Layer::Default 의 uint64 wrapper 가 됨
- 옛 호출처 (migrate_demo:140,146 / effekseer_demo:52) 변경 0
- 미래 SP-LayerMigration 가 데모 마이그레이션 후 본 파일 삭제 예정
- spec D-9 의 옵션 분기 중 2번째 채택 (후보 A 확정 — pre-flight 결과 반영)

SP5 Task 7."
```

---

## Task 8: 3 데모 시각 회귀 + 종합 검증

**Files:** (변경 없음 — 검증 task)

- [ ] **Step 1: clean build — 캐시 영향 격리**

Run:
```bash
cmake --build --preset ninja --target tweeny_demo migrate_demo _MyApp_ effekseer_demo 2>&1 | tail -20
```

Expected: 4 데모 모두 PASS. compiler warning 0 (uint32→uint64 promotion 은 silent).

- [ ] **Step 2: tweeny_demo 시각 회귀 — 행/열 quad ping-pong 트위닝**

Run:
```bash
cd build_ninja/apps/tweeny_demo && ./tweeny_demo
```

기대:
- 11 row × N col 의 quad 가 좌→우→좌 트위닝 (easing 별 다른 곡선).
- 우클릭 드래그 카메라 회전 *없음* (tweeny 는 마우스 입력 미사용).
- 콘솔 warn 0건 (SceneCamera 가 수집됨).

- [ ] **Step 3: migrate_demo 시각 회귀 — Scene + PostFX 5-chain**

Run:
```bash
cd build_ninja/apps/migrate_demo && ./migrate_demo
```

기대:
- Scene 본체 (Box / Plane / Outline / Window / Light marker) 정상 표시.
- PostFX 5 chain (알파벳 순) 정상 적용.
- ImGui 패널 (좌상) 정상.
- LAYER_SCENE = uint64 호환으로 SetLayer / CullingMask 직접 대입 *영향 0*.

- [ ] **Step 4: _MyApp_ 시각 회귀 — 탑다운 슈터**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

기대:
- TestPattern atlas sprite 정상 표시.
- WASD Player 이동, Mouse 우클릭 드래그 yaw/pitch.
- Camera follow target 정상 (Player 머리 위에서 추적).
- Layer 변경 영향 *없음* (모든 Actor 기본 layer Default).

- [ ] **Step 5: (선택) effekseer_demo 시각 회귀**

Run:
```bash
cd build_ninja/apps/effekseer_demo/demo1 && ./demo1
```

기대: Effekseer 파티클 정상 표시.

- [ ] **Step 6: SP5 완료 tag commit**

```bash
git commit --allow-empty -m "milestone(SP5): IRenderStage + Layer 시스템 정착 완료

Task 1-7 완료 + 시각 회귀 4 데모 PASS:
- tweeny_demo: SceneCamera Overlay + IRenderStage 정통 호출
- migrate_demo: Layer/CullingMask uint64 호환 (rewrite 0)
- _MyApp_: Layer 변경 영향 0
- effekseer_demo: 변경 영향 0

Future seam (별 SP):
- SP-PostFXStage: migrate_demo Camera chain → PostFXStage 클래스
- SP-FramebufferResize: Framebuffer::Resize + EnsureSize
- SP-LayerMigration: 데모들의 LAYER_SCENE → Layer::Default 마이그레이션 + src/common/layer.h 삭제
- SP-RendererTraverseRefactor: scene_renderer.cpp 의 4 DFS pass → generic template (9 후보 중 #7)

SP5."
```

---

## Self-Review

### Spec coverage 검사

| spec §10.1 task | 본 plan task | 매핑 |
|---|---|---|
| 1. `src/scene/layer.h` 신설 | Task 1 | ✅ |
| 2. Actor/Camera type uint32→uint64 + Layer overload | Task 2 + Task 3 | ✅ (분리됨 — Actor 와 Camera+cpp cast 가 별 task) |
| 3. `src/render/render_stage.{h,cpp}` 신설 + CMakeLists | Task 4 | ✅ |
| 4. SceneRenderer 의 `: public IRenderStage` 상속 + override | Task 5 | ✅ |
| 5. scene_renderer.cpp 의 cullingMask 비교 0u → 0ull | Task 3 안 포함 | ✅ (Camera type 변경과 연동) |
| 6. tweeny_demo 마이그레이션 | Task 6 | ✅ |
| 7. `src/common/layer.h` 삭제 (homestead 검증) | Task 7 (**삭제 → alias 변환** 으로 정정 — pre-flight 결과 반영) | ✅ + 정정 |
| 8. 4 데모 빌드 PASS + 시각 회귀 검증 | Task 8 | ✅ (4 → 3 주요 + 1 선택) |

**spec §4 file-by-file 와의 매핑**: 모든 NEW/MOD/DEL 항목이 task 1-7 에 분산. ✅

### Placeholder scan

- "TBD/TODO/implement later" — 없음 ✅
- "Add appropriate error handling" — 없음 ✅
- "Write tests for the above" (without code) — 없음 (no_auto_tests 정책상 단위 테스트 미작성) ✅
- "Similar to Task N" — 없음 (각 task 의 코드가 전부 인라인) ✅
- 코드 블록 없는 step — 없음 ✅
- 정의 안 된 type/function 참조 — 없음 (모두 spec §4 의 시그니처에서 도출) ✅

### Type consistency

- `Layer::Default` / `Layer::All` 사용 일관 — Task 1 정의 ↔ Task 2 (Actor.mLayer 기본값) ↔ Task 3 (Camera.CullingMask 기본값) ↔ Task 6 (tweeny SetCullingMask) ↔ Task 7 (LAYER_SCENE alias) ✅
- `SJH::Scene::ToBits(Layer)` 사용 일관 — Task 2/3/7 ✅
- `SetLayer(Layer)` overload 시그니처 — Task 2 정의 ↔ Task 6 사용 ✅
- `SetCullingMask(Layer)` overload 시그니처 — Task 3 정의 ↔ Task 6 사용 ✅
- `IRenderStage::Render(RenderTarget&)` 시그니처 — Task 4 정의 ↔ Task 5 override ↔ Task 6 호출 ✅
- 멤버 명 `mStages` 일관 — Task 6 의 6 step 에 거쳐 ✅

### 발견된 spec 가정 정정 (pre-flight)

spec §10.1 의 task 7 "src/common/layer.h 삭제" 가 *후보 B 가정* 의 task — 실제 grep 으로 *후보 A 확정* (migrate_demo + effekseer_demo 사용). 본 plan Task 7 가 *deprecated alias 변환* 으로 정정 (spec §4.10 의 옵션 1 채택). 데모 영향 0.

---

## Execution Handoff

Plan 작성 + self-review 완료. 두 가지 실행 옵션:

1. **Subagent-Driven (recommended)** — `superpowers:subagent-driven-development`. Fresh subagent 가 각 task 별로 dispatch + 두 단계 review (spec 정합 + 코드 품질). 빠른 iteration.
2. **Inline Execution** — `superpowers:executing-plans`. 본 세션에서 batch 실행 + checkpoint review. controller 가 직접 코드 수정.

사용자 선택:
- Subagent-Driven: 8 task × 2 review = 16 subagent dispatch (병렬 가능한 부분 있음).
- Inline: 본 세션에서 순차 task 1-8 진행 (1~2 시간 예상).
