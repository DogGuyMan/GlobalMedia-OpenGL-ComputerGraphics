# Warmup 보일러플레이트 추출 — main.cpp 책임 분리 (A 그룹)

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **작성일**: 2026-05-31
> **선행**: ParticleStage SP 완료 ([`2026-05-27-particle-stage-design.md`](2026-05-27-particle-stage-design.md))
> **관련 문서**: [`.claude/architecture.md`](../../../.claude/architecture.md) §1 (YAGNI/명시적 의존성), §4 (PUBLIC vs PRIVATE), §11.3 (자원 보유 컨벤션), [`<.agents>/skills/clean-ddd-hexagonal/SKILL.md`](../../../.agents/skills/clean-ddd-hexagonal/SKILL.md)
> **대상 데모**: `apps/_MyApp_` 만 — Warmup 패턴이 본 데모에서 발현. migrate_demo / audio_demo 는 영향 0
> **후속 SP 후보**: B 그룹 (ImGui bootstrap 분리), CreateWorldCameraActor 분리, render_pipeline 모듈 승격

---

## 1. 동기 — 왜 이 SP 가 필요한가

### 1.1 직접 동기 — main.cpp 의 책임 누수 (250줄 보일러)

`apps/_MyApp_/main.cpp` 의 startup() 가 *씬 구성 + 렌더 셋업 + UI 셋업* 250줄을 직접 수행. 5 개의 `Warmup*` private 메서드 (CreateAndRegister*, WramupSceneRenderer, WarmupPassRenderer, WarmupSkybox) 가 *Application 의 책임이 아닌 Engine 보일러* 를 포함.

증상:
- main.cpp 길이 ~620줄 — God Object 진입
- `WramupSceneRenderer` / `WarmupPassRenderer` 의 *모든 코드* 가 *다른 PostFX 사용 데모* 가 그대로 복사할 보일러
- `glfwGetFramebufferSize + aspect` 계산이 *5번 반복* (서로 다른 메서드 안)
- ScreenCamera Ortho 셋업이 *PostFX 2-Camera 패턴의 고정 설정* 인데 *데모마다 22줄 복제*

### 1.2 SKILL 기준 평가 — 본 프로젝트 한계

`<.agents>/skills/clean-ddd-hexagonal/SKILL.md` 의 *"When to Use" 표* 에서 본 프로젝트는 다수 *Skip* 조건 충족 (1인, 학습 규모, 단일 entry, swap 의도 없음). 따라서 **부분 적용**:
- ✅ **Dependency Rule** (main → Client modules → SJH::engine core 단방향)
- ✅ **Anti-patterns 회피** (Skipping Use Cases, Leaking Infrastructure, God Module)
- ❌ Use Case / Repository / CQRS 등은 부적합

### 1.3 보일러 vs Client 분류 (분석 완료)

| 그룹 | 범위 | 본 SP 처리 |
|---|---|---|
| **A — Engine 공통 보일러** (A1~A5) | window helper, ScreenCamera factory, RenderPipeline 셋업, PostFX 체인, Skybox factory | ✅ 본 SP 처리 |
| **B — Client UI 공통** (B1~B3) | ImGui init, PostFXDebug entries 빌드, gamma data-driven | ⚠ B3 의 *데이터 driven 측* (PostFXStageConfig.InitFloats) 만 본 SP. UI 측은 별도 SP |
| **C — 본 데모 한정** | FMOD bank/event 이름, PlayerActorConfig, matrix_skybox shader, World Camera Controller | ❌ 그대로 유지 |

---

## 2. 핵심 결정 (확정 6종)

