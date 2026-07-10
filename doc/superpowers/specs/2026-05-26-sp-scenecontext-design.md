# SP-SceneContext + ProgramRegistry — Cocos2D Aggregate Root 정통 합류 + Program 매 프레임 DFS 폐기

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **상태**: design (brainstorming 단락별 합의 완료, plan 미작성)
> **브랜치**: `game/module/rendertarget`
> **기준 HEAD**: `6c6c243 [dev] : material eager delete`
> **분할 SP 2종 중 #1**: SP-SceneContext+ProgramRegistry (본 문서, SP-ProgramRegistry 흡수) → SP-UniversalRT
> **시각 회귀 대상**: `migrate_demo` + `_MyApp_`
> **참조 메모리**:
> - `doc/handoffs/2026-05-26/2026-05-26-render-refactor-session.md` — 이전 세션 종합 메모리 (R1~R6 채택 / X1~X5 거부)
> - `.claude/CLAUDE.md` — 자원 보유 컨벤션 §11.3 + 모듈 레이아웃
> - Context7 4 엔진 리서치 (Unity SRP / Unreal FScene / Godot RenderingServer / Cocos2D-x v4 `Scene::_cameras`)
> - **재결정 노트 (2026-05-26 second brainstorm)**: 본 spec 은 *원본 SP-SceneContext spec* 위에 patch — SP-ProgramRegistry 흡수, `MAX_*_LIGHTS=16` 통일, migrate_demo 활성 유지, Phase 4.5 신설, Phase 당 1 commit 채택

---

## §1. 목적과 동기

**핵심 동기 5종** (사용자 제기, 4 엔진 정통 검증 완료):

| # | 동기 | 4 엔진 정통 부합 |
|---|---|---|
| 1 | `SceneRenderer::Render(target, view, proj)` 명시 오버로드 폐기 — 단위 테스트 호출처 0건 grep 확인 | 부분 정합 (Unity manual `ScriptableCullingParameters` 옵션은 보존, 본 프로젝트는 사용처 0이므로 완전 폐기 안전) |
| 2 | Camera 매 프레임 DFS 수집 (`CollectCameras`) 폐기 → `SceneContext` 등록 시 push | **4 for 3** (Unity 만 culling 시스템 별도, Unreal/Godot/Cocos 모두 *영구 등록*) |
| 3 | Camera RT non-null 강제 (`SetTargetRenderTarget(nullptr)` assert) | **4 for 4** — SP-UniversalRT 영역으로 분할 |
| 4 | Light 매 프레임 DFS 수집 (`CollectLights`) 폐기 → `SceneContext` 등록 시 push | **4 for 3** (Unreal `FScene::Lights` / Cocos `Scene::_lights` 정통) |
| 5 | Program 매 프레임 DFS 수집 (`CollectPrograms`) 폐기 → `ResourceRegistry::GetAllPrograms()` 신설 | **4 for 4** (Unity `ShaderManager` / Unreal `GShaderManager` / Godot `RenderingServer.shader_create` / Cocos `GLProgramCache`) — **본 SP 영역 *내* (재결정)** — `ResourceRegistry::CreateProgram/FindProgram` 이 이미 존재, `GetAllPrograms()` 1 메서드만 신설하면 DFS 한 줄 대체 가능 |

**본 SP 범위**: 동기 #1·#2·#4·**#5** (SP-ProgramRegistry 흡수, 사용자 P4-a′ 결정). 동기 #3 = SP-UniversalRT (별도).

**4 엔진 정통 합류 형태**: Cocos2D-x v4 의 `Scene::_cameras` / `Scene::_lights` Aggregate Root 패턴.

| 엔진 | Camera Aggregate | Light Aggregate |
|---|---|---|
| Unity SRP | `Camera.allCameras` (정적) | `CullingResults.visibleLights` (per-cull) |
| Unreal | `FScene::Cameras` (영구) | `FScene::Lights` (영구) — actor spawn 시 `RegisterComponent` → `FLightSceneInfo` 영구 보관 |
| Godot | `Scenario` RID 기반 | `Scenario` RID 기반 — `RenderingServer.instance_set_scenario` |
| **Cocos2D-x** | **`Scene::_cameras` vector** | **`Scene::_lights` vector** — `Scene::addChild(camera/light)` 시 push |

본 프로젝트는 *Cocos `Scene::_cameras`* 패턴에 가장 가까움 — `Scene::Director` Singleton + `Actor` 트리 + Component lifecycle (`OnEnter`/`OnExit`) 이 이미 정통.

---

## §2. 거부 결정 (사용자 명시 + 4 엔진 정통 근거)

| 거부 | 근거 |
|---|---|
| ❌ `Material` + `Light` 공통 `Apply(prog)` 인터페이스 | **4 for 4 부재** — Unity/Unreal/Godot/Cocos 모두 *공통 base class 없음*. LSP 부재 + Bounded Context 분리 (`Material` = 표면 외형 / `Light` = 씬 조명). Sandi Metz "duplication is far cheaper than the wrong abstraction". (이전 세션 §X3 재확인) |
| ❌ Generic `SceneContext::Register<T>(T*)` template | API 이중 구조 (`specific Add/AddLight` + `generic Register`) 가 군더더기. 4 엔진 모두 *타입별 명시 메서드* (Cocos `_cameras.push` / `_lights.push`). |
| ❌ Component base 에 `OnRegisterScene(SceneContext&)` virtual hook + DI | Dependency Injection 이론은 OK 이나 *4 엔진 모두 Component 가 Director/Server 를 직접 참조* — 정통과 어긋남. YAGNI 위반 (현재 Component 1~4개만 Scene 의존). |
| ❌ `Camera.Depth` 보존 + sort | 본 프로젝트의 `Camera.Depth` 가 두 의미 혼용 — (i) 진짜 다중 view 우선순위 (Unity 정통) (ii) PostFX 체인 단계 인덱스 (misuse). 4 엔진 정통은 *PostFX = 비-카메라 Pass list* (Unity URP `RenderPassEvent` enum + `EnqueuePass` / Unreal `BlendableLocation` + `BlendablePriority` / Godot `CompositorEffect` Array[RID] / Cocos 3.x Custom Pipeline `addRenderPass` 순서). 본 SP 는 *Cocos2D `addChild` 순서* 정통 채택 — `Depth` 폐기. PostFXPass 도입은 별도 SP. |
| ❌ `Director::mActiveCamera` 슬롯 보존 | `GetActiveCamera()` 호출처 grep 결과 **0건** 확인. setter 만 5곳에서 호출 — *dead code*. SceneContext 가 *상위 대체*. |
| ❌ `SceneContext` 가 `Program` 도 보유 | `Program` Aggregate 는 `ResourceRegistry::mPrograms` 가 이미 owner (CreateProgram/FindProgram 도 이미 존재). SceneContext 가 *중복 보유* 할 필요 없음 — `ResourceRegistry::GetAllPrograms()` 1 메서드 신설로 충분. |
| ❌ 공통 Apply 인터페이스 — `Material::Apply(prog)` + `Light::Apply(prog)` 통일 | 4 for 4 부재 (§X3 재확인). Material/Light 의 표면적 공통점은 "Program 에 송신" 뿐 — Bounded Context 분리 + LSP 부재. |

