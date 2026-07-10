# SP-SceneContext + ProgramRegistry Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** SceneRenderer 의 매 프레임 Camera/Light/Program DFS 3종 폐기 — Cocos2D `Scene::_cameras`/`_lights` Aggregate Root 정통 합류 (`Scene::SceneContext`) + `ResourceRegistry::GetAllPrograms()` 1 메서드 신설.

**Architecture:** Component `OnEnter/OnExit` lifecycle hook 으로 SceneContext 자동 등록 (Cocos2D `addChild` 정통). `Camera::Depth` + `Director::ActiveCamera` 데드코드 청산. 셰이더 컨벤션 `MAX_POINT_LIGHTS=16` + `MAX_SPOT_LIGHTS=16` 대칭 통일 (이름 `NUM_*` → `MAX_*` 정정).

**Tech Stack:** C++17, OpenGL 4.1 Core, GLSL 410, CMake 3.14, `spdlog` (warn-once 진단).

**Spec 참조:** `doc/superpowers/specs/2026-05-26-sp-scenecontext-design.md` (로컬 전용 — `.gitignore` `doc/`).

---

## 정책 — TDD red-green 비강제 + 로컬 전용 plan

- `MEMORY.md` 의 `no_auto_tests` 정책상 *단위 테스트 task 미생성*. 검증 = *빌드 + 시각 회귀* + `ctest` 21개 활성 pass.
- 본 plan 파일도 `.gitignore` 정책으로 *로컬 전용* (commit 안 함). spec 과 동일 처리.

## Commit 묶음 — 의존 그래프상 2 commit 자연 합의

P6-a "Phase 당 1 commit" 사용자 결정은 spec §10.3 분석상 분리 불가 — `Camera::Depth` 폐기 (P1) + `SetActiveCamera` 폐기 (P2) 가 *데모 호출처* 와 *SceneRenderer std::sort* 둘 다 동시 정리 필요. 따라서:

| commit | Phase 묶음 | 단독 빌드 OK? |
|---|---|---|
| **#1** | **P1.5** (셰이더 컨벤션 — 독립) | ✅ |
| **#2** | **P1+P2+P3+P4+P4.5+P5** (코어 + 데모 일괄) | ✅ |

P6 = 시각 회귀 검증 단계 (commit 없음).

---

## File Structure

### Commit #1 — Phase 1.5 셰이더 컨벤션 (5 파일)

| 파일 | 책임 |
|---|---|
| `src/common/constants.h` | `NUM_POINT_LIGHTS=2` 폐기 + `MAX_POINT_LIGHTS=16` + `MAX_SPOT_LIGHTS=16` 신설 + `UNI_SPOT_LIGHTS_PREFIX/ENABLED_PREFIX` 신설 |
| `apps/migrate_demo/resources/shader/lighting.fs` | `#define MAX_POINT_LIGHTS 16` + `MAX_SPOT_LIGHTS 16` + `spotLights[]` 배열 + main() for 루프 |
| `apps/migrate_demo/resources/shaders/phong_color.fs` | 동일 패턴 |
| `apps/migrate_demo/resources/shaders/phong_tex.fs` | 동일 패턴 |
| `<src>/render/scene_renderer.cpp` (`SendLightUniforms` 본문 + 시그니처) | `Const::NUM_POINT_LIGHTS` → `MAX_POINT_LIGHTS` + SpotLight 단일 송신 → 16 루프 + `SpotLight* spot` → `const std::vector<SpotLight*>& spots` |
| `<src>/render/scene_renderer.h` (`SendLightUniforms` + `CollectLights` 선언) | 시그니처 정합 |

### Commit #2 — 코어 + 데모 (14 파일)

| 파일 | 책임 |
|---|---|
| `src/scene/scene_context.h` (신규) | `SceneContext` 클래스 — Camera/DirLight/PointLight/SpotLight Aggregate Root |
| `src/scene/scene_context.cpp` (신규) | 구현 — `AddCamera`/`RemoveCamera` + `AddLight`/`RemoveLight` 3 overload + reject + warn |
| `src/scene/CMakeLists.txt` | `scene_context.cpp` 추가 |
| `src/scene/camera.h` | `int Depth` 필드 + `operator<` 삭제 |
| `src/scene/camera.cpp` | `OnEnter`/`OnExit` 본문 (SceneContext 자동 등록) |
| `src/scene/scene.h` | `SceneContext mContext` 멤버 + `GetContext()` 추가 + `SetActiveCamera/GetActiveCamera/mActiveCamera/Camera forward decl` 삭제 |
| `src/object/light.h` | `OnEnter/OnExit` 인라인 빈 본문 삭제 (선언만, 본문은 .cpp) |
| `src/scene/light.cpp` | DirLight/PointLight/SpotLight `OnEnter/OnExit` 본문 (SceneContext 자동 등록) |
| `src/resource_registry/resource_registry.h` | `GetAllPrograms()` 선언 추가 |
| `src/resource_registry/resource_registry.cpp` | `GetAllPrograms()` 구현 추가 |
| `<src>/render/scene_renderer.h` | `Render(t,v,p)` 오버로드 + `CollectCameras/CollectLights/CollectPrograms` 선언 삭제, `SendLightUniforms` 첫 인자 `unordered_set` → `vector<Program*>` |
| `<src>/render/scene_renderer.cpp` | `Render(RenderTarget&)` 슬림다운 + `RenderWithCamera` 갱신 + 폐기 함수 정의 삭제 |
| `<apps>/migrate_demo/main.cpp` | line 145·161·372 삭제 (Depth=0 + SetActiveCamera + Depth=i+1) |
| `apps/_MyApp_/main.cpp` | line 117·224 삭제 (SetActiveCamera 2 곳) |

---

# Commit #1 — Phase 1.5 셰이더 컨벤션 확장

## Task 1: `Const::MAX_*_LIGHTS` 이름 정정 + 값 16 통일

**Files:**
- Modify: `src/common/constants.h:198-199`

- [ ] **Step 1: 옛 상수 폐기 + 새 상수 신설**