| ID | 결정 | 채택 옵션 | 근거 (SKILL/architecture 정합) |
|----|------|----------|--------------------------------|
| **D-1** | 보일러 거주 모듈 전략 | 기존 모듈 확장 + 새 헤더 1개 (옵션 1) | 옵션 2 (SJH::bootstrap) 는 *Composition Root 가 inner 모듈에 들어가는 critical 위반* (SKILL line 113). 옵션 3 (render_pipeline 모듈) 은 Premature module split (architecture.md §1 의 "반복 패턴 두 군데 이상 등장 후" 위반). |
| **D-2** | 자유 함수 시그니처 스타일 | Pure factory — caller 가 wiring | compound_actor.h 의 기존 `CreateCameraActor` (ActorUPtr 반환) 컨벤션 일치. caller (main.cpp) 가 Director.AddChild / mStages.push_back 책임. |
| **D-3** | A3+A4 거주 | 새 헤더 `src/render/render_pipeline.{h,cpp}` — SJH::render 모듈 안의 자유 함수 family | render 의 모든 element (SceneRenderer, ScreenQuadStage, PassComponent) 사용 → render 모듈 내부 sub-helper 가 정통. 별도 STATIC 분리는 YAGNI 위반. |
| **D-4** | A2+A5 거주 | 기존 `src/scene/compound_actor.h` 확장 | PreBuilt Actor factory 컨벤션 (compound_actor.h 의 기존 `CreateCameraActor` 패턴) 과 일치. |
| **D-5** | A1 거주 + glfw 의존 격리 | 새 헤더 `src/common/window_helper.{h,cpp}` — `GLFWwindow*` forward declaration | SJH::common 본체가 glfw 전체 의존을 받지 않도록 *별도 헤더로 격리*. window_helper.h 가 GLFWwindow* forward decl, .cpp 만 `<GLFW/glfw3.h>` include. |
| **D-6** | B3 gamma data-driven 도입 범위 | `PostFXStageConfig.InitFloats` 만 본 SP. UI helper 분리는 별도 SP | A4 의 BuildPostFXChain 이 *configs 의 InitFloats* 받아 mat->Properties.Floats 초기화. 기존 `std::string(def.Name) == "gamma"` 분기 hack 제거. |

---

## 3. 변경 후 흐름

### 3.1 Before / After (`apps/_MyApp_/main.cpp::startup()` 핵심 추출)

```
[Before — ~60줄]
WramupSceneRenderer(reg, scene_renderer, PASSTHOURH_PROGRAM_CONFIG);   // 33줄 private
mCamera = CreateAndRegisterWorldCamera();                              // 30줄 private (유지 — Client)
mScreenCamera = CreateAndRegisterScreenCamera();                       // 22줄 private (A2 대상)
WarmupPassRenderer(reg, POSTFX_PROGRAM_CONFIGS);                       // 47줄 private (A4 대상)
// ... mStages.insert 등 ...
WarmupSkybox(reg, dir);                                                // 23줄 private (A5 대상)

[After — ~45줄]
const auto fb = SJH::GetFramebufferInfo(window);                       // A1
mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fb.Width, fb.Height);
mSceneFB = SJH::Framebuffer::Create(fb.Width, fb.Height);

auto sqStage = SJH::Render::SetupDefaultPipeline(reg, manager.SceneRenderer(), mSceneFB.get());  // A3
mScreenQuadStagePtr = sqStage.get();
mStages.push_back(std::move(sqStage));

mCamera = CreateAndRegisterWorldCamera();                              // 유지 — Client 도메인
auto screenCamActor = SJH::Scene::CreateScreenCameraActor("ScreenCamera", fb.Aspect, mSceneFB.get());  // A2
mScreenCamera = screenCamActor->GetComponent<SJH::Scene::Camera>();
auto* screenCamActorPtr = dir.Root().AddChild(std::move(screenCamActor));

auto chain = SJH::Render::BuildPostFXChain(reg, *screenCamActorPtr, configs, mSceneFB.get(), fb.Width, fb.Height);  // A4
mPostFXFBs      = std::move(chain.Framebuffers);
mPassComponents = std::move(chain.PassComponents);

// ... mStages insert + ParticleStage + 나머지 (FMOD/Player/Imgui 유지) ...

// Skybox — A5
WarmupSkyboxClient(reg, dir);  // matrix_skybox 셰이더 부분 유지 + CreateSkyboxActor 호출로 줄어듦
```

### 3.2 render() 의 follow hook (A5) — 셰이더 측 처리 채택

사용자 결정으로 Skybox follow 가 *셰이더 측* (matrix_skybox.vs 가 view matrix 의 translation 성분 제거) 자동 처리됨. → `SyncSkyboxToCamera` 자유 함수 **추가 안 함**. A5 는 `CreateSkyboxActor` factory 만 추출.