---

## §3. Phase 분할 — 7 단계 (Phase 4.5 신설 — SP-ProgramRegistry 흡수)

| Phase | 범위 | 추정 변경 파일 | commit |
|---|---|---|---|
| **P1** | `Scene::SceneContext` 신규 클래스 + `Camera::Depth` 필드 폐기 | `src/scene/scene_context.{h,cpp}` (신규), `src/scene/camera.h` | 1 |
| **P1.5** | 셰이더 컨벤션 확장 — `MAX_POINT_LIGHTS=16` + `MAX_SPOT_LIGHTS=16` + `spotLights[]` 배열화 + Const 이름 정정 (NUM_*→MAX_*) | 셰이더 3개 (`lighting.fs`/`phong_color.fs`/`phong_tex.fs`), `src/common/constants.h`, `<src>/render/scene_renderer.cpp` `SendLightUniforms` | 1 |
| **P2** | `Director` 갱신 — `mContext` 값 멤버 + `mActiveCamera`/Set/Get + `Camera` forward decl 완전 삭제 | `src/scene/scene.h`, `src/scene/scene.cpp` | 1 |
| **P3** | Camera/DirLight/PointLight/SpotLight 의 `OnEnter`/`OnExit` 본문 채움 (.cpp 로 이동) | `src/scene/camera.cpp`, `src/scene/light.cpp` (신규 또는 기존 확장) | 1 |
| **P4** | `SceneRenderer` 슬림다운 — DFS 폐기 (Cam/Light/Program 셋 모두) + `std::sort` 폐기 + 명시 `Render(t,v,p)` 폐기 + render-time `IsEnabled` filter | `src/render/scene_renderer.{h,cpp}` | 1 |
| **P4.5** | **신설 (SP-ProgramRegistry 흡수)** — `ResourceRegistry::GetAllPrograms()` 1 메서드 추가 → SceneRenderer 의 `CollectPrograms` DFS → 한 줄 대체 | `src/resource_registry/resource_registry.{h,cpp}`, `<src>/render/scene_renderer.cpp` | 1 |
| **P5** | 데모 cleanup (활성 데모만) — `migrate_demo` 3 줄 (Depth=0/Depth=i+1/SetActiveCamera) + `_MyApp_` 2 줄 (SetActiveCamera) | `<apps>/migrate_demo/main.cpp`, `apps/_MyApp_/main.cpp` | 1 |
| **P6** | 시각 회귀 게이트 — `migrate_demo` + `_MyApp_` 시각 동일 + 콘솔 warn 0 + Catch2 단위 테스트 21개 pass | — (검증 단계) | — |

---

## §4. Phase 1 — `SceneContext` 신규 클래스 + `Camera::Depth` 폐기

### 4.1 신규 파일 — `src/scene/scene_context.h`

```cpp
#ifndef __SJH_SCENE_CONTEXT_H__
#define __SJH_SCENE_CONTEXT_H__

#include <vector>

namespace SJH       { class DirLight; class PointLight; class SpotLight; }
namespace SJH::Scene { class Camera; }

namespace SJH::Scene
{
    /// @brief Cocos2D-x v4 `Scene::_cameras` / `Scene::_lights` 정통 Aggregate Root.
    /// @details
    ///   ### 책임
    ///   - 씬에 등록된 Camera / Light Component 의 *비소유 raw 포인터* 컬렉션 보관
    ///   - Component 의 `OnEnter` / `OnExit` 시점에 자동 push/pop (Cocos2D `addChild` 정통)
    ///   - SceneRenderer 가 매 프레임 DFS traverse 하던 책임을 *등록 시점에 흡수* (4 엔진 4 for 3)
    ///
    ///   ### 비-책임
    ///   - ❌ Program / Material / Mesh 보유 — SP-ProgramRegistry / Material owner 영역
    ///   - ❌ 활성/비활성 filter — 보관은 *모두*, render-time filter 는 SceneRenderer 책임
    ///   - ❌ Camera 우선순위 정렬 — `Camera::Depth` 폐기 (Cocos2D `addChild` 순서 정통). PostFX 체인 순서는
    ///        AddCamera 호출 순서로 자연 보장. 진짜 다중 view 우선순위가 필요한 시점에 *별도 SP-PostFXPass*
    ///        또는 *enum 기반 stage* 도입 (4 엔진 정통).
    ///
    ///   ### Lifetime 가정
    ///   - 모든 Component 의 owner 는 Actor — Actor::~Actor 가 mComponents 소멸 시 Component::~ 호출.
    ///     단 `Component::OnExit` 의 명시 호출이 *없으면* SceneContext 에 dangling 가능 — Actor 의
    ///     OnExit/destructor 가 OnExit 보장하므로 정통 라이프사이클 준수 시 안전.
    class SceneContext
    {
    public:
        // ── Camera (Cocos2D `Scene::_cameras` 정통) ───────────────────────
        /// @brief Camera Component 등록. 중복 시 assert (Component::OnEnter 가 contract 상 1회 호출 보장).
        void AddCamera(Camera *cam);

        /// @brief Camera Component 해제. 미등록 erase 는 no-op (idempotent).
        void RemoveCamera(Camera *cam);

        /// @brief 등록 순서대로 (Cocos `addChild` 정통) Camera 컬렉션 반환. enabled filter 는 소비자 책임.
        const std::vector<Camera *> &GetCameras() const { return mCameras; }

        // ── DirLight 단일 슬롯 (셰이더 컨벤션: dirLight 단일) ───────────────
        /// @brief DirLight 등록. 이미 있으면 reject + spdlog::warn (셰이더가 첫 1개만 지원).
        void AddLight(SJH::DirLight *light);
        void RemoveLight(SJH::DirLight *light);
        SJH::DirLight *GetDirLight() const { return mDirLight; }

        // ── PointLight vector (max 16 = Const::MAX_POINT_LIGHTS) ──────────
        /// @brief PointLight 등록. MAX_POINT_LIGHTS 초과 시 reject + spdlog::warn.
        void AddLight(SJH::PointLight *light);
        void RemoveLight(SJH::PointLight *light);
        const std::vector<SJH::PointLight *> &GetPointLights() const { return mPointLights; }

        // ── SpotLight vector (max 16 = Const::MAX_SPOT_LIGHTS) ────────────
        /// @brief SpotLight 등록. MAX_SPOT_LIGHTS 초과 시 reject + spdlog::warn.
        void AddLight(SJH::SpotLight *light);
        void RemoveLight(SJH::SpotLight *light);
        const std::vector<SJH::SpotLight *> &GetSpotLights() const { return mSpotLights; }

    private:
        std::vector<Camera *>            mCameras;
        SJH::DirLight                   *mDirLight = nullptr;
        std::vector<SJH::PointLight *>   mPointLights;   // reserve(Const::MAX_POINT_LIGHTS) = 16
        std::vector<SJH::SpotLight *>    mSpotLights;    // reserve(Const::MAX_SPOT_LIGHTS) = 16
    };
}

#endif // __SJH_SCENE_CONTEXT_H__
```