[src/common/constants.h:197-199](src/common/constants.h#L197-L199) 부근의 PointLight 상수 블록을 다음으로 교체:

```cpp
// 셰이더 schema 와 1:1 — lighting.fs / phong_color.fs / phong_tex.fs 의 `#define`.
// 변경 (2026-05-26): NUM_POINT_LIGHTS=2 → MAX_POINT_LIGHTS=16 (이름 정정 + 값) + MAX_SPOT_LIGHTS=16 신설.
inline constexpr int MAX_POINT_LIGHTS = 16;
inline constexpr int MAX_SPOT_LIGHTS  = 16;
```

- [ ] **Step 2: 배열 uniform prefix 추가** (단일 `UNI_SPOT_LIGHT` 폐기)

같은 파일에서 `UNI_POINT_LIGHTS_PREFIX` 정의 직후에 SpotLight prefix 추가:

```cpp
inline constexpr auto UNI_SPOT_LIGHTS_PREFIX         = "spotLights[";
inline constexpr auto UNI_SPOT_LIGHTS_ENABLED_PREFIX = "spotLightsEnabled[";
```

기존 `UNI_SPOT_LIGHT` / `UNI_SPOT_LIGHT_ENABLED` (단일 spotLight 용) 가 있으면 삭제. grep:
```bash
grep -n 'UNI_SPOT_LIGHT' src/common/constants.h
```
존재 시 삭제. 없으면 skip.

- [ ] **Step 3: 빌드 — 사용처 컴파일 에러 확인 (의도)**

```bash
cmake --build --preset ninja --target migrate_demo 2>&1 | head -20
```

Expected: `'NUM_POINT_LIGHTS' is not a member of 'SJH::Const'` 에러 → Task 3 에서 정정. *Task 1 이후 일시적 빌드 깨짐* 은 정상.

---

## Task 2: 셰이더 3개 동시 변경 (lighting.fs)

**Files:**
- Modify: `apps/migrate_demo/resources/shader/lighting.fs:50-165`

- [ ] **Step 1: `#define` 변경**

[apps/migrate_demo/resources/shader/lighting.fs:50](apps/migrate_demo/resources/shader/lighting.fs#L50):

```glsl
// 변경 전
#define NUM_POINT_LIGHTS 2

// 변경 후
#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS  16
```

- [ ] **Step 2: PointLight 배열 변수명 정정** (`NUM_*` → `MAX_*`)

같은 파일 line 53 + 61:
```glsl
// 변경 전
uniform PointLight pointLights[NUM_POINT_LIGHTS];
uniform int        pointLightsEnabled[NUM_POINT_LIGHTS];

// 변경 후
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int        pointLightsEnabled[MAX_POINT_LIGHTS];
```

- [ ] **Step 3: SpotLight 단일 → 배열**

같은 파일 line 54 + 62:
```glsl
// 변경 전
uniform SpotLight spotLight;
uniform int       spotLightEnabled;

// 변경 후
uniform SpotLight spotLights      [MAX_SPOT_LIGHTS];
uniform int       spotLightsEnabled[MAX_SPOT_LIGHTS];
```

- [ ] **Step 4: PointLights for 루프 변수명** (line 158)

```glsl
// 변경 전
for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {

// 변경 후
for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
```

- [ ] **Step 5: main() SpotLight 단일 if → for 루프** (line 164-165)

```glsl
// 변경 전
if (spotLightEnabled != 0)
    result += CalcSpotLight(spotLight, pixelNorm, viewDir);

// 변경 후
for (int i = 0; i < MAX_SPOT_LIGHTS; ++i) {
    if (spotLightsEnabled[i] != 0)
        result += CalcSpotLight(spotLights[i], pixelNorm, viewDir);
}
```

`CalcSpotLight` 함수 시그니처는 그대로 (인자 1 SpotLight + pixelNorm + viewDir) — 호출 인자만 `spotLight` → `spotLights[i]`.

---

## Task 3: phong_color.fs 동일 패턴

**Files:**
- Modify: `apps/migrate_demo/resources/shaders/phong_color.fs`

- [ ] **Step 1~5: Task 2 와 동일 패턴 적용**

대상 line:
- line 48 `#define NUM_POINT_LIGHTS 2` → Task 2 Step 1
- line 51-52 PointLight + SpotLight 배열 → Task 2 Step 2~3
- line 55-56 enabled int → Task 2 Step 2~3
- line 134 for 루프 → Task 2 Step 4
- line 139-140 SpotLight if → for → Task 2 Step 5

---

## Task 4: phong_tex.fs 동일 패턴

**Files:**
- Modify: `apps/migrate_demo/resources/shaders/phong_tex.fs`

- [ ] **Step 1~5: Task 2 와 동일 패턴 적용**

대상 line:
- line 48 `#define NUM_POINT_LIGHTS 2`
- line 51-52, 55-56 uniform 배열
- line 136 for 루프
- line 141-142 SpotLight if → for

---

## Task 5: `SendLightUniforms` 시그니처 변경 + 본문 16 루프

**Files:**
- Modify: `<src>/render/scene_renderer.h:65-76` (`CollectLights` + `SendLightUniforms` 선언)
- Modify: `<src>/render/scene_renderer.cpp:93-215` (`CollectLights` + `SendLightUniforms` 본문)

- [ ] **Step 1: `CollectLights` 선언 시그니처** (outSpot vector 화)

[<src>/render/scene_renderer.h:59-62](<src>/render/scene_renderer.h#L59-L62):

```cpp
// 변경 전
void CollectLights(const Scene::Actor& actor,
                   DirLight*& outDir,
                   std::vector<PointLight*>& outPoints,
                   SpotLight*& outSpot);

// 변경 후
void CollectLights(const Scene::Actor& actor,
                   DirLight*& outDir,
                   std::vector<PointLight*>& outPoints,
                   std::vector<SpotLight*>& outSpots);
```

- [ ] **Step 2: `SendLightUniforms` 선언 시그니처** (spot vector + Phase 1.5 한정 unordered_set 유지)

[<src>/render/scene_renderer.h:72-76](<src>/render/scene_renderer.h#L72-L76):

```cpp
// 변경 후 (첫 인자는 Phase 1.5 동안 unordered_set 유지, commit #2 의 Task 16 에서 vector 로 정정)
void SendLightUniforms(const std::unordered_set<const Program*>& programs,
                       DirLight* dir,
                       const std::vector<PointLight*>& points,
                       const std::vector<SpotLight*>& spots,   // ← outSpot 단일 → vector
                       const vmath::vec3& viewPos);
```

- [ ] **Step 3: `CollectLights` 본문 — outSpot 단일 → vector push**

[<src>/render/scene_renderer.cpp:93-129](<src>/render/scene_renderer.cpp#L93-L129) 의 `CollectLights` 안 SpotLight 분기:

```cpp
// 변경 전
if (auto *l = actor.GetComponent<SpotLight>())
{
    if (l->IsEnabled())
    {
        if (outSpot == nullptr)
            outSpot = l;
        else
            spdlog::warn("SceneRenderer::CollectLights — SpotLight 중복 발견. 첫 1개만 사용.");
    }
}

// 변경 후
if (auto *l = actor.GetComponent<SpotLight>())
{
    if (l->IsEnabled())
        outSpots.push_back(l);
}
```

- [ ] **Step 4: 호출처 (`RenderWithCamera`) 갱신**

[<src>/render/scene_renderer.cpp:77-85](<src>/render/scene_renderer.cpp#L77-L85):

```cpp
// 변경 전
DirLight *dir = nullptr;
std::vector<PointLight *> points;
SpotLight *spot = nullptr;
CollectLights(Scene::Director::Get().Root(), dir, points, spot);
...
SendLightUniforms(programs, dir, points, spot, viewPos);

// 변경 후
DirLight *dir = nullptr;
std::vector<PointLight *> points;
std::vector<SpotLight *> spots;
CollectLights(Scene::Director::Get().Root(), dir, points, spots);
...
SendLightUniforms(programs, dir, points, spots, viewPos);
```

- [ ] **Step 5: `SendLightUniforms` 본문 — `NUM_*` → `MAX_*` + SpotLight 단일 → 16 루프**

[<src>/render/scene_renderer.cpp:147-215](<src>/render/scene_renderer.cpp#L147-L215) 의 `SendLightUniforms`:

- (a) line 156, 158 — `Const::NUM_POINT_LIGHTS` → `Const::MAX_POINT_LIGHTS`
- (b) line 187 — `Const::NUM_POINT_LIGHTS` → `Const::MAX_POINT_LIGHTS`
- (c) line 204-213 — SpotLight 단일 송신 → 16 루프 (PointLights 패턴 그대로):

```cpp
// 변경 전 (line 204-213)
if (spot) {
    Uniforms::SetSpotLight(*prog, "spotLight", *spot,
                           spot->GetWorldPosition(), spot->GetWorldDirection());
    Uniforms::SetInt(*prog, "spotLightEnabled", 1);
} else {
    Uniforms::SetInt(*prog, "spotLightEnabled", 0);
}

// 변경 후
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

- (d) line 156-158 의 *과다 warn* 분기 — 16 초과 시만 warn (`MAX_POINT_LIGHTS=16` 이라 사실상 발화 어려움) 그대로 유지하되 메시지의 `NUM_POINT_LIGHTS` → `MAX_POINT_LIGHTS`.

---

## Task 6: 빌드 + 시각 회귀 (commit #1 검증)

- [ ] **Step 1: 빌드**

```bash
cmake --build --preset ninja --target migrate_demo
```

Expected: 빌드 성공. `Const::NUM_POINT_LIGHTS` 잔재 에러 없음.

- [ ] **Step 2: migrate_demo 실행 — 시각 동일 확인**

```bash
cd build_ninja/apps/migrate_demo && ./migrate_demo
```

Expected: Phong 라이팅 + 5-pass PostFX 체인 시각 동일 (이전 commit `6c6c243` 대비). PointLight 추가 14개 + SpotLight 추가 15개는 모두 `enabled=0` 로 송신되어 *시각 영향 0*. 콘솔 warn 0건.

- [ ] **Step 3: 사용자 시각 회귀 게이트**

사용자에게 *시각 동일 확인* 요청. OK 받으면 Task 7 진행.

---

## Task 7: Commit #1

- [ ] **Step 1: git status 확인**

```bash
git status -- src/common/constants.h apps/migrate_demo/resources/ src/render/scene_renderer.{h,cpp}
```

위 파일 5개만 modified 인지 확인. 다른 파일은 stage 안 함.

- [ ] **Step 2: git add + commit**

```bash
git add src/common/constants.h \
        apps/migrate_demo/resources/shader/lighting.fs \
        apps/migrate_demo/resources/shaders/phong_color.fs \
        apps/migrate_demo/resources/shaders/phong_tex.fs \
        <src>/render/scene_renderer.h \
        <src>/render/scene_renderer.cpp

git commit -m "$(cat <<'EOF'
refactor(shaders): MAX_POINT_LIGHTS=16 + MAX_SPOT_LIGHTS=16 대칭 통일

SP-SceneContext+ProgramRegistry Phase 1.5.

- Const::NUM_POINT_LIGHTS=2 → Const::MAX_POINT_LIGHTS=16 (이름 + 값)
- Const::MAX_SPOT_LIGHTS=16 신설
- 셰이더 3개 (lighting.fs / phong_color.fs / phong_tex.fs):
  - #define NUM_POINT_LIGHTS 2 → #define MAX_POINT_LIGHTS 16 + MAX_SPOT_LIGHTS 16
  - uniform SpotLight spotLight → spotLights[MAX_SPOT_LIGHTS]
  - main() SpotLight 단일 if → for-루프 (PointLights 와 대칭)
- SendLightUniforms — SpotLight 단일 송신 → 16 루프 + spotLights[i] / spotLightsEnabled[i]
- CollectLights — outSpot 단일 → outSpots vector

시각 회귀 사용자 검증 완료 — migrate_demo 시각 동일 (PointLight/SpotLight 추가 슬롯은 enabled=0 가드).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 3: 결과 확인**

```bash
git log -1 --stat
```

Expected: 6 파일 modified, 변경 라인 수 합리적.

---

# Commit #2 — Phase 1+2+3+4+4.5+5 코어 + 데모 일괄

## Task 8: `SceneContext` 헤더 신규 — `src/scene/scene_context.h`

**Files:**
- Create: `src/scene/scene_context.h`

- [ ] **Step 1: 헤더 작성**

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
    ///   - SceneRenderer 가 매 프레임 DFS traverse 하던 책임을 *등록 시점에 흡수*
    ///
    ///   ### 비-책임
    ///   - ❌ Program / Material / Mesh 보유 — ResourceRegistry / Material owner 영역
    ///   - ❌ 활성/비활성 filter — 보관은 *모두*, render-time filter 는 SceneRenderer 책임
    ///   - ❌ Camera 우선순위 정렬 — `Camera::Depth` 폐기 (Cocos2D `addChild` 순서 정통)
    ///
    ///   ### Lifetime 가정
    ///   - 모든 Component 의 owner 는 Actor — Actor 의 OnEnter/OnExit 가 Component lifecycle 보장.
    class SceneContext
    {
    public:
        // ── Camera (Cocos2D `Scene::_cameras` 정통) ───────────────────────
        void AddCamera(Camera* cam);
        void RemoveCamera(Camera* cam);
        const std::vector<Camera*>& GetCameras() const { return mCameras; }

        // ── DirLight 단일 슬롯 (셰이더 컨벤션: dirLight 1개) ───────────────
        void AddLight(SJH::DirLight* light);
        void RemoveLight(SJH::DirLight* light);
        SJH::DirLight* GetDirLight() const { return mDirLight; }

        // ── PointLight vector (max 16 = Const::MAX_POINT_LIGHTS) ──────────
        void AddLight(SJH::PointLight* light);
        void RemoveLight(SJH::PointLight* light);
        const std::vector<SJH::PointLight*>& GetPointLights() const { return mPointLights; }

        // ── SpotLight vector (max 16 = Const::MAX_SPOT_LIGHTS) ────────────
        void AddLight(SJH::SpotLight* light);
        void RemoveLight(SJH::SpotLight* light);
        const std::vector<SJH::SpotLight*>& GetSpotLights() const { return mSpotLights; }

        SceneContext();

    private:
        std::vector<Camera*>            mCameras;
        SJH::DirLight*                  mDirLight = nullptr;
        std::vector<SJH::PointLight*>   mPointLights;   // reserve(Const::MAX_POINT_LIGHTS) = 16
        std::vector<SJH::SpotLight*>    mSpotLights;    // reserve(Const::MAX_SPOT_LIGHTS) = 16
    };
}

#endif // __SJH_SCENE_CONTEXT_H__
```

- [ ] **Step 2: 빌드 확인 (소비자 0)**

```bash
cmake --build --preset ninja --target sjhopengl_scene 2>&1 | head -10
```

Expected: 빌드 OK (.h 만이라 컴파일 단위 영향 없음).

---

## Task 9: `SceneContext` 구현 — `src/scene/scene_context.cpp`

**Files:**
- Create: `src/scene/scene_context.cpp`

- [ ] **Step 1: 구현 작성**

```cpp
#include "scene/scene_context.h"
#include "common/constants.h"
#include "scene/camera.h"
#include "object/light.h"
#include <algorithm>
#include <cassert>
#include <<spdlog>/spdlog.h>

namespace SJH::Scene
{
    SceneContext::SceneContext()
    {
        mPointLights.reserve(static_cast<std::size_t>(Const::MAX_POINT_LIGHTS));
        mSpotLights.reserve(static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS));
    }

    // ── Camera ────────────────────────────────────────────────────────────
    void SceneContext::AddCamera(Camera* cam)
    {
        assert(cam && "SceneContext::AddCamera — nullptr");
        assert(std::find(mCameras.begin(), mCameras.end(), cam) == mCameras.end()
               && "SceneContext::AddCamera — 중복 등록 (Component::OnEnter contract 위반)");
        mCameras.push_back(cam);
    }

    void SceneContext::RemoveCamera(Camera* cam)
    {
        mCameras.erase(std::remove(mCameras.begin(), mCameras.end(), cam), mCameras.end());
    }

    // ── DirLight (단일 슬롯) ──────────────────────────────────────────────
    void SceneContext::AddLight(SJH::DirLight* light)
    {
        assert(light && "SceneContext::AddLight(DirLight) — nullptr");
        if (mDirLight != nullptr)
        {
            spdlog::warn("SceneContext::AddLight(DirLight) — 이미 등록됨. 추가 등록 거부 (셰이더 dirLight 단일).");
            return;
        }
        mDirLight = light;
    }

    void SceneContext::RemoveLight(SJH::DirLight* light)
    {
        if (mDirLight == light) mDirLight = nullptr;
    }

    // ── PointLight (vector, max 16) ──────────────────────────────────────
    void SceneContext::AddLight(SJH::PointLight* light)
    {
        assert(light && "SceneContext::AddLight(PointLight) — nullptr");
        if (mPointLights.size() >= static_cast<std::size_t>(Const::MAX_POINT_LIGHTS))
        {
            spdlog::warn("SceneContext::AddLight(PointLight) — MAX_POINT_LIGHTS={} 초과. 등록 거부.",
                         Const::MAX_POINT_LIGHTS);
            return;
        }
        mPointLights.push_back(light);
    }

    void SceneContext::RemoveLight(SJH::PointLight* light)
    {
        mPointLights.erase(std::remove(mPointLights.begin(), mPointLights.end(), light),
                           mPointLights.end());
    }

    // ── SpotLight (vector, max 16) ───────────────────────────────────────
    void SceneContext::AddLight(SJH::SpotLight* light)
    {
        assert(light && "SceneContext::AddLight(SpotLight) — nullptr");
        if (mSpotLights.size() >= static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS))
        {
            spdlog::warn("SceneContext::AddLight(SpotLight) — MAX_SPOT_LIGHTS={} 초과. 등록 거부.",
                         Const::MAX_SPOT_LIGHTS);
            return;
        }
        mSpotLights.push_back(light);
    }

    void SceneContext::RemoveLight(SJH::SpotLight* light)
    {
        mSpotLights.erase(std::remove(mSpotLights.begin(), mSpotLights.end(), light),
                          mSpotLights.end());
    }
}
```

- [ ] **Step 2: 빌드 — 소비자 0 이라 OK (Task 10 후 wiring)**

`scene_context.cpp` 는 CMakeLists 에 아직 미등록 → 빌드 안 됨. 다음 Task 10 에서 wiring.

---

## Task 10: `src/scene/CMakeLists.txt` — `scene_context.cpp` 추가

**Files:**
- Modify: `src/scene/CMakeLists.txt:1-7`

- [ ] **Step 1: STATIC 라이브러리 소스 목록에 추가**

```cmake
add_library(sjhopengl_scene STATIC
    actor.cpp
    scene.cpp
    scene_context.cpp      # ← 추가 (SP-SceneContext+ProgramRegistry)
    model_spawner.cpp
    camera.cpp
    compound_actor.cpp
)
```

- [ ] **Step 2: 빌드 확인 — `sjhopengl_scene` 컴파일 OK**

```bash
cmake --build --preset ninja --target sjhopengl_scene 2>&1 | tail -10
```

Expected: link 성공. spdlog include 가 PUBLIC project_deps 의 game_deps 가 아니라서 *직접 include 깨질 수* 있음 — `<<spdlog>/spdlog.h>` 가 `include/spdlog/` 에 있어 `project_deps` 의 INTERFACE include 로 자동 전파. 단 spdlog 라이브러리 *심볼 link* 가 필요하면 `SJH::scene` 의 PUBLIC link 에 `spdlog` 추가 필요.

확인:
```bash
grep -A5 'target_link_libraries(sjhopengl_scene' src/scene/CMakeLists.txt
```

`spdlog` 가 *없으면* 추가:
```cmake
target_link_libraries(sjhopengl_scene
    PUBLIC  SJH::common SJH::object project_deps
    PRIVATE spdlog   # ← 추가 (scene_context.cpp 의 warn 송신)
)
```

---

## Task 11: `Camera::Depth` + `operator<` 폐기 — `src/scene/camera.h`

**Files:**
- Modify: `src/scene/camera.h:41-42, 114-117`

- [ ] **Step 1: `int Depth = 0;` 필드 삭제** (line 41)

`Camera` 클래스의 public 멤버에서 `int Depth = 0;` 한 줄 삭제. 주변 comment 도 같이 정리.

- [ ] **Step 2: `bool operator<` 폐기** (line 114-117)

```cpp
// 변경 전
bool operator<(const Camera &other) const
{
    return Depth < other.Depth;
}

// 변경 후
// (operator< 완전 삭제 — Camera 정렬 자체가 SceneContext::mCameras 의 addCamera 호출 순서로 자연 보장)
```

- [ ] **Step 3: 빌드 — *의도된 깨짐* 확인** (Task 12+ 에서 정리)

```bash
cmake --build --preset ninja --target migrate_demo 2>&1 | grep -i error | head -10
```

Expected:
- `<src>/render/scene_renderer.cpp:33` `a->Depth` 에러
- `<apps>/migrate_demo/main.cpp:145, 372` `sceneCam->Depth` 에러

이 에러들은 Task 16, 17, 18 에서 정리됨.

---

## Task 12: `Director` 갱신 — `src/scene/scene.h`

**Files:**
- Modify: `src/scene/scene.h:1-48` (전체 재작성)

- [ ] **Step 1: 전체 헤더 교체**

```cpp
#ifndef __SJH_SCENE_H__
#define __SJH_SCENE_H__

#include "scene/actor.h"
#include "scene/scene_context.h"

namespace SJH::Scene
{
    /// @brief Cocos cc::Director 정통 — root Actor + SceneContext Aggregate Root 보유 싱글톤.
    /// @details `Scene::Director::Get().Root()` / `Scene::Director::Get().GetContext()`.
    class Director
    {
    public:
        static Director& Get();

        Actor&       Root()       { return mRoot; }
        const Actor& Root() const { return mRoot; }

        void Enter()             { mRoot.OnEnter(); }
        void Exit()              { mRoot.OnExit(); }
        void Update(float dt)    { mRoot.Update(dt); }

        SceneContext&       GetContext()       { return mContext; }
        const SceneContext& GetContext() const { return mContext; }

        Director(const Director&)            = delete;
        Director& operator=(const Director&) = delete;
        Director(Director&&)                 = delete;
        Director& operator=(Director&&)      = delete;

    private:
        Director() : mRoot("WorldRoot") {}
        ~Director() = default;

        Actor        mRoot;
        SceneContext mContext;
    };
}

#endif // __SJH_SCENE_H__
```

**삭제된 항목**:
- `class Camera;` forward decl
- `void SetActiveCamera(Camera* cam);`
- `Camera* GetActiveCamera() const;`
- `Camera* mActiveCamera = nullptr;`

- [ ] **Step 2: 빌드 — 데모 호출처 에러 확인 (의도)**

```bash
cmake --build --preset ninja --target _MyApp_ migrate_demo 2>&1 | grep -i error | head
```

Expected: `'SetActiveCamera' is not a member` 에러 5건 (migrate:161, _MyApp_:117, 224). Task 18, 19 에서 정리.

---

## Task 13: `Camera::OnEnter/OnExit` 본문 — `src/scene/camera.cpp`

**Files:**
- Modify: `src/scene/camera.cpp` (기존 InverseAffine 옆에 추가)

- [ ] **Step 1: include + 본문 추가**

`src/scene/camera.cpp` 의 `#include` 블록에 `#include "scene/scene.h"` 추가 (없으면). 같은 파일의 `namespace SJH::Scene { ... }` 안에 추가:

```cpp
void Camera::OnEnter()
{
    Director::Get().GetContext().AddCamera(this);
}

void Camera::OnExit()
{
    Director::Get().GetContext().RemoveCamera(this);
}
```

- [ ] **Step 2: `camera.h` 의 인라인 빈 본문 폐기**

[src/scene/camera.h:104-109](src/scene/camera.h#L104-L109) 의 `virtual void OnEnter() override {}` + `virtual void OnExit() override {}` 인라인 빈 본문을 *선언만* 으로 변경:

```cpp
// 변경 전
virtual void OnEnter() override {}
virtual void OnExit() override {}
virtual void Update(float dt) override {}

// 변경 후
virtual void OnEnter() override;
virtual void OnExit() override;
virtual void Update(float dt) override {}    // Update 빈 본문은 유지 (Camera 는 매 프레임 작업 없음)
```

- [ ] **Step 3: 빌드 확인**

```bash
cmake --build --preset ninja --target sjhopengl_scene 2>&1 | tail -5
```

Expected: OK. Camera::OnEnter/OnExit 가 SceneContext 의존 정합.

---

## Task 14: DirLight/PointLight/SpotLight `OnEnter/OnExit` — `src/scene/light.cpp`

**Files:**
- Modify: `src/scene/light.cpp` (기존 GetWorldDirection 등 옆에 추가)
- Modify: `src/object/light.h` (인라인 빈 본문 → 선언만)

- [ ] **Step 1: light.cpp 에 include + 본문 6 개 추가**

`src/scene/light.cpp` 의 `#include` 블록에 `#include "scene/scene.h"` 추가. 같은 파일의 `namespace SJH { ... }` 안에 추가 (기존 GetWorldXxx 헬퍼 옆):

```cpp
void DirLight::OnEnter()
{
    Scene::Director::Get().GetContext().AddLight(this);
}

void DirLight::OnExit()
{
    Scene::Director::Get().GetContext().RemoveLight(this);
}

void PointLight::OnEnter()
{
    Scene::Director::Get().GetContext().AddLight(this);
}

void PointLight::OnExit()
{
    Scene::Director::Get().GetContext().RemoveLight(this);
}

void SpotLight::OnEnter()
{
    Scene::Director::Get().GetContext().AddLight(this);
}

void SpotLight::OnExit()
{
    Scene::Director::Get().GetContext().RemoveLight(this);
}
```

- [ ] **Step 2: light.h 의 인라인 빈 본문 → 선언만**

[src/object/light.h:75-83, 116-124, 153-161](src/object/light.h#L75) — 각 Light 클래스의 OnEnter/OnExit 인라인 빈 본문 3쌍:

```cpp
// 변경 전 (DirLight / PointLight / SpotLight 각각)
virtual void OnEnter() override {
}
virtual void OnExit() override {
}
virtual void Update(float dt) override {
}

// 변경 후
virtual void OnEnter() override;
virtual void OnExit() override;
virtual void Update(float dt) override {}     // Update 빈 본문 유지
```

- [ ] **Step 3: 빌드 확인**

```bash
cmake --build --preset ninja --target sjhopengl_object 2>&1 | tail -5
```

Expected: OK. SJH::object → SJH::scene PUBLIC 의존 (이미 정합) + light.cpp 의 scene.h include OK.

---

## Task 15: `ResourceRegistry::GetAllPrograms()` 신설

**Files:**
- Modify: `src/resource_registry/resource_registry.h:88-95` (FindProgram 직후)
- Modify: `src/resource_registry/resource_registry.cpp` (FindProgram 구현 직후)

- [ ] **Step 1: 선언 추가**

[src/resource_registry/resource_registry.h:94](src/resource_registry/resource_registry.h#L94) 의 `FindProgram` 직후:

```cpp
/// @brief @p key 로 캐시된 Program *조회* (생성 안 함). 없으면 nullptr.
Program *FindProgram(const std::string &key);

/// @brief 캐시된 모든 Program 의 raw 포인터 벡터 반환 (호출 시점 스냅샷).
/// @details mPrograms map 순회로 매 호출 vector 생성. mPrograms.size() 가 보통 1~10 이라
///          비용 무시. SceneRenderer 가 프레임당 1회 호출해 Light uniform 송신 대상 program 집합 획득.
///          owner 는 ResourceRegistry (라이프타임 보장).
std::vector<Program*> GetAllPrograms() const;
```

`#include <vector>` 도 같은 파일 상단에 추가 (없으면).

- [ ] **Step 2: 구현 추가**

`src/resource_registry/resource_registry.cpp` 의 `FindProgram` 구현 직후:

```cpp
std::vector<Program*> ResourceRegistry::GetAllPrograms() const
{
    std::vector<Program*> out;
    out.reserve(mPrograms.size());
    for (const auto& [key, prog] : mPrograms)
        out.push_back(prog.get());
    return out;
}
```

- [ ] **Step 3: 빌드 확인**

```bash
cmake --build --preset ninja --target sjhopengl_resource_registry 2>&1 | tail -5
```

Expected: OK.

---

## Task 16: `SceneRenderer` 슬림다운 — 헤더

**Files:**
- Modify: `<src>/render/scene_renderer.h:36-79`

- [ ] **Step 1: `Render(t, v, p)` 오버로드 + Collect 3종 선언 삭제**

```cpp
// 변경 전
void Render(RenderTarget& defaultTarget) override;

void Render(RenderTarget& defaultTarget,
            const vmath::mat4& viewMat, const vmath::mat4& projMat);

// 폐기 함수 선언
void CollectFromActor(...);
void CollectCameras(...);
void RenderWithCamera(...);
void CollectLights(...);
void CollectPrograms(...);
void SendLightUniforms(const std::unordered_set<const Program*>& programs, ...);

// 변경 후
void Render(RenderTarget& defaultTarget) override;
// Render(target, view, proj) 오버로드 ❌ 삭제 (호출처 0건 grep 확인)

// 폐기 함수 선언
void CollectFromActor(...);
void RenderWithCamera(...);
// CollectCameras ❌ 삭제 (SceneContext::GetCameras 가 대체)
// CollectLights  ❌ 삭제 (SceneContext::GetXxxLight(s) 가 대체)
// CollectPrograms ❌ 삭제 (ResourceRegistry::GetAllPrograms() 가 대체)
void SendLightUniforms(const std::vector<Program*>& programs, ...);  // unordered_set → vector
```

- [ ] **Step 2: include 정리** — `<unordered_set>` 제거 가능 (사용처 0)

[<src>/render/scene_renderer.h:12](<src>/render/scene_renderer.h#L12) `#include <unordered_set>` 삭제.

- [ ] **Step 3: forward decl 정리**

`namespace SJH { class Program; class DirLight; class PointLight; class SpotLight; class RenderTarget; }` 는 그대로 (SendLightUniforms 시그니처에서 여전히 사용).

---

## Task 17: `SceneRenderer` 슬림다운 — 본문 + 폐기

**Files:**
- Modify: `<src>/render/scene_renderer.cpp` (전체 재작성에 가까움)

- [ ] **Step 1: include 정리**

`#include "scene/scene.h"` 가 이미 있어야 하고 (있음 확인됨), `<unordered_set>` 제거. `#include "resource_registry/resource_registry.h"` 추가 (GetAllPrograms 사용).

- [ ] **Step 2: `Render(RenderTarget&)` 본문 — DFS + sort 폐기**

```cpp
void SceneRenderer::Render(RenderTarget& defaultTarget)
{
    // 1. SceneContext 에서 Camera 컬렉션 직접 조회 — DFS 폐기.
    const auto& cameras = Scene::Director::Get().GetContext().GetCameras();
    if (cameras.empty())
    {
        spdlog::warn("SceneRenderer::Render — SceneContext 에 Camera 0 — 프레임 skip.");
        return;
    }

    // 2. addCamera 호출 순서 그대로 렌더 (Cocos2D `addChild` 정통). std::sort 폐기.
    //    render-time IsEnabled filter (Component::SetEnabled(false) 가 SceneContext 에 영향 X).
    for (auto* cam : cameras)
    {
        if (cam->IsEnabled())
            RenderWithCamera(*cam, defaultTarget);
    }
}
```

- [ ] **Step 3: `RenderWithCamera` 본문 — SceneContext + ResourceRegistry 사용**

```cpp
void SceneRenderer::RenderWithCamera(Scene::Camera& cam, RenderTarget& defaultTarget)
{
    auto& rc = DeviceContext::Get();

    RenderTarget& target = cam.GetTargetRenderTarget() ? *cam.GetTargetRenderTarget()
                                                       : defaultTarget;
    rc.BeginFrame(target);

    const auto viewMat     = cam.GetViewMatrix();
    const auto projMat     = cam.GetProjectionMatrix();
    const auto cullingMask = cam.CullingMask;

    vmath::vec3 viewPos(0.0f, 0.0f, 0.0f);
    if (auto* camOwner = cam.GetOwner())
    {
        const auto camWorld = camOwner->GetWorldMatrix();
        viewPos = vmath::vec3(camWorld[3][0], camWorld[3][1], camWorld[3][2]);
    }

    // SceneContext 에서 Light 직접 조회 — CollectLights DFS 폐기.
    auto& ctx = Scene::Director::Get().GetContext();

    DirLight* dir = (ctx.GetDirLight() && ctx.GetDirLight()->IsEnabled())
                    ? ctx.GetDirLight() : nullptr;

    std::vector<PointLight*> points;
    points.reserve(ctx.GetPointLights().size());
    for (auto* l : ctx.GetPointLights())
        if (l->IsEnabled()) points.push_back(l);

    std::vector<SpotLight*> spots;
    spots.reserve(ctx.GetSpotLights().size());
    for (auto* l : ctx.GetSpotLights())
        if (l->IsEnabled()) spots.push_back(l);

    // Program 컬렉션 — ResourceRegistry::GetAllPrograms() 한 줄 (CollectPrograms DFS 폐기).
    auto programs = ResourceRegistry::Get().GetAllPrograms();

    SendLightUniforms(programs, dir, points, spots, viewPos);

    mProcessor.Clear();
    CollectFromActor(Scene::Director::Get().Root(), viewMat, cullingMask);
    mProcessor.SortMultiStage();
    mProcessor.Process(rc, viewMat, projMat);
}
```

- [ ] **Step 4: `CollectCameras` / `CollectLights` / `CollectPrograms` / `Render(t,v,p)` 정의 삭제**

[<src>/render/scene_renderer.cpp:44-145, 217-226](<src>/render/scene_renderer.cpp#L44) 의 4 함수 정의 모두 *완전 삭제*.

- [ ] **Step 5: `SendLightUniforms` 시그니처 첫 인자 변경**

[<src>/render/scene_renderer.cpp:147-148](<src>/render/scene_renderer.cpp#L147):

```cpp
// 변경 전
void SceneRenderer::SendLightUniforms(const std::unordered_set<const Program*>& programs, ...)

// 변경 후
void SceneRenderer::SendLightUniforms(const std::vector<Program*>& programs, ...)
```

본문의 `const Program*` 순회 부분은 그대로 (vector 도 같은 형식으로 iterate 가능).

- [ ] **Step 6: 빌드 확인**

```bash
cmake --build --preset ninja --target sjhopengl_render 2>&1 | tail -10
```

Expected: link OK. CollectXxx 함수 미정의 에러 없음.

---

## Task 18: `migrate_demo` cleanup (3 줄 삭제)

**Files:**
- Modify: `<apps>/migrate_demo/main.cpp:145, 161, 372`

- [ ] **Step 1: line 145 삭제**

```cpp
// 삭제: sceneCam->Depth = 0;
```

- [ ] **Step 2: line 161 삭제**

```cpp
// 삭제: dir.SetActiveCamera(sceneCam);
```

- [ ] **Step 3: line 372 삭제** (BuildPostFXChain 내부)

```cpp
// 삭제: cam->Depth = static_cast<int>(i) + 1;
```

- [ ] **Step 4: 빌드 확인**

```bash
cmake --build --preset ninja --target migrate_demo 2>&1 | tail -10
```

Expected: 빌드 OK (Depth + SetActiveCamera 호출처 모두 청산됨).

---

## Task 19: `_MyApp_` cleanup (2 줄 삭제)

**Files:**
- Modify: `apps/_MyApp_/main.cpp:117, 224`

- [ ] **Step 1: line 117 삭제**

```cpp
// 삭제: dir.SetActiveCamera(cam);
```

- [ ] **Step 2: line 224 삭제** (shutdown 부분)

```cpp
// 삭제: SJH::Scene::Director::Get().SetActiveCamera(nullptr);
```

- [ ] **Step 3: 빌드 확인**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -10
```

Expected: 빌드 OK.

---

## Task 20: 전체 빌드 + 시각 회귀 (commit #2 검증)

- [ ] **Step 1: 활성 데모 3개 전체 빌드**

```bash
cmake --build --preset ninja --target migrate_demo _MyApp_ audio_demo
```

Expected: 3 데모 모두 빌드 성공.

- [ ] **Step 2: migrate_demo 시각 회귀**

```bash
cd build_ninja/apps/migrate_demo && ./migrate_demo
```

Expected:
- ✅ Phong 라이팅 + 5-pass PostFX 체인 시각 동일 (이전 commit `6c6c243` 대비 1:1)
- ✅ 콘솔 warn 0 (SceneContext::AddCamera 5건 정상 발생 — log 없음, AddLight DirLight 1 + 다른 Light 0)
- ✅ assert 발화 0 (cameras.empty() 안 발화)

- [ ] **Step 3: _MyApp_ 시각 회귀**

```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

Expected: 탑다운 슈터 (sprite + Player + 4 회색 벽 + 노란 Pickup) 시각 동일.

- [ ] **Step 4: audio_demo 시각 회귀** (Camera 미사용 — SP 영향 0 검증)

```bash
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected: FMOD UI 그대로.

- [ ] **Step 5: (옵션) Catch2 단위 테스트 21개 pass 확인**

```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure
```

Expected: 21개 활성 테스트 모두 pass (`test_scene_node` / `test_scene_graph` / `test_glfw_utils` 3개 비활성은 not built — 기존 상태).

- [ ] **Step 6: 사용자 시각 회귀 게이트**

3 데모 시각 동일 + 콘솔 warn 0 + ctest pass 확인 후 사용자 OK 받기. OK 받으면 Task 21 진행.

---

## Task 21: Commit #2

- [ ] **Step 1: git status 확인**

```bash
git status -- src/scene/ src/object/light.{h,cpp} src/resource_registry/ src/render/scene_renderer.{h,cpp} <apps>/migrate_demo/main.cpp apps/_MyApp_/main.cpp
```

위 파일들만 modified + 2 신규 (scene_context.h / scene_context.cpp) 확인.

- [ ] **Step 2: git add + commit**

```bash
git add src/scene/scene_context.h \
        src/scene/scene_context.cpp \
        src/scene/CMakeLists.txt \
        src/scene/scene.h \
        src/scene/camera.h \
        src/scene/camera.cpp \
        src/object/light.h \
        src/scene/light.cpp \
        src/resource_registry/resource_registry.h \
        src/resource_registry/resource_registry.cpp \
        <src>/render/scene_renderer.h \
        <src>/render/scene_renderer.cpp \
        <apps>/migrate_demo/main.cpp \
        apps/_MyApp_/main.cpp

git commit -m "$(cat <<'EOF'
refactor(scene+render+registry): SceneContext + ResourceRegistry::GetAllPrograms() 일괄

SP-SceneContext+ProgramRegistry Phase 1+2+3+4+4.5+5 (코어 + 데모 일괄).
의존 그래프상 분리 불가 — Camera::Depth 폐기 (P1) + SetActiveCamera 폐기 (P2) 가
데모 호출처 + SceneRenderer std::sort 둘 다 동시 정리 필요.

코어 — SceneContext + Director GetContext + Component hook
- src/scene/scene_context.{h,cpp} 신규 — Cocos2D `Scene::_cameras/_lights` 정통
  - DirLight 단일 슬롯, Point/Spot vector(max 16), MAX_* 초과 reject + warn
- src/scene/scene.h — Director::GetContext() 도입, SetActiveCamera/mActiveCamera/Camera fwd decl 3종 폐기
- src/scene/camera.{h,cpp} — Depth + operator< 폐기, OnEnter/OnExit 본문 추가
- src/object/light.{h,cpp} — DirLight/PointLight/SpotLight OnEnter/OnExit 본문 추가

코어 — SceneRenderer DFS 3종 폐기
- Render(t,v,p) 오버로드 폐기 (호출처 0건 grep 확인)
- CollectCameras/CollectLights/CollectPrograms 정의 + 선언 모두 폐기
- Render() — SceneContext::GetCameras() 직접 + std::sort 폐기 + render-time IsEnabled filter
- RenderWithCamera — SceneContext::GetXxxLight(s) + ResourceRegistry::GetAllPrograms()
- SendLightUniforms — unordered_set → vector<Program*> 시그니처 정합

코어 — ResourceRegistry::GetAllPrograms() 신설
- mPrograms map 순회 → vector<Program*> 값 사본 반환 (프레임당 1회 호출, 비용 무시)

데모 cleanup (활성 2 데모만, 비활성 tweeny/effekseer 손 안 댐)
- migrate_demo: line 145 (Depth=0) + 161 (SetActiveCamera) + 372 (Depth=i+1) 삭제
- _MyApp_: line 117 + 224 (SetActiveCamera 2 곳) 삭제

시각 회귀 사용자 검증 완료 — migrate_demo + _MyApp_ + audio_demo 모두 동일.
Catch2 21개 활성 테스트 pass.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 3: 결과 확인**

```bash
git log -1 --stat
```

Expected: 14 파일 modified/created.

---

## Task 22: Spec 변경 기록 갱신 (commit 없음 — 로컬 전용)

**Files:**
- Modify: `doc/superpowers/specs/2026-05-26-sp-scenecontext-design.md` (.gitignore — commit 안 함)

- [ ] **Step 1: §13 변경 기록에 구현 완료 항목 추가**

기존 표 끝에 한 행 추가:

```markdown
| 2026-05-XX (구현 완료) | commit #1: `<hash1>` (Phase 1.5) + commit #2: `<hash2>` (코어+데모). 시각 회귀 사용자 검증 완료. |
```

`<hash1>` / `<hash2>` 는 Task 7, Task 21 의 결과로 채움.

---

# Self-Review (필수 — plan 작성 후)

**Spec coverage (각 §과 task 매핑)**:
- ✅ Spec §3 P1 (SceneContext class + Camera::Depth) → Task 8 + 9 + 10 + 11
- ✅ Spec §3 P1.5 (셰이더 + Const) → Task 1 + 2 + 3 + 4 + 5
- ✅ Spec §3 P2 (Director GetContext + 3종 폐기) → Task 12
- ✅ Spec §3 P3 (Camera/Light OnEnter/OnExit) → Task 13 + 14
- ✅ Spec §3 P4 (SceneRenderer 슬림다운) → Task 16 + 17
- ✅ Spec §3 P4.5 (ResourceRegistry::GetAllPrograms) → Task 15
- ✅ Spec §3 P5 (데모 cleanup) → Task 18 + 19
- ✅ Spec §3 P6 (시각 회귀) → Task 6 + 20
- ✅ Spec §10.3 commit 묶음 (2 commit) → Task 7 (commit #1) + Task 21 (commit #2)

**Placeholder scan**:
- "TBD" / "TODO" / "implement later" / "Similar to Task N" — 검색 결과 0
- 모든 code step 이 실제 코드 또는 정확한 line 변경 명시
- "Add appropriate error handling" 류 추상 표현 0

**Type consistency**:
- `SceneContext::AddCamera/AddLight` 시그니처 — Task 8 (선언) ↔ Task 9 (구현) ↔ Task 13/14 (호출) 일치 ✅
- `SendLightUniforms` 시그니처 — Task 5 (commit #1 의 unordered_set + spots vector) → Task 16/17 (commit #2 의 vector<Program*>) 명시적 전환 ✅
- `ResourceRegistry::GetAllPrograms()` 반환 타입 `std::vector<Program*>` — Task 15 (선언+구현) ↔ Task 17 (사용) 일치 ✅
- `Const::MAX_POINT_LIGHTS` / `MAX_SPOT_LIGHTS` — Task 1 (선언) ↔ Task 2~5 (사용) ↔ Task 9 (SceneContext 가드) 일치 ✅

---

# Execution Handoff

**Plan complete and saved to `doc/superpowers/plans/2026-05-26-sp-scenecontext-programregistry-plan.md`** (로컬 전용 — `.gitignore doc/` 정책 일관).

**Two execution options:**

**1. Subagent-Driven (recommended)** — 각 Task 마다 fresh subagent 디스패치 + 사용자 시각 회귀 게이트 사이 review. 빠른 반복, 컨텍스트 격리.

**2. Inline Execution** — 본 세션에서 batch 실행 + 체크포인트마다 review. 컨텍스트 연속성.

**Which approach?**