```
[Before — 이미 제거됨]
if (mSkyboxActor && mCamera && mCamera->GetOwner())
    mSkyboxActor->GetTransform().Translate = mCamera->GetOwner()->GetTransform().Translate;

[After — 변화 없음]
// 위치는 셰이더가 view 이동 제거로 자동 처리.
```

(`u_time` 갱신은 *데모 한정 matrix_skybox 셰이더* 이므로 보존)

### 3.3 main.cpp 멤버 타입 변경

```
[Before]
std::array<SJH::FramebufferUPtr, 5> mPostFXFBs;   // 고정 5

[After]
std::vector<SJH::FramebufferUPtr> mPostFXFBs;     // BuildPostFXChain 의 configs 가변 size 대응
```

---

## 4. 변경 명세

### 4.1 파일별 변경표

| # | 파일 | 변경 | 추정 라인 |
|---|---|---|---|
| 1 | `src/common/window_helper.h` *(신규)* | `struct FramebufferInfo {int Width, Height; float Aspect;}` + `GetFramebufferInfo(GLFWwindow*)` 선언. `GLFWwindow` forward decl. | +20 |
| 2 | `src/common/window_helper.cpp` *(신규)* | 본구현 — `<GLFW/glfw3.h>` include + `glfwGetFramebufferSize` + aspect 계산 + zero-division 가드 | +20 |
| 3 | `src/common/CMakeLists.txt` | `window_helper.cpp` 등록 (`target_sources` 의 PRIVATE) | +1 |
| 4 | `src/scene/compound_actor.h` | `CreateScreenCameraActor(name, aspect, sceneFB)` + `CreateSkyboxActor(mesh, mat, scale=50)` 선언 추가 | +10 |
| 5 | `src/scene/compound_actor.cpp` | 2 함수 본구현. CreateScreenCameraActor 는 기존 main.cpp:395~416 의 22줄 그대로 이주. CreateSkyboxActor 는 main.cpp:579~584 의 ~6줄 + scale param. | +28 |
| 6 | `<src>/render/render_pipeline.h` *(신규)* | `DefaultPipelineConfig` struct + `SetupDefaultPipeline(reg, sceneRenderer, sceneFB, cfg) → unique_ptr<ScreenQuadStage>` + `PostFXStageConfig` struct + `PostFXChainResult` struct + `BuildPostFXChain(reg, screenCamActor, configs, sceneFB, w, h) → PostFXChainResult` 선언 | +60 |
| 7 | `<src>/render/render_pipeline.cpp` *(신규)* | 2 함수 본구현 — 기존 main.cpp:436~469 (SetupDefaultPipeline) + 471~517 (BuildPostFXChain) 이주. B3 의 gamma 특수 케이스 제거, 대신 PostFXStageConfig.InitFloats map 으로 일반화. | +100 |
| 8 | `src/render/CMakeLists.txt` | `render_pipeline.cpp` 등록 | +1 |
| 9 | `apps/_MyApp_/main.cpp` | (a) 새 include 3개 (window_helper.h, render_pipeline.h, compound_actor.h 이미 있음). (b) startup() 의 5 Warmup 호출을 자유 함수 호출로 교체. (c) `CreateAndRegisterScreenCamera` / `WramupSceneRenderer` / `WarmupPassRenderer` private 메서드 *완전 제거*. (d) `WarmupSkybox` 는 `CreateSkyboxActor` 호출로 ~10줄로 축소 (matrix_skybox 셰이더/텍스처는 유지). (e) render() 의 follow 3줄 → `SyncSkyboxToCamera` 1줄. (f) 멤버 `mPostFXFBs` 타입 `array<5>` → `vector`. (g) anonymous namespace 의 `POSTFX_PROGRAM_CONFIGS` 를 `PostFXStageConfig` 타입으로 변경 (gamma 의 InitFloats={{"gamma", 1.0f}} 추가). | ±130 |

**총 추가 (Engine): ~250줄. main.cpp 순감소: ~130줄.**

### 4.2 새 헤더 — `src/common/window_helper.h`