### 4.2 신규 파일 — `src/scene/scene_context.cpp`

- `AddCamera` — `assert(std::find(mCameras.begin(), mCameras.end(), cam) == mCameras.end())` 후 push_back
- `RemoveCamera` — `std::erase(mCameras, cam)` (C++20 이지만 본 프로젝트 C++17 → `mCameras.erase(std::remove(...), mCameras.end())`)
- `AddLight(DirLight*)` — `if (mDirLight) { spdlog::warn(...); return; } else mDirLight = light;`
- `AddLight(PointLight*)` — `if (mPointLights.size() >= Const::MAX_POINT_LIGHTS) { spdlog::warn(...); return; } else mPointLights.push_back(light);`
- `AddLight(SpotLight*)` — `if (mSpotLights.size() >= Const::MAX_SPOT_LIGHTS) { spdlog::warn(...); return; } else mSpotLights.push_back(light);`
- `RemoveLight` 3 overload — 동일 erase 패턴 (Dir 은 `if (mDirLight == light) mDirLight = nullptr;`)
- ctor 에서 `mPointLights.reserve(Const::MAX_POINT_LIGHTS); mSpotLights.reserve(Const::MAX_SPOT_LIGHTS);` (allocation 1회)
- `#include "common/constants.h"` + `#include <<spdlog>/spdlog.h>` + `#include "object/light.h"` + `#include "scene/camera.h"` + `#include <algorithm>`

### 4.3 `Camera::Depth` 폐기 — `src/scene/camera.h`

```cpp
class Camera : public Component {
    float FovYDeg = 45.0f;
    float Aspect  = 16.0f / 9.0f;
    float NearZ   = 0.1f;
    float FarZ    = 100.0f;
    // ❌ int Depth = 0;       — 폐기 (Cocos2D `addChild` 순서 정통)
    uint64_t CullingMask = SJH::Scene::ToBits(SJH::Scene::Layer::All);
    // ... 나머지 동일
    // ❌ bool operator<(const Camera &other) const { return Depth < other.Depth; }  — Depth 기반 비교 폐기
};
```

`operator<` 도 같이 폐기 (호출처 = `SceneRenderer::Render` 의 `std::sort` 람다 한 곳, Phase 4 에서 제거).

### 4.4 `src/scene/CMakeLists.txt` 갱신

`scene_context.cpp` 를 STATIC 라이브러리 소스 목록에 추가.

---

## §5. Phase 1.5 — 셰이더 컨벤션 확장

### 5.1 `src/common/constants.h`

```cpp
// 셰이더 schema 와 1:1 — lighting.fs / phong_color.fs / phong_tex.fs 의 #define
inline constexpr int MAX_POINT_LIGHTS = 16;  // 변경: NUM_POINT_LIGHTS=2 → MAX_POINT_LIGHTS=16 (이름 정정 + 값)
inline constexpr int MAX_SPOT_LIGHTS  = 16;  // 신설

// 배열 uniform — prefix + 인덱스 + STR_INDEX_CLOSE 조합
inline constexpr auto UNI_POINT_LIGHTS_PREFIX         = "pointLights[";
inline constexpr auto UNI_POINT_LIGHTS_ENABLED_PREFIX = "pointLightsEnabled[";
inline constexpr auto UNI_SPOT_LIGHTS_PREFIX          = "spotLights[";          // 신설
inline constexpr auto UNI_SPOT_LIGHTS_ENABLED_PREFIX  = "spotLightsEnabled[";   // 신설

// ❌ inline constexpr auto UNI_SPOT_LIGHT         = "spotLight";         — 폐기
// ❌ inline constexpr auto UNI_SPOT_LIGHT_ENABLED = "spotLightEnabled";  — 폐기
// ❌ inline constexpr int  NUM_POINT_LIGHTS      = 2;                   — 폐기 (MAX_POINT_LIGHTS 로 정정)
```

**이름 정정 근거** (사용자 P1.5-b 결정): `NUM_*` 은 *고정 개수* 의미로 읽힘 — 실제로는 *최대 개수* (런타임 enabled 0~N 가변) 라서 `MAX_*` 가 정확. 셰이더 `#define` 도 함께 정정.

### 5.2 셰이더 3개 — `lighting.fs` / `phong_color.fs` / `phong_tex.fs`

**공통 변경 패턴** (3개 모두 동일):