```cpp
#ifndef __SJH_COMMON_WINDOW_HELPER_H__
#define __SJH_COMMON_WINDOW_HELPER_H__

struct GLFWwindow;  // forward — common 본체가 glfw 전체 의존 받지 않도록 격리 (D-5)

namespace SJH
{
    /// @brief 프레임버퍼 크기 + aspect 1 호출 packed return.
    struct FramebufferInfo
    {
        int   Width  = 0;
        int   Height = 0;
        float Aspect = 0.0f;
    };

    /// @brief glfwGetFramebufferSize + aspect 계산.
    /// @details Retina HiDPI 의 physical framebuffer 기준. Aspect = Width/Height (Height==0 시 0).
    FramebufferInfo GetFramebufferInfo(GLFWwindow* window);
}

#endif // __SJH_COMMON_WINDOW_HELPER_H__
```

### 4.3 새 헤더 — `<src>/render/render_pipeline.h`

```cpp
#ifndef __SJH_RENDER_PIPELINE_H__
#define __SJH_RENDER_PIPELINE_H__

#include "<buffer>/framebuffer.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SJH
{
    class ResourceRegistry;
    class SceneRenderer;
    class ScreenQuadStage;
}
namespace SJH::Scene { class Actor; class PassComponent; }

namespace SJH::Render
{
    /// @brief 기본 RenderPipeline 셋업의 식별자/경로 묶음.
    struct DefaultPipelineConfig
    {
        std::string PassthroughKey    = "screen_passthrough";
        std::string PassthroughVS     = "./resources/shaders/passthrough.vs";
        std::string PassthroughFS     = "./resources/shaders/passthrough.fs";
        std::string ScreenQuadMeshKey = "mesh_screen_quad";
        std::string BypassMatKey      = "mat_bypass_passthrough";
    };

    /// @brief PostFX 사용 데모의 표준 setup.
    /// @details passthrough Program 등록 + ScreenQuad Mesh 등록 + bypassMaterial 등록
    ///          + SceneRenderer 에 SetScreenQuadMesh/SetBypassMaterial 주입 + ScreenQuadStage 생성.
    /// @return ScreenQuadStage UPtr — caller 가 mStages.push_back 책임 (D-2 Pure factory).
    std::unique_ptr<ScreenQuadStage> SetupDefaultPipeline(
        ResourceRegistry& reg,
        SceneRenderer& sceneRenderer,
        Framebuffer* sceneFB,
        const DefaultPipelineConfig& cfg = {});

    /// @brief PostFX 한 단계의 셰이더 + 초기 uniform 값.
    struct PostFXStageConfig
    {
        std::string Name;
        std::string VertFile;
        std::string FragFile;
        std::unordered_map<std::string, float> InitFloats; // D-6 data-driven (gamma=1.0 등)
    };

    /// @brief PostFX 체인 빌드 결과.
    struct PostFXChainResult
    {
        std::vector<FramebufferUPtr>          Framebuffers;   // owner — caller 가 멤버 vector 로 보유
        std::vector<Scene::PassComponent*>    PassComponents; // 비소유 raw — Debug UI 참조용
    };

    /// @brief PostFX 체인 빌드 — 각 stage 의 Program/Material/FB 생성
    ///        + PassActor(Layer::Screen) + AddComponent<PassComponent>(prev, fb, mat)
    ///        + screenCamActor->AddChild.
    /// @details 첫 stage 의 InputFB = sceneFB, 이후 stages 는 prev 단계의 OutputFB.
    ///          configs 의 각 element 의 InitFloats 가 Material::Properties::Floats 에 복사.
    PostFXChainResult BuildPostFXChain(
        ResourceRegistry& reg,
        Scene::Actor& screenCamActor,
        const std::vector<PostFXStageConfig>& configs,
        Framebuffer* sceneFB,
        int fbWidth, int fbHeight);
}

#endif // __SJH_RENDER_PIPELINE_H__
```

### 4.4 compound_actor.h 추가 선언

```cpp
namespace SJH::Scene
{
    // (기존) CreateCameraActor / CreateLightActor 등

    /// @brief PostFX 2-Camera 패턴의 Orthographic ScreenCamera Actor 생성.
    /// @details IsOrthographic=true + OrthoSize=1.0 + NearZ=-1 + NoClear=true
    ///          + CullingMask(UI|Screen) + SetTargetRenderTarget(sceneFB).
    /// @return Actor UPtr — caller 가 Director::Root().AddChild 책임.
    ActorUPtr CreateScreenCameraActor(const std::string& name, float aspect, Framebuffer* sceneFB);

    /// @brief Skybox Actor 생성 — Box mesh + 큰 scale + MeshRenderer.
    /// @details 카메라 따라가기는 *셰이더 측* (vert shader 가 view matrix translation 제거)
    ///          으로 자동 처리. SyncSkyboxToCamera 자유 함수 불필요.
    /// @return Actor UPtr — caller 가 dir.Root().AddChild 책임.
    ActorUPtr CreateSkyboxActor(Mesh* skyboxMesh, Material* skyboxMat, float scale = 50.0f);
}
```

### 4.5 의존성 그래프

```
SJH::common::window_helper.h    → GLFWwindow* (forward)
SJH::common::window_helper.cpp  → <GLFW/glfw3.h>
SJH::scene::compound_actor.h    → SJH::scene::actor, camera, layer + SJH::object (Mesh) + SJH::material + SJH::buffer (Framebuffer)
SJH::render::render_pipeline.h  → SJH::buffer (Framebuffer) + forward (ResourceRegistry, SceneRenderer, ScreenQuadStage, Scene::Actor, Scene::PassComponent)
SJH::render::render_pipeline.cpp → SJH::render (전체) + SJH::scene (Actor, PassComponent, Layer) + SJH::resource_registry + SJH::material
```

**Dependency Rule 검증** (SKILL):
- ✅ 모두 *inner module* (SJH::*) 거주
- ✅ main (Composition Root, outer) → SJH (inner) 단방향
- ✅ render_pipeline → render/scene/resource_registry (동일 layer 안의 helper, 위반 아님)
- ✅ Composition Root 는 main.cpp 유지 — bootstrap 모듈로 inner 거주 회피

---

## 5. ddd Rules 정합성 (SKILL 기준)

| Rule | 평가 | 비고 |
|------|------|------|
| **Separation of Concerns** | ✅ | 각 자유 함수 단일 책임 (ScreenCamera factory, Skybox factory, RenderPipeline init, PostFX chain build) |
| **Domain-Specific Naming** | ✅ | `SetupDefaultPipeline` / `BuildPostFXChain` / `CreateScreenCameraActor` — 의도 명시. `Manager`/`Helper`/`Utils` 회피 |
| **Explicit Side Effects** | ✅ | 각 함수 doxygen 이 *무엇을 등록/생성/주입* 하는지 명시. SetupDefaultPipeline 의 *SceneRenderer 에 주입* 부분 명시 |
| **Explicit Data Flow** | ✅ | Pure factory — 입력 인자 + 반환값으로 데이터 흐름 명확. 숨은 멤버 변수 의존 0 |
| **POLA (Principle of Least Astonishment)** | ✅ | compound_actor.h 의 기존 `Create*` 컨벤션 (ActorUPtr 반환) 일치. render_pipeline 의 `Setup*/Build*` 동사 분리 |
| **Library-First** | ✅ | 자체 구현 0 — 기존 ResourceRegistry / SceneRenderer / ScreenQuadStage 활용 |
| **Function/File Size Limits** | ✅ | render_pipeline.cpp ~100줄, compound_actor.cpp +35줄. 함수당 < 30줄 |
| **Early Return Pattern** | ✅ | BuildPostFXChain 의 `if (!prog) continue;` / `if (!fb) continue;` 패턴 유지 |
| **Single Source of Truth** | ✅ | sceneFB 는 *caller* (main.cpp) 의 단일 멤버. 각 자유 함수가 인자로 받음 — 두 진실의 원천 충돌 회피 |
| **Call-Site Honesty** | ✅ | spdlog::error 진단 (셰이더 로드 실패 / FB 생성 실패) 유지 — 호출자가 디버깅 가능 |

### Anti-pattern 회피 검증 (SKILL line 131~141)