```glsl
// 변경 전
#define NUM_POINT_LIGHTS 2
uniform SpotLight spotLight;
uniform int spotLightEnabled;
// main(): if (spotLightEnabled != 0) result += CalcSpotLight(spotLight, ...);

// 변경 후
#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS  16
uniform PointLight pointLights      [MAX_POINT_LIGHTS];
uniform int        pointLightsEnabled[MAX_POINT_LIGHTS];
uniform SpotLight  spotLights       [MAX_SPOT_LIGHTS];
uniform int        spotLightsEnabled[MAX_SPOT_LIGHTS];
// main():
//     for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
//         if (pointLightsEnabled[i] != 0)
//             result += CalcPointLight(pointLights[i], pixelNorm, fragPos, viewDir);
//     }
//     for (int i = 0; i < MAX_SPOT_LIGHTS; ++i) {
//         if (spotLightsEnabled[i] != 0)
//             result += CalcSpotLight(spotLights[i], pixelNorm, viewDir);
//     }
```

**GLSL fragment uniform 한도 안전성**: GL 4.1 의 `GL_MAX_FRAGMENT_UNIFORM_VECTORS` 가 1024 보장. PointLight(8 vec4) × 16 + SpotLight(10 vec4) × 16 + 기타 ≈ 300 vec4 — 안전 마진 큼.

### 5.3 `SendLightUniforms` 확장 — `<src>/render/scene_renderer.cpp`

PointLights 루프 패턴 (이미 존재) 을 SpotLights 에도 적용:

```cpp
// 변경 전 (단일 spotLight)
if (spot) {
    Uniforms::SetSpotLight(*prog, "spotLight", *spot, ...);
    Uniforms::SetInt(*prog, "spotLightEnabled", 1);
} else {
    Uniforms::SetInt(*prog, "spotLightEnabled", 0);
}

// 변경 후 (spotLights[i] 루프 — PointLights 와 동일 패턴)
for (std::size_t i = 0; i < static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS); ++i)
{
    const std::string idxStr = Const::UNI_SPOT_LIGHTS_PREFIX
        + std::to_string(i) + Const::STR_INDEX_CLOSE;
    const std::string enStr = Const::UNI_SPOT_LIGHTS_ENABLED_PREFIX
        + std::to_string(i) + Const::STR_INDEX_CLOSE;
    if (i < spots.size())
    {
        Uniforms::SetSpotLight(*prog, idxStr.c_str(), *spots[i],
                               spots[i]->GetWorldPosition(),
                               spots[i]->GetWorldDirection());
        Uniforms::SetInt(*prog, enStr.c_str(), 1);
    }
    else
    {
        Uniforms::SetInt(*prog, enStr.c_str(), 0);
    }
}
```

PointLights 루프도 `Const::NUM_POINT_LIGHTS=2` → `Const::MAX_POINT_LIGHTS=16` 으로 자동 확대 (이름 정정 + 값 변경, 루프 본문 동일 — 부족 슬롯은 enabled=0 으로 채움). 부족 슬롯 enabled=0 가드가 셰이더 본문의 `if (pointLightsEnabled[i] != 0)` 분기로 *시각 영향 0* 보장.

### 5.4 `SceneRenderer::CollectLights` 시그니처 정렬 (Phase 4 에서 어차피 폐기될 함수지만 P1.5 에서 미리 vector 화)

```cpp
// 변경 전
void CollectLights(const Scene::Actor &actor,
                   DirLight *&outDir,
                   std::vector<PointLight *> &outPoints,
                   SpotLight *&outSpot);

// 변경 후 (SpotLight 도 vector)
void CollectLights(const Scene::Actor &actor,
                   DirLight *&outDir,
                   std::vector<PointLight *> &outPoints,
                   std::vector<SpotLight *> &outSpots);
```

본 함수는 Phase 4 에서 *완전 폐기* 예정이므로 변경 최소 — vector 시그니처만 정렬, 본문도 outSpot 가드 폐기 후 vector push.

---

## §6. Phase 2 — `Director` 갱신

### 6.1 `src/scene/scene.h`

```cpp
#ifndef __SJH_SCENE_H__
#define __SJH_SCENE_H__

#include "scene/actor.h"
#include "scene/scene_context.h"   // NEW

namespace SJH::Scene
{
    /// @brief Cocos cc::Director 정통 — root Actor + SceneContext Aggregate Root 보유 싱글톤.
    class Director
    {
    public:
        static Director &Get();

        Actor       &Root()       { return mRoot; }
        const Actor &Root() const { return mRoot; }

        void Enter()             { mRoot.OnEnter(); }
        void Exit()              { mRoot.OnExit(); }
        void Update(float dt)    { mRoot.Update(dt); }

        SceneContext       &GetContext()       { return mContext; }   // NEW
        const SceneContext &GetContext() const { return mContext; }   // NEW

        // ❌ void    SetActiveCamera(Camera *cam);  — 폐기
        // ❌ Camera *GetActiveCamera() const;       — 폐기

        Director(const Director &)            = delete;
        Director &operator=(const Director &) = delete;
        Director(Director &&)                 = delete;
        Director &operator=(Director &&)      = delete;

    private:
        Director() : mRoot("WorldRoot") {}
        ~Director() = default;

        Actor        mRoot;
        SceneContext mContext;   // NEW — 값 멤버 (Director Singleton lifetime = SceneContext lifetime)
        // ❌ Camera *mActiveCamera = nullptr;       — 폐기
    };
}

#endif // __SJH_SCENE_H__
```

`class Camera;` forward decl 도 제거 (Set/GetActiveCamera 폐기로 불필요).

---

## §7. Phase 3 — Component lifecycle hook 연결

### 7.1 `src/scene/camera.cpp` (또는 camera.h 인라인)

```cpp
#include "scene/camera.h"
#include "scene/scene.h"    // Director::Get() + GetContext()

namespace SJH::Scene
{
    void Camera::OnEnter()
    {
        Director::Get().GetContext().AddCamera(this);
    }

    void Camera::OnExit()
    {
        Director::Get().GetContext().RemoveCamera(this);
    }
}
```

기존 `Update(float)` 빈 구현은 그대로.

### 7.2 `src/scene/light.cpp` (또는 light.h 인라인 — `#include "scene/scene.h"` 추가)

```cpp
#include "object/light.h"
#include "scene/scene.h"    // Director::Get() + GetContext()

namespace SJH
{
    void DirLight::OnEnter()   { Scene::Director::Get().GetContext().AddLight(this); }
    void DirLight::OnExit()    { Scene::Director::Get().GetContext().RemoveLight(this); }

    void PointLight::OnEnter() { Scene::Director::Get().GetContext().AddLight(this); }
    void PointLight::OnExit()  { Scene::Director::Get().GetContext().RemoveLight(this); }

    void SpotLight::OnEnter()  { Scene::Director::Get().GetContext().AddLight(this); }
    void SpotLight::OnExit()   { Scene::Director::Get().GetContext().RemoveLight(this); }
}
```

### 7.3 Circular include 회피

- `scene_context.h` 는 `Camera` / `DirLight` / `PointLight` / `SpotLight` *forward decl* 만 (포인터 멤버 + 참조 인자라 가능)
- `scene_context.cpp` 가 *full include* (`scene/camera.h`, `object/light.h`)
- `camera.cpp` / `light.cpp` 가 `scene/scene.h` full include — Director Singleton 접근
- `scene.h` 는 `scene_context.h` full include — `SceneContext mContext` 값 멤버 (size 필요)

순환 의존 0건 확인.

### 7.4 안전성 가정 (spec §위험 절에 기록)

- `Actor::~Actor()` 가 `mComponents` 의 unique_ptr 소멸로 Component::~ 호출 — 단 `OnExit` 의 명시 호출은 *Actor::OnExit() 또는 RemoveComponent 시점에만* 발생 (actor.h:163-164).
- *시나리오*: Actor 가 OnEnter 후 OnExit 없이 ~Actor → Component::~ → SceneContext 에 dangling pointer.
- *완화*: `Actor::~Actor` 가 `if (mEntered) OnExit();` 같은 안전 가드를 *추가하면* OK. 단 본 SP 범위 외 — Actor lifecycle 자체 변경은 별도 결정. 현 안전 보장 = *호출자 컨벤션 (`RemoveChild` 전 OnExit 보장)*.

---

## §8. Phase 4 — `SceneRenderer` 슬림다운

### 8.1 `<src>/render/scene_renderer.h`

```cpp
class SceneRenderer : public IRenderStage
{
public:
    /// @brief 씬 트리의 SceneContext 에서 Camera 컬렉션 직접 조회 -> render-time IsEnabled filter -> 직렬 렌더.
    /// @details Camera 0 또는 모두 disabled → assert (Camera 는 항상 필수).
    void Render(RenderTarget &defaultTarget) override;

    // ❌ void Render(RenderTarget &defaultTarget, const mat4 &view, const mat4 &proj);  — 폐기 (호출처 0건)

private:
    void CollectFromActor(const Scene::Actor &actor, const mat4 &viewMat, uint64_t cullingMask);

    // ❌ void CollectCameras(...);  — 폐기 (SceneContext::GetCameras 가 대체)
    // ❌ void CollectLights(...);   — 폐기 (SceneContext::GetDirLight/GetPointLights/GetSpotLights 가 대체)
    // ❌ void CollectPrograms(...); — 폐기 (Phase 4.5 의 ResourceRegistry::GetAllPrograms() 가 대체)

    void RenderWithCamera(Scene::Camera &cam, RenderTarget &defaultTarget);

    void SendLightUniforms(const std::vector<Program *> &programs,    // ← unordered_set → vector (P4.5 GetAllPrograms() 반환 정합)
                           DirLight *dir,
                           const std::vector<PointLight *> &points,
                           const std::vector<SpotLight *> &spots,
                           const vmath::vec3 &viewPos);

    MeshPassProcessor mProcessor;
};
```

### 8.2 `<src>/render/scene_renderer.cpp` 의 `Render(RenderTarget&)`

```cpp
void SceneRenderer::Render(RenderTarget &defaultTarget)
{
    // 1. SceneContext 에서 Camera 컬렉션 직접 조회 — DFS 폐기.
    const auto &cameras = Scene::Director::Get().GetContext().GetCameras();
    assert(!cameras.empty() && "SceneRenderer::Render — SceneContext 에 Camera 미등록. "
                               "Camera 는 4 엔진 정통상 항상 필수.");

    // 2. render-time IsEnabled filter — 등록은 모두, 가시성만 filter (P4-b).
    //    Camera::Depth 기반 std::sort 폐기 — Cocos `addChild` 순서 정통 (P1-c′).
    for (auto *cam : cameras)
    {
        if (cam->IsEnabled())
            RenderWithCamera(*cam, defaultTarget);
    }
}
```

### 8.3 `RenderWithCamera` 의 Light 조회

```cpp
void SceneRenderer::RenderWithCamera(Scene::Camera &cam, RenderTarget &defaultTarget)
{
    auto &rc = DeviceContext::Get();

    RenderTarget &target = cam.GetTargetRenderTarget() ? *cam.GetTargetRenderTarget()
                                                       : defaultTarget;
    // (Camera RT non-null 강제는 SP-UniversalRT 영역 — 본 SP 는 fallback 유지)
    rc.BeginFrame(target);

    const auto viewMat     = cam.GetViewMatrix();
    const auto projMat     = cam.GetProjectionMatrix();
    const auto cullingMask = cam.CullingMask;

    vmath::vec3 viewPos(0.0f, 0.0f, 0.0f);
    if (auto *camOwner = cam.GetOwner())
    {
        const auto camWorld = camOwner->GetWorldMatrix();
        viewPos = vmath::vec3(camWorld[3][0], camWorld[3][1], camWorld[3][2]);
    }

    // SceneContext 에서 Light 직접 조회 — CollectLights DFS 폐기.
    auto &ctx = Scene::Director::Get().GetContext();

    // render-time IsEnabled filter (P4-b 일관) — DirLight 단일은 직접 체크.
    DirLight *dir = (ctx.GetDirLight() && ctx.GetDirLight()->IsEnabled())
                    ? ctx.GetDirLight() : nullptr;

    std::vector<PointLight *> points;
    points.reserve(ctx.GetPointLights().size());
    for (auto *l : ctx.GetPointLights())
        if (l->IsEnabled()) points.push_back(l);

    std::vector<SpotLight *> spots;
    spots.reserve(ctx.GetSpotLights().size());
    for (auto *l : ctx.GetSpotLights())
        if (l->IsEnabled()) spots.push_back(l);

    // Program 컬렉션 — Phase 4.5 의 ResourceRegistry::GetAllPrograms() 한 줄 대체 (DFS 폐기).
    auto programs = ResourceRegistry::Get().GetAllPrograms();

    SendLightUniforms(programs, dir, points, spots, viewPos);

    mProcessor.Clear();
    CollectFromActor(Scene::Director::Get().Root(), viewMat, cullingMask);
    mProcessor.SortMultiStage();
    mProcessor.Process(rc, viewMat, projMat);
}
```