| Anti-pattern | 본 SP 대응 |
|---|---|
| Anemic Domain Model | N/A (게임 도메인 모델은 본 SP 무관) |
| Repository per Entity | N/A |
| **God Module** (SKILL 의 God Aggregate 모듈 버전) | ✅ 회피 — bootstrap 모듈 신설 안 함 (옵션 2 거부 근거) |
| **Premature module split** | ✅ 회피 — render_pipeline 자체 STATIC 분리 안 함 (옵션 3 거부 근거) |
| **Composition Root in inner module** | ✅ 회피 — main.cpp 가 Composition Root 유지 |
| Leaking Infrastructure | ✅ Engine 함수가 game 한정 정보 (BGM event 이름 등) 받지 않음 |

---

## 6. 손대지 않는 것 (Out of Scope)

| 항목 | 이유 |
|---|---|
| **WramupFMOD / WramupPlayer** | 게임 도메인 (PlayerActorConfig, BGM event 이름) — Client 한정 |
| **WarmupSkybox 의 matrix_skybox shader/texture 키 + u_time 갱신** | 이 데모 한정 effect. A5 는 *factory + follow* 만 추출 |
| **CreateAndRegisterWorldCamera** | TargetFollowableCameraController + mMouse + 초기 transform — 게임 카메라 의도 |
| **B 그룹 (ImGui init / PostFXDebug entries factory)** | 별도 SP 후보. B3 의 *데이터 driven 측 (PostFXStageConfig.InitFloats)* 만 본 SP. UI 측 (PostFXDebugLayer ctor) 은 보존 |
| **migrate_demo / audio_demo** | 본 패턴 사용 안 함. 영향 0 |
| **새 STATIC 모듈 생성** | D-1 — 옵션 1 채택. 기존 모듈 확장만 |
| **단위 테스트 추가** | `no_auto_tests` 정책 — 시각 회귀로 검증 |
| **mScreenQuadStagePtr = nullptr 초기화** | ParticleStage SP final review 의 기존 Important 1건 — 별도 cleanup commit |

---

## 7. 검증

### 7.1 시각 회귀 시나리오 (T4 시점)

본 SP 의 *시각 결과 = 변경 전과 100% 동일* (보일러 추출 = 행위 보존).

| # | 시나리오 | 기대 결과 |
|---|---|---|
| V1 | `_MyApp_` 빌드 + 실행 | 컴파일 + 런타임 정상 |
| V2 | 좌클릭 → distortion 파티클 발사 | 변경 전과 동일 표시 |
| V3 | gamma 슬라이더 0.5 | 화면 + 파티클 어두워짐 (PostFX 통합 보존) |
| V4 | sobel 토글 ON | 파티클까지 윤곽선 검출 |
| V5 | invert 토글 ON | 파티클 색까지 반전 |
| V6 | blurring 토글 ON | 블러 처리 |
| V7 | sharpening 토글 ON | 샤프닝 |
| V8 | 전체 PostFX OFF | sceneFB → ScreenQuadStage 직접 blit (변경 전 동일) |
| V9 | ImGui PostFXDebug 패널 (좌측 (20,140)) | 5 행 체크박스 토글 + gamma 슬라이더 동작 |
| V10 | Skybox 따라가기 — WASD 이동 시 | Skybox 가 카메라 따라옴 (셰이더 측 view translation 제거 — 자유 함수 무관) |
| V11 | 창 리사이즈 (드래그) | sceneFB / DefaultRenderTarget / Camera.Aspect 재생성. PassComponent FB 는 *재생성 안 함* (기존 제약 유지) |

### 7.2 런타임 진단 로그 (변화 없음)

- spdlog::error "[PassComponent] 셰이더 로드 실패" — 셰이더 경로 오타 시 (현재 정상)
- spdlog::error "[PassComponent] FB 생성 실패" — Framebuffer::Create 실패 시 (드물음)

### 7.3 컴파일 검증

- `cmake --preset ninja` reconfigure (새 .cpp 파일 인식)
- `cmake --build --preset ninja --target _MyApp_` — Debug 빌드 성공
- `cmake --build --preset ninja-release --target _MyApp_` — Release 빌드 성공
- migrate_demo / audio_demo — 영향 0, 기존 빌드 결과 동일

---