### 8.4 폐기되는 함수

- `Render(RenderTarget&, view, proj)` 정의 + 선언 — *완전 삭제* (호출처 0건 grep 확인)
- `CollectCameras` 정의 + 선언 — *완전 삭제*
- `CollectLights` 정의 + 선언 — *완전 삭제*
- `CollectPrograms` 정의 + 선언 — *완전 삭제* (Phase 4.5 `ResourceRegistry::GetAllPrograms()` 대체)

### 8.5 SceneRenderer 책임 축소 결과

| 책임 | 변경 전 | 변경 후 |
|---|---|---|
| Camera 수집 + 정렬 | DFS + std::sort | `SceneContext::GetCameras()` 직접 조회 |
| Light 수집 | DFS | `SceneContext::Get{Dir,Point,Spot}Light(s)` 직접 조회 |
| Program 수집 | DFS | `ResourceRegistry::GetAllPrograms()` 직접 조회 (Phase 4.5) |
| DrawCommand 생성 | DFS | DFS (유지 — Cocos `Scene::render` 정통, MeshRenderer 가 Actor 트리에 분산) |
| Light uniform 송신 | 4 책임 중 하나 | 2 책임 중 하나 (Phase 2 LightUniformDispatcher 분리는 별도 SP) |

**SceneRenderer 의 DFS 는 1 종으로 축소** — *MeshRenderer 수집* 만 (Cocos `Scene::render` 가 노드 트리 traverse 하며 RenderCommand push 하는 정통 패턴 정합).

---

## §8.5. Phase 4.5 — `ResourceRegistry::GetAllPrograms()` 신설 (SP-ProgramRegistry 흡수)

### 8.5.1 Background — 이미 존재하는 부분