## 8. 정통 매핑 (SKILL Where-to-go 결정 트리)

| 함수 | "Where does this code go?" 결과 | 거주지 |
|---|---|---|
| `GetFramebufferInfo(window)` | "Talks to external systems" (GLFW) | infrastructure/common (`SJH::common`) |
| `CreateScreenCameraActor` | "Pure business logic" (Actor composition) | domain/scene (`SJH::scene`) |
| `CreateSkyboxActor` | 동일 | domain/scene |
| `SyncSkyboxToCamera` | "Pure business logic" (Transform 갱신) | domain/scene |
| `SetupDefaultPipeline` | "Orchestrates domain + has side effects" (ResourceRegistry 등록 + SceneRenderer 주입) | application/render (`SJH::render`) |
| `BuildPostFXChain` | 동일 | application/render |

**main.cpp** = Composition Root (SKILL line 113 의 "main / Bootstrap / entry point").

---

## 9. 알려진 이슈 / Future Work

### 9.1 B 그룹 (UI helper 분리)

- `UI::ImGuiBootstrap::Init(window)` — sb7 + ImGui v1.53 init 5줄 보일러 캡슐화
- `UI::PostFXDebugLayer::BuildFromConfigs(configs, passComponents, gamma)` — entries 빌드 루프 factory
- 별도 SP 후보 — B 그룹 전체 분리

### 9.2 CreateWorldCameraActor 분리

- TargetFollowableCameraController 부착 + mMouse setUp + 초기 transform — Client helper 후보
- 다른 게임 데모도 같은 패턴 사용 시 분리 가치 ↑

### 9.3 render_pipeline 모듈 승격

- PostFX 가 Bloom/HDR/SSAO 등으로 복잡해질 때 — 자체 STATIC 모듈로 분리
- 현재는 자유 함수 family 로 충분 (architecture.md §1 "반복 패턴 두 군데 이상 등장 후" 트리거 대기)

### 9.4 Skybox FollowComponent 화

- `SyncSkyboxToCamera` 자유 함수 → `SkyboxFollowComponent : Scene::Component` 진화
- render() 의 매 프레임 호출 → Component Update hook 자동
- 단 *현재 main.cpp 가 명시 호출* 가독성 좋음 — 진화 미루기 OK

### 9.5 mPostFXFBs 타입 변경의 잠재 영향

- `array<5>` → `vector` — heap allocation 추가. 시각 영향 0, 성능 측정 가능한 차이 0 (5개 element).
- vector 가 *configs 가변 size* 대응 — 다음 PostFX 단계 추가 시 array 크기 변경 안 함.

---

## 10. 결정 로그 요약

| ID | 결정 | 채택 | 영향 |
|----|------|-----|------|
| D-1 | 보일러 거주 모듈 전략 | 옵션 1 (기존 모듈 확장 + 새 헤더 1) | Composition Root 보존, YAGNI, SKILL 9/10 |
| D-2 | 자유 함수 시그니처 | Pure factory (caller wiring) | compound_actor.h 컨벤션 일치 |
| D-3 | A3+A4 거주 | `src/render/render_pipeline.{h,cpp}` 새 헤더 | render 모듈 안 자유 함수 family, 별도 STATIC 분리 안 함 |
| D-4 | A2+A5 거주 | `src/scene/compound_actor.h` 확장 | PreBuilt Actor factory 컨벤션 |
| D-5 | A1 거주 + glfw 격리 | `src/common/window_helper.{h,cpp}` + GLFWwindow* forward | SJH::common 본체 glfw 의존 회피 |
| D-6 | B3 gamma data-driven 범위 | PostFXStageConfig.InitFloats 만 본 SP | UI helper (B1/B2) 는 별도 SP |

---

## 11. 후속 작업

- `writing-plans` skill 진입 — 본 spec 기반 implementation plan 작성 (`doc/superpowers/plans/2026-05-31-warmup-boilerplate-extraction-impl.md`)
- Plan 단위 Task 5 개 예상: T1 (A1 window_helper) → T2 (A2+A5 compound_actor 확장) → T3 (A3+A4 render_pipeline) → T4 (main.cpp 통합) → T5 (시각 회귀 후 finishing)

---

**spec 끝**.