| 항목 | 현재 상태 |
|---|---|
| `ResourceRegistry::CreateProgram(key, vsPath, fsPath)` | **이미 존재** ([resource_registry.h:89-91](src/resource_registry/resource_registry.h#L89-L91)) |
| `ResourceRegistry::FindProgram(key)` | **이미 존재** ([resource_registry.h:94](src/resource_registry/resource_registry.h#L94)) |
| `ResourceRegistry::mPrograms` | **이미 존재** ([resource_registry.h:142](src/resource_registry/resource_registry.h#L142)) — `std::unordered_map<std::string, ProgramUPtr>` |
| `migrate_demo` / `_MyApp_` Program 사용 | **이미 `reg.CreateProgram` 패턴** ([<migrate_demo>/main.cpp:107-109](<apps>/migrate_demo/main.cpp#L107-L109), [apps/_MyApp_/main.cpp:102](apps/_MyApp_/main.cpp#L102)) |
| `tweeny_demo:199` `ProgramUPtr mProgram` 잔재 | *비활성 데모* — P5 에서 손대지 않음 (사용자 P5-a 결정) |
| `audio_demo` / `effekseer_demo` Program | **미사용** (FMOD / Effekseer 자체 셰이더) |
| `box2d_demo` `glCreateProgram` | **Box2D 내부 디버그 draw** — SP 영역 외 |

> 핸드오프 §[금지 사항] 7 의 *"Program/Framebuffer 는 ResourceRegistry 미지원"* 가 **outdated** — 실제로는 둘 다 지원 (Framebuffer 도 [resource_registry.h:108](src/resource_registry/resource_registry.h#L108) `CreateFramebuffer`).

### 8.5.2 실제 신규 — `GetAllPrograms()` 1 메서드만

```cpp
// src/resource_registry/resource_registry.h — 추가 (FindProgram 직후)
class ResourceRegistry {
public:
    /// @brief 캐시된 모든 Program 의 raw 포인터 벡터 반환 (호출 시점 스냅샷).
    /// @details mPrograms map 순회로 매 호출 vector 생성. mPrograms.size() 가 보통 1~10 이라
    ///          비용 무시. SceneRenderer 가 프레임당 1회 호출해 Light uniform 송신 대상 program 집합 획득.
    /// @return raw 포인터 vector — owner 는 ResourceRegistry (라이프타임 보장).
    std::vector<Program*> GetAllPrograms() const;
};
```

```cpp
// src/resource_registry/resource_registry.cpp — 추가
std::vector<Program*> ResourceRegistry::GetAllPrograms() const
{
    std::vector<Program*> out;
    out.reserve(mPrograms.size());
    for (const auto& [key, prog] : mPrograms)
        out.push_back(prog.get());
    return out;
}
```

### 8.5.3 반환 타입 — 값 사본 `std::vector<Program*>` (사용자 P4.5-a 결정)

대안 옵션 (B. cached vector ref / C. ForEachProgram callback) 모두 *zero-alloc* 우위지만 사용처가 프레임당 1회라 *비용 무시 가능* → **A. 값 사본 vector 가 가장 단순 + 캡슐화 완전**.

### 8.5.4 SceneRenderer 의 사용

위 §8.3 의 `RenderWithCamera` 가 `ResourceRegistry::Get().GetAllPrograms()` 호출 → `SendLightUniforms(programs, ...)` 전달. `CollectPrograms` DFS 완전 폐기.

### 8.5.5 SP 분리 정신과의 정합

원본 spec 의 *분할 3 SP* 결정 (SceneContext / ProgramRegistry / UniversalRT) 은 *Program 관련 변경이 크다* 는 가정 위였음. 실제 grep 결과 **CreateProgram + FindProgram 이 이미 존재** + **활성 데모가 이미 사용 중** → *실제 변경 = GetAllPrograms() 1 메서드 + SceneRenderer 한 줄*. *별도 SP 로 만들 만한 크기가 아님* → 본 SP 흡수가 정통 (사용자 P4-a′ 결정).

---

## §9. Phase 5 — 데모 cleanup

### 9.1 `apps/CMakeLists.txt` — **변경 없음** (재결정)

```cmake
add_subdirectory(_MyApp_)
add_subdirectory(migrate_demo)         # ← 활성 유지 (사용자 P5-a 결정 — 시각 회귀 대상)
# add_subdirectory(box2d_demo)         # ← 비활성 유지 (Box2D 내부 코드)
# add_subdirectory(effekseer_demo)     # ← 비활성 유지
# add_subdirectory(tweeny_demo)        # ← 비활성 유지
add_subdirectory(audio_demo)
```

### 9.2 `<apps>/migrate_demo/main.cpp` (3 줄 삭제)

```cpp
// :145 — 삭제
// sceneCam->Depth = 0;

// :161 — 삭제
// dir.SetActiveCamera(sceneCam);

// :372 — 삭제 (BuildPostFXChain 내부, PostFX 체인 카메라)
// cam->Depth = static_cast<int>(i) + 1;
```

**자동 순서 보장**: [main.cpp:158](<apps>/migrate_demo/main.cpp#L158) `dir.Root().AddChild(sceneCamActor)` → [main.cpp:161](<apps>/migrate_demo/main.cpp#L161) `BuildPostFXChain(reg, dir, ...)` 가 PostFX Actor 들을 *순차 AddChild*. AddChild 순서가 OnEnter 호출 순서 = `SceneContext::AddCamera` 호출 순서 = 렌더 순서. **Depth 두 줄 삭제만으로 자동 정합** — 별도 메커니즘 불필요.

### 9.3 `apps/_MyApp_/main.cpp` (2 줄 삭제)

```cpp
// :117 — 삭제
// dir.SetActiveCamera(cam);

// :224 — 삭제 (shutdown)
// SJH::Scene::Director::Get().SetActiveCamera(nullptr);
```

Camera Component 의 OnEnter 가 자동 등록 + OnExit 가 자동 해제. 명시 SetActiveCamera/nullptr 불필요.

### 9.4 비활성 데모 (tweeny_demo / effekseer_demo / box2d_demo)

- **소스 변경 없음** (사용자 P5-a 결정 — 비활성 손 안 대기)
- 향후 재활성 시점에 *Camera::Depth 폐기 + SetActiveCamera 폐기* 때문에 빌드 깨짐 → 그때 정리 (1~3 줄 수정)
- tweeny_demo:199 `SJH::ProgramUPtr mProgram` + line 125 `tsm->SetProgram(mProgram.get())` 도 그대로 — *비활성이라 빌드 안 됨*

### 9.5 audio_demo

영향 0 (Camera / SceneRenderer / Director / Depth / SetActiveCamera 미사용 grep 확인).

---

## §10. Phase 6 — 시각 회귀 게이트

### 10.1 빌드 + 실행

```bash
cmake --build --preset ninja --target migrate_demo _MyApp_ audio_demo

cd build_ninja/apps/migrate_demo && ./migrate_demo   # Phong 라이팅 + 5-pass PostFX 체인
cd build_ninja/apps/_MyApp_     && ./_MyApp_         # 탑다운 슈터 (Light 미사용)
cd build_ninja/apps/audio_demo  && ./audio_demo      # FMOD (Camera 미사용)
```

### 10.2 수용 기준

- ✅ **migrate_demo 시각 동일** — 이전 commit `6c6c243` 대비 Phong 결과 + PostFX 체인 5 단계 모두 1:1 일치 (PointLight 추가 14개 + SpotLight 추가 15개는 모두 `enabled=0` 이라 시각 영향 0)
- ✅ **_MyApp_ 시각 동일** — 탑다운 슈터 sprite + Player + 4 회색 벽 + 노란 Pickup 1:1 일치
- ✅ **audio_demo 시각 동일** — FMOD UI 그대로 (SP 영향 0)
- ✅ **콘솔 warn 0건** — `SceneRenderer::Render — SceneContext 에 Camera 0`, `DirLight 중복`, `PointLight 16 초과`, `SpotLight 16 초과` 등 모두 미발생
- ✅ **assert 발화 0건** — `cameras.empty()` assert 가 Director::Enter 정상 흐름에서 발화 X
- ✅ **빌드 성공** — migrate_demo + _MyApp_ + audio_demo 모두 link error 0
- ✅ **테스트 통과** (Catch2 활성 시) — `cmake --preset ninja -DENABLE_TESTING=ON && ctest --test-dir build_ninja -V` 21개 활성 테스트 모두 pass

### 10.3 commit 전략 — Phase 당 1 commit (사용자 P6-a 결정)

| Phase | commit 메시지 (제안) | 빌드 OK 단독? |
|---|---|---|
| **P1** | `feat(scene): SceneContext + Camera::Depth 폐기` | ⚠ Camera::Depth 폐기로 *SceneRenderer 의 sort 람다 컴파일 에러* — 단독 commit 보류 또는 P4 와 묶음. |
| **P1.5** | `refactor(shaders): MAX_POINT_LIGHTS=16 + MAX_SPOT_LIGHTS=16 대칭 통일` | ✅ (이름 정정 + 셰이더 + Send 본문 함께) |
| **P2** | `refactor(scene): Director::mActiveCamera 폐기 + GetContext() 도입` | ⚠ 데모의 `dir.SetActiveCamera(...)` 호출 컴파일 에러 → P5 와 묶음 |
| **P3** | `feat(scene): Camera/Light OnEnter/OnExit SceneContext 자동 등록` | ✅ (SceneRenderer 가 아직 SceneContext 미사용이라 단독 빌드 OK) |
| **P4 + P4.5** | `refactor(render): SceneRenderer DFS 폐기 + ResourceRegistry::GetAllPrograms()` | ✅ (P3 까지 완료 후라야 SceneContext 가 채워져 있음) |
| **P5** | `refactor(demo): SetActiveCamera/Camera::Depth 호출처 청산` | ✅ (P2 + P4 완료 후라야 빌드 OK) |
| **P6** | (commit 없음 — 검증 단계) | — |

> ⚠ 표시 = 단독 commit 시 빌드 깨짐. *실제 commit chain* 은 plan 단계에서 P1↔P4 / P2↔P5 의존성을 풀어 묶음 결정. 권장 묶음:
>  - **commit 1**: P1.5 (셰이더 — 독립)
>  - **commit 2**: P3 (Component hook — 독립)
>  - **commit 3**: P1 + P2 + P4 + P4.5 + P5 (코어 + 데모 — 동시 빌드 OK)
>  - **commit 4**: P6 (시각 회귀 검증 결과 + spec 마무리)

---

## §11. 위험 + 미해결 영역

| 항목 | 영향 | 완화 |
|---|---|---|
| `Actor::~Actor()` 가 `if (mEntered) OnExit()` 안전 가드 없음 | OnExit 호출 없이 ~Actor 시 SceneContext 에 dangling pointer | 본 SP 범위 외 — Actor lifecycle 자체 변경은 별도. 현 호출자 컨벤션 (`RemoveChild` 전 OnExit) 으로 안전 |
| `Component::SetEnabled(false)` 시 SceneContext add/remove 자동 X | render-time filter pass 1회 추가 (성능 영향 ≈ 0, 컬렉션 작음) | render-time filter 채택 (P4-b) |
| `Camera::Depth` 폐기 시 진짜 다중 view 우선순위 표현 수단 없음 | 미니맵 / 분할 화면 / UI overlay 도입 시점에 *AddCamera 순서로 보장* 또는 *별도 SP-CameraPriority* | YAGNI — 현재 migrate_demo (5-pass PostFX, AddChild 순서 정합) + _MyApp_ (단일 카메라) 모두 충족 |
| `MAX_POINT_LIGHTS=16` + `MAX_SPOT_LIGHTS=16` 확대로 매 프레임 ~32 light uniform set | program 당 ~5 uniform × 32 = ~160 glUniform 호출 — GL 3.3 forward rendering 한계 (1024 fragment uniform vector) 안에서 마진 큼 | 측정 후 필요 시 UBO 도입 (별도 SP) |
| Phase 2 LightUniformDispatcher 분리 미진행 | `SendLightUniforms` 가 SceneRenderer 의 책임으로 잔존 | 핸드오프 §[미진행 SP] Phase 2 — 별도 SP 로 후속 |
| 비활성 데모 (tweeny_demo / effekseer_demo / box2d_demo) 향후 재활성 시 빌드 깨짐 | tweeny: `ProgramUPtr mProgram` + `dir.SetActiveCamera(cam)` / effekseer: `dir.SetActiveCamera(mSceneCam)` 잔존 | 재활성 시점에 *별도 작업* 으로 정리 (Phase 1·5 동일 수정 패턴 적용 1~3 줄) |

---

## §12. 후속 SP 와의 의존 관계

```
SP-SceneContext+ProgramRegistry (본 spec)
   │
   ├─ Camera::Depth 폐기 + SceneContext 등록 자동화 (Cocos2D 정통)
   ├─ 셰이더 MAX_POINT=16 + MAX_SPOT=16 대칭 통일
   ├─ SceneRenderer 의 Camera/Light/Program DFS 3종 폐기
   └─ ResourceRegistry::GetAllPrograms() 1 메서드 신설
   │
   ▼
SP-UniversalRT      ← 동기 #3 영역
   │
   ├─ Camera::SetTargetRenderTarget(nullptr) → assert
   ├─ SceneRenderer 의 defaultTarget fallback 폐기
   └─ ScreenQuadStage 도입 — Camera 가 backbuffer 에 직접 그리지 않음 (4 엔진 정통 Compositor)
       PostFXPass 비-카메라 객체 도입 검토 (Godot CompositorEffect Array[RID] 정통)
```

원본 spec 의 *SP-ProgramRegistry 별도 SP* 는 **본 SP 흡수** (사용자 P4-a′ 결정) — `CreateProgram/FindProgram` 이 이미 존재했고 `GetAllPrograms()` 1 메서드만 추가하면 충분해 별도 SP 의 크기가 아니었음.

각 후속 SP 는 별도 brainstorm 진입 시점에 본 spec §11 의 *위험* 절을 *해결 영역* 으로 상속.

---

## §13. 변경 기록

| 일자 | 변경 |
|---|---|
| 2026-05-26 (초안) | Phase 1·1.5·2·3·4·5·6 의 단락별 합의 결과 시리얼라이즈. brainstorming 모드 단락별 질의로 모든 결정 명시. (분할 3 SP, NUM_POINT=8, NUM_SPOT=4, migrate_demo 비활성, Phase 별 commit 거부) |
| 2026-05-26 (재결정 patch) | **second brainstorm 결과 patch**: (1) SP-ProgramRegistry 흡수 — Phase 4.5 신설, ResourceRegistry::GetAllPrograms() 1 메서드 추가. (2) 셰이더 컨벤션 `NUM_*` → `MAX_*` 이름 정정 + 값 `MAX_POINT_LIGHTS=16` + `MAX_SPOT_LIGHTS=16` 통일. (3) migrate_demo 활성 유지 — cleanup 3 줄 (Depth 2 + SetActiveCamera 1). (4) Phase 당 1 commit (이전 거부 → A 채택). (5) 시각 회귀 대상 _MyApp_ 단일 → migrate_demo + _MyApp_. (6) `SceneRenderer::CollectPrograms` 완전 폐기 (임시 유지 → 즉시 폐기). |
| 2026-05-26 (구현 완료) | **구현 진행 commit chain** (의존 그래프상 2 commit 분산 + Phase 1.5 마무리 분리): (a) `a6c6387 [dev] : 렌더러 리펙토링` — 사용자 직접 commit, Phase 1.5 의 셰이더 3개 + SceneRenderer.{h,cpp} 본문 (MAX_*=16 + SpotLight 16 배열 + SendLightUniforms 16 루프 + CollectLights vector) + ResourceRegistry 포맷 변경 (spaces→tabs, 별개 작업). (b) `57e6580 refactor(common+apps): Const::MAX_*_LIGHTS=16 + migrate_demo 활성 (Phase 1.5 마무리)` — Const + apps/CMakeLists.txt 분리 commit. (c) `7d5dd2e refactor(scene+render+registry): SceneContext + ResourceRegistry::GetAllPrograms() 일괄` — commit #2 (Phase 1+2+3+4+4.5+5 코어+데모 일괄, 14 파일). **시각 회귀 사용자 검증 완료** — migrate_demo (Phong + 5-pass PostFX) + _MyApp_ (탑다운 슈터) + audio_demo demo1/demo2 (FMOD/ImGui) 모두 시각 동일. **구현 모드**: subagent-driven-development (A1-A2 + B1-B6 총 8 subagent + controller-led spec compliance review — code reviewer subagent 는 org 월간 한도 초과로 controller 가 직접 grep + 빌드 검증). |
