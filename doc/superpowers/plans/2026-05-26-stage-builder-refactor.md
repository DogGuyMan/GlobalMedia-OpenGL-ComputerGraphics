# Stage Builder 리팩토링 — 구현 plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `_MyApp_` 의 스테이지 형성 로직 (plane mesh + wall/pickup material + walls 4개 + Pickup Sensor) 을 `apps/_MyApp_/src/Stage/` 로 응집해 main.cpp 를 30+ 줄 축소.

**Architecture:** Builder 자유함수 (`CreateStageActor(StageConfig)`) 가 Composition Root, EStageStatus 는 `Components::StageState` 가 보유. wall_factory/pickup_factory 는 `Stage/Factories/` 로 이동 (네임스페이스 변경). 모든 자원은 기존 `SJH::ResourceRegistry::RegisterMesh/CreateProgram/CreateSharedMaterial` 로 위탁 — 코어 변경 없음.

**Tech Stack:** C++17, CMake STATIC + INTERFACE umbrella, Box2D v2.4.1, SJH::engine 우산.

**Spec:** [`doc/superpowers/specs/2026-05-26-stage-builder-refactor-design.md`](../specs/2026-05-26-stage-builder-refactor-design.md)

**Constraint (memory):** 단위 테스트 자발 추가 금지 (`no_auto_tests.md`). TDD red-green 강제 안 함. 빌드 통과 + 시각 회귀가 검증 게이트.

---

## File Structure

| 신설/이동/수정 | 경로 | 책임 |
|---|---|---|
| 신설 | `apps/_MyApp_/src/Stage/CMakeLists.txt` | `MyApp::Stage` STATIC (`Physics/CMakeLists.txt` 패턴) |
| 수정 | `apps/_MyApp_/src/Stage/Stage.h` | EStageStatus enum 만 — 기존 Stage 클래스 제거 |
| 신설 | `apps/_MyApp_/src/Stage/StageConfig.h` | PoD 입력 |
| 신설 | `<apps>/_MyApp_/src/Stage/StageBuilder.h` | `CreateStageActor` 선언 |
| 신설 | `<apps>/_MyApp_/src/Stage/StageBuilder.cpp` | Composition Root (자원 등록 + walls/pickups spawn) |
| 신설 | `apps/_MyApp_/src/Stage/Components/StageStateComponent.h` | `Components::StageState` |
| 이동 | `<apps>/_MyApp_/src/Stage/Factories/wall_factory.h` | `<Physics>/wall_factory.h` → namespace 변경 |
| 이동 | `apps/_MyApp_/src/Bootstrap/pickup_factory.h` | `apps/_MyApp_/src/Bootstrap/pickup_factory.h` → namespace 변경 (PickupTriggerLogger 동반) |
| 수정 | `apps/_MyApp_/src/CMakeLists.txt` | `add_subdirectory(Stage)` + `myapp_client` 에 `MyApp::Stage` 합류 |
| 수정 | `apps/_MyApp_/main.cpp` | startup() 30+ 줄 축소 (mPlane / 4 walls / Pickup 셋업 제거 → CreateStageActor 1 호출) |
| 삭제 | `<apps>/_MyApp_/src/Physics/wall_factory.h` | Stage/Factories/ 로 이동 완료 후 |
| 삭제 | `apps/_MyApp_/src/Bootstrap/pickup_factory.h` | Stage/Factories/ 로 이동 완료 후 |

---

## Task 1: `<Stage>/Factories/wall_factory.h` 신설

**Files:**
- Create: `<apps>/_MyApp_/src/Stage/Factories/wall_factory.h`

- [ ] **Step 1: 디렉토리 생성**

```bash
mkdir -p apps/_MyApp_/src/Stage/Factories
mkdir -p apps/_MyApp_/src/Stage/Components
```

- [ ] **Step 2: 파일 작성**

```cpp
#ifndef __MYAPP_STAGE_FACTORIES_WALL_FACTORY_H__
#define __MYAPP_STAGE_FACTORIES_WALL_FACTORY_H__

#include "apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h"
#include "<Physics>/filter.h"
#include "scene/actor.h"
#include <<box2d>/box2d.h>
#include <memory>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Stage::Factories
{
    /// @brief 정적 벽 Actor — b2_staticBody + box shape + Components::BoxBody.
    /// @param name   Actor 이름
    /// @param world  b2World (lifetime = PhysicsSystem)
    /// @param center 벽 중심 (XY 평면)
    /// @param half   half-extents (box 절반 크기)
    inline std::unique_ptr<SJH::Scene::Actor> CreateWallActor(
        std::string name, b2World& world, vmath::vec2 center, vmath::vec2 half)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>(std::move(name));

        b2BodyDef bd;
        bd.type = b2_staticBody;
        bd.position.Set(center[0], center[1]);
        b2Body* body = world.CreateBody(&bd);

        b2PolygonShape box;
        box.SetAsBox(half[0], half[1]);

        b2FixtureDef fd;
        fd.shape               = &box;
        fd.isSensor            = false;   // solid — Unity isTrigger OFF
        fd.filter.categoryBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Wall);
        fd.filter.maskBits     = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::WallMask);
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<TopdownShooter::Physics::Components::BoxBody>();
        pb->SetBody(body);
        pb->SetHeightOffset(0.0f);

        return actor;
    }
}

#endif // __MYAPP_STAGE_FACTORIES_WALL_FACTORY_H__
```

---

## Task 2: `apps/_MyApp_/src/Bootstrap/pickup_factory.h` 신설 (PickupTriggerLogger 동반)

**Files:**
- Create: `apps/_MyApp_/src/Bootstrap/pickup_factory.h`

- [ ] **Step 1: 파일 작성**

```cpp
#ifndef __MYAPP_STAGE_FACTORIES_PICKUP_FACTORY_H__
#define __MYAPP_STAGE_FACTORIES_PICKUP_FACTORY_H__

#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"
#include "apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h"
#include "<Physics>/filter.h"
#include "scene/actor.h"
#include <<box2d>/box2d.h>
#include <memory>
#include <<spdlog>/spdlog.h>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Stage::Factories
{
    /// @brief 픽업/감지 영역 — Trigger 이벤트를 로그로 출력 (M3 시각 검증용).
    class PickupTriggerLogger : public SJH::Scene::Component,
                                public TopdownShooter::Physics::IContactable
    {
    public:
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override {}

        void OnTriggerEnter(SJH::Scene::Actor* other) override
        {
            spdlog::info("[Pickup] OnTriggerEnter — other='{}'",
                         other ? other->GetName().c_str() : "(null)");
        }

        void OnTriggerExit(SJH::Scene::Actor* other) override
        {
            spdlog::info("[Pickup] OnTriggerExit — other='{}'",
                         other ? other->GetName().c_str() : "(null)");
        }
    };

    /// @brief 정적 Sensor 박스 — Unity isTrigger=true 영역과 동일.
    /// @param name   Actor 이름
    /// @param world  b2World
    /// @param center 박스 중심 (XY 평면)
    /// @param half   half-extents
    inline std::unique_ptr<SJH::Scene::Actor> CreatePickupActor(
        std::string name, b2World& world, vmath::vec2 center, vmath::vec2 half)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>(std::move(name));

        b2BodyDef bd;
        bd.type = b2_staticBody;
        bd.position.Set(center[0], center[1]);
        b2Body* body = world.CreateBody(&bd);

        b2PolygonShape box;
        box.SetAsBox(half[0], half[1]);

        b2FixtureDef fd;
        fd.shape               = &box;
        fd.isSensor            = true;   // Unity isTrigger ON
        fd.filter.categoryBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Pickup);
        fd.filter.maskBits     = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Player);
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<TopdownShooter::Physics::Components::BoxBody>();
        pb->SetBody(body);
        pb->SetSensor(true);

        actor->AddComponent<PickupTriggerLogger>();

        return actor;
    }
}

#endif // __MYAPP_STAGE_FACTORIES_PICKUP_FACTORY_H__
```

---

## Task 3: `apps/_MyApp_/src/Stage/Stage.h` 갱신 — EStageStatus 만

**Files:**
- Modify: `apps/_MyApp_/src/Stage/Stage.h`

기존 Stage 클래스 (Actor 상속) 제거. EStageStatus 만 비트 플래그로 정의 (FSM 통합 후속 단계 대비).

- [ ] **Step 1: 파일 전체 교체**

```cpp
#ifndef __TOPDOWNSHOOTER_STAGE_H__
#define __TOPDOWNSHOOTER_STAGE_H__

#include <cstdint>

namespace TopdownShooter::Stage
{
    /// @brief 스테이지 진행 상태 — Title / Combat / Boss 3 단계 비트 플래그.
    /// @details
    ///   - 각 enumerator 가 단일 비트 — FSM 통합 시 GetStateFlag/GetTransitFlag 와 호환.
    ///   - 현재는 Components::StageState 가 단순 보유 (setter/getter). FSM 활성화는 M4 / M7.
    ///   - NONE 은 의도적으로 없음 — 항상 Title/Combat/Boss 중 하나.
    enum class EStageStatus : uint64_t
    {
        Title  = 1ull << 0,
        Combat = 1ull << 1,
        Boss   = 1ull << 2,
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_H__
```

---

## Task 4: `apps/_MyApp_/src/Stage/StageConfig.h` 신설

**Files:**
- Create: `apps/_MyApp_/src/Stage/StageConfig.h`

- [ ] **Step 1: 파일 작성**

```cpp
#ifndef __TOPDOWNSHOOTER_STAGE_STAGE_CONFIG_H__
#define __TOPDOWNSHOOTER_STAGE_STAGE_CONFIG_H__

#include "apps/_MyApp_/src/Stage/Stage.h"
#include <vector>
#include <vmath.h>

// b2World / ResourceRegistry forward decl — heavy include 회피.
class b2World;
namespace SJH
{
    class ResourceRegistry;
}

namespace TopdownShooter::Stage
{
    /// @brief CreateStageActor 의 PoD 입력. main.cpp 가 startup() 안에서 채워서 주입.
    /// @details
    ///   - world / registry 는 *필수* — nullptr 이면 CreateStageActor 가 assert.
    ///   - pickupPositions 가 비어 있으면 Pickup Sensor 0 개 (벽만).
    struct StageConfig
    {
        b2World*               world           = nullptr;   // 필수
        SJH::ResourceRegistry* registry        = nullptr;   // 필수
        float                  arenaHalfExtent = 10.0f;     // 벽 안쪽 절반 크기
        float                  wallThickness   = 0.5f;
        std::vector<vmath::vec2> pickupPositions = { vmath::vec2(0.0f, 3.0f) };
        EStageStatus           startStatus     = EStageStatus::Title;
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_STAGE_CONFIG_H__
```

---

## Task 5: `apps/_MyApp_/src/Stage/Components/StageStateComponent.h` 신설

**Files:**
- Create: `apps/_MyApp_/src/Stage/Components/StageStateComponent.h`

- [ ] **Step 1: 파일 작성**

```cpp
#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_STAGE_STATE_COMPONENT_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_STAGE_STATE_COMPONENT_H__

#include "apps/_MyApp_/src/Stage/Stage.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage::Components
{
    /// @brief Stage Actor 에 부착되는 진행 상태 Component.
    /// @details
    ///   - 현재는 setter/getter 만 — FSM 통합은 후속 (M4 / M7).
    ///   - 이 Component 의 존재 여부로 Actor 가 "Stage" 인지 식별 가능.
    class StageState : public SJH::Scene::Component
    {
    public:
        EStageStatus Current() const { return mCurrent; }
        void SetCurrent(EStageStatus s) { mCurrent = s; }

    private:
        EStageStatus mCurrent = EStageStatus::Title;
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_STAGE_STATE_COMPONENT_H__
```

---

## Task 6: `<Stage>/StageBuilder.h` + `.cpp` 신설 (Composition Root)

**Files:**
- Create: `<apps>/_MyApp_/src/Stage/StageBuilder.h`
- Create: `<apps>/_MyApp_/src/Stage/StageBuilder.cpp`

- [ ] **Step 1: 헤더 작성**

```cpp
// <apps>/_MyApp_/src/Stage/StageBuilder.h
#ifndef __TOPDOWNSHOOTER_STAGE_STAGE_BUILDER_H__
#define __TOPDOWNSHOOTER_STAGE_STAGE_BUILDER_H__

#include "apps/_MyApp_/src/Stage/StageConfig.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Stage
{
    /// @brief Stage Actor 생성 — walls + pickups + StageState Component 가 child/component 로 매단 일반 Actor 반환.
    /// @details
    ///   - plane mesh / wallMat / pickupMat / simple.vs/fs Program 은 cfg.registry 에 자동 등록
    ///     (key: "stage_plane" / "stage_wall" / "stage_pickup" / "stage_solid_plane").
    ///   - 같은 key 가 이미 있으면 Find 로 재사용 (idempotent).
    ///   - cfg.world 또는 cfg.registry 가 nullptr 이면 assert.
    /// @param cfg StageConfig — world + registry 필수.
    /// @return Stage Actor (호출자가 Director::Root().AddChild 로 위탁)
    std::unique_ptr<SJH::Scene::Actor> CreateStageActor(const StageConfig& cfg);
}

#endif // __TOPDOWNSHOOTER_STAGE_STAGE_BUILDER_H__
```

- [ ] **Step 2: 구현 작성**

```cpp
// <apps>/_MyApp_/src/Stage/StageBuilder.cpp
#include "<Stage>/StageBuilder.h"
#include "apps/_MyApp_/src/Stage/Components/StageStateComponent.h"
#include "apps/_MyApp_/src/Bootstrap/pickup_factory.h"
#include "<Stage>/Factories/wall_factory.h"

#include "material/material.h"
#include "material/material_uniforms.h"
#include "object/mesh.h"
#include "program/program.h"
#include "render/mesh_renderer.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"

#include <<box2d>/box2d.h>
#include <cassert>
#include <string>

namespace TopdownShooter::Stage
{
    namespace
    {
        // wall/pickup 시각화 자원의 registry key — "stage_" prefix 로 영역 명시.
        constexpr const char* kPlaneKey     = "stage_plane";
        constexpr const char* kProgKey      = "stage_solid_plane";
        constexpr const char* kWallMatKey   = "stage_wall";
        constexpr const char* kPickupMatKey = "stage_pickup";
        constexpr const char* kVS           = "resources/shaders/simple.vs";
        constexpr const char* kFS           = "resources/shaders/simple.fs";

        SJH::Mesh* EnsurePlane(SJH::ResourceRegistry& reg)
        {
            if (auto* existing = reg.FindMesh(kPlaneKey))
                return existing;
            auto planeUPtr = SJH::Mesh::CreatePlane();
            return reg.RegisterMesh(kPlaneKey, std::move(planeUPtr));
        }

        SJH::Program* EnsureProgram(SJH::ResourceRegistry& reg)
        {
            if (auto* existing = reg.FindProgram(kProgKey))
                return existing;
            return reg.CreateProgram(kProgKey, kVS, kFS);
        }

        SJH::Material* EnsureMaterial(SJH::ResourceRegistry& reg, const std::string& key,
                                       SJH::Program* prog, vmath::vec4 baseColor)
        {
            if (auto* existing = reg.FindSharedMaterial(key))
                return existing;
            auto* mat = reg.CreateSharedMaterial(key);
            mat->SetProgram(prog);
            mat->SetPass(SJH::Pass::Kind::Opaque);
            SJH::Uniforms::SetVec4(*mat, "baseColor", baseColor);
            return mat;
        }
    } // namespace

    std::unique_ptr<SJH::Scene::Actor> CreateStageActor(const StageConfig& cfg)
    {
        assert(cfg.world    != nullptr && "StageConfig::world 가 nullptr — PhysicsSystem 미초기화 상태에서 호출됨");
        assert(cfg.registry != nullptr && "StageConfig::registry 가 nullptr");

        auto& reg = *cfg.registry;

        // 1) 공유 자원 등록 — idempotent (이미 있으면 Find 로 재사용)
        SJH::Mesh*     plane     = EnsurePlane(reg);
        SJH::Program*  solidProg = EnsureProgram(reg);
        SJH::Material* wallMat   = EnsureMaterial(reg, kWallMatKey, solidProg,
                                                   vmath::vec4(0.55f, 0.55f, 0.60f, 1.0f));
        SJH::Material* pickupMat = EnsureMaterial(reg, kPickupMatKey, solidProg,
                                                   vmath::vec4(1.0f, 0.85f, 0.2f, 1.0f));

        // 2) Stage Actor + StageState Component
        auto stage = std::make_unique<SJH::Scene::Actor>("MainStage");
        auto* stageState = stage->AddComponent<Components::StageState>();
        stageState->SetCurrent(cfg.startStatus);

        // 3) 벽 4개 — arena 안쪽 둘레
        const float arena = cfg.arenaHalfExtent;
        const float wallH = cfg.wallThickness;
        auto spawnWall = [&](const char* name, vmath::vec2 center, vmath::vec2 half) {
            auto a = Factories::CreateWallActor(name, *cfg.world, center, half);
            a->GetTransform().Scale = vmath::vec3(half[0] * 2.0f, 1.0f, half[1] * 2.0f);
            a->AddComponent<SJH::Scene::MeshRenderer>(plane, wallMat);
            stage->AddChild(std::move(a));
        };
        spawnWall("WallTop",    vmath::vec2(0.0f, +arena), vmath::vec2(arena, wallH));
        spawnWall("WallBottom", vmath::vec2(0.0f, -arena), vmath::vec2(arena, wallH));
        spawnWall("WallLeft",   vmath::vec2(-arena, 0.0f), vmath::vec2(wallH, arena));
        spawnWall("WallRight",  vmath::vec2(+arena, 0.0f), vmath::vec2(wallH, arena));

        // 4) Pickup Sensor — cfg.pickupPositions 각 좌표마다 하나씩
        for (std::size_t i = 0; i < cfg.pickupPositions.size(); ++i)
        {
            const auto& pos = cfg.pickupPositions[i];
            auto name = std::string("PickupTest") + std::to_string(i);
            auto p = Factories::CreatePickupActor(std::move(name), *cfg.world, pos, vmath::vec2(0.8f, 0.8f));
            p->GetTransform().Scale = vmath::vec3(1.6f, 1.0f, 1.6f);
            p->AddComponent<SJH::Scene::MeshRenderer>(plane, pickupMat);
            stage->AddChild(std::move(p));
        }

        return stage;
    }
} // namespace TopdownShooter::Stage
```

---

## Task 7: `Stage/CMakeLists.txt` 신설

**Files:**
- Create: `apps/_MyApp_/src/Stage/CMakeLists.txt`

`Physics/CMakeLists.txt` 패턴을 그대로 따름. Factory 헤더가 `Physics/*` 를 include 하므로 `MyApp::Physics` 는 PUBLIC dependency.

- [ ] **Step 1: 파일 작성**

```cmake
add_library(myapp_stage STATIC
    StageBuilder.cpp
)
add_library(MyApp::Stage ALIAS myapp_stage)

# src/ 기준 상대 인클루드 ("<Stage>/StageBuilder.h", "<Stage>/Factories/wall_factory.h" 등) — Physics 와 동일 패턴.
target_include_directories(myapp_stage
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(myapp_stage
    PUBLIC
        SJH::scene
        SJH::object
        SJH::material
        SJH::render
        SJH::resource_registry
        MyApp::Physics    # Stage/Factories/*.h 가 <Physics>/filter.h + Components.Interfaces.h include
        project_deps
        game_deps
)

target_compile_features(myapp_stage PUBLIC cxx_std_17)
```

---

## Task 8: `apps/_MyApp_/src/CMakeLists.txt` 갱신 — Stage 합류

**Files:**
- Modify: `apps/_MyApp_/src/CMakeLists.txt`

- [ ] **Step 1: `add_subdirectory(Physics)` 다음 줄에 추가**

기존:
```cmake
add_subdirectory(Algebraic)
add_subdirectory(Entity)
add_subdirectory(InputHandler)
add_subdirectory(Physics)
# M5 스켈레톤 — leaf Playable 3 모듈 (handoff doc/handoffs/2026-05-26/2026-05-26-M5-handoff.md)
add_subdirectory(Audio)
add_subdirectory(VFX)
add_subdirectory(Tween)
```

→ 변경:
```cmake
add_subdirectory(Algebraic)
add_subdirectory(Entity)
add_subdirectory(InputHandler)
add_subdirectory(Physics)
add_subdirectory(Stage)
# M5 스켈레톤 — leaf Playable 3 모듈 (handoff doc/handoffs/2026-05-26/2026-05-26-M5-handoff.md)
add_subdirectory(Audio)
add_subdirectory(VFX)
add_subdirectory(Tween)
```

- [ ] **Step 2: `myapp_client` INTERFACE 에 `MyApp::Stage` 합류**

기존:
```cmake
target_link_libraries(myapp_client INTERFACE
    MyApp::Algebraic
    MyApp::Entity
    MyApp::InputHandler
    MyApp::Physics
    MyApp::Audio    # M5 — FmodPlayable + FmodStudioPlayable (스켈레톤)
    MyApp::VFX      # M5 — EffekseerPlayable (스켈레톤)
    MyApp::Tween    # M5 — TweenPlayable (스켈레톤, 선택 도입)
)
```

→ 변경 (MyApp::Physics 줄 다음에 MyApp::Stage 추가):
```cmake
target_link_libraries(myapp_client INTERFACE
    MyApp::Algebraic
    MyApp::Entity
    MyApp::InputHandler
    MyApp::Physics
    MyApp::Stage
    MyApp::Audio    # M5 — FmodPlayable + FmodStudioPlayable (스켈레톤)
    MyApp::VFX      # M5 — EffekseerPlayable (스켈레톤)
    MyApp::Tween    # M5 — TweenPlayable (스켈레톤, 선택 도입)
)
```

---

## Task 9: `apps/_MyApp_/main.cpp` 축소

**Files:**
- Modify: `apps/_MyApp_/main.cpp` (3 영역)

- [ ] **Step 1: 인클루드 정리**

기존 `#include "apps/_MyApp_/src/Bootstrap/pickup_factory.h"` ([apps/_MyApp_/main.cpp:19](apps/_MyApp_/main.cpp#L19)) 와 `#include "<Physics>/wall_factory.h"` ([:20](apps/_MyApp_/main.cpp#L20)) 두 줄을 제거하고 `#include "<Stage>/StageBuilder.h"` 한 줄로 대체.

```cpp
// 제거
#include "apps/_MyApp_/src/Bootstrap/pickup_factory.h"
#include "<Physics>/wall_factory.h"

// 추가 (<Physics>/physics_system.h 다음 줄에)
#include "<Stage>/StageBuilder.h"
```

`#include "<Physics>/filter.h"` 는 *유지* — Player 셋업의 pac.physics.categoryBits / maskBits ([:152-153](apps/_MyApp_/main.cpp#L152-L153)) 에서 ToBits/PhysicsLayer 사용.

`#include "material/*"`, `#include "object/mesh.h"`, `#include "render/mesh_renderer.h"` 는 *유지* — src/buffer/render_target.h, <render>/scene_renderer.h 처럼 다른 부분이 의존.

- [ ] **Step 2: startup() 안 30+ 줄 제거**

기존 라인 73-78 (mPlane 생성), 105-141 (material 셋업 + 벽 4 + Pickup) 전체 제거. 대신 Physics::Init 다음에 `CreateStageActor` 1 호출.

`startup()` 안의 *기존* block 을:

```cpp
                // Plane — walls/pickups 용. sprite 는 SpriteRenderer 가 내부적으로 자체 plane 을 registry 에 자동 등록.
                mPlane = SJH::Mesh::CreatePlane();
                if (!mPlane)
                {
                    spdlog::error("[M1] mesh create failed");
                    return;
                }
```

→ **삭제**.

그리고 아래 block (Physics::Init 호출 다음):

```cpp
                // === Physics 초기화 ===
                mPhysics.Init();

                // === wall/pickup 시각화 material — simple.vs/fs (MVP + baseColor) 공유 ===
                auto *solidProg = reg.CreateProgram(
                    "solid_plane",
                    "resources/shaders/simple.vs",
                    "resources/shaders/simple.fs");
                auto *wallMat = reg.CreateSharedMaterial("solid_wall");
                wallMat->SetProgram(solidProg);
                wallMat->SetPass(SJH::Pass::Kind::Opaque);
                SJH::Uniforms::SetVec4(*wallMat, "baseColor", vmath::vec4(0.55f, 0.55f, 0.60f, 1.0f));

                auto *pickupMat = reg.CreateSharedMaterial("solid_pickup");
                pickupMat->SetProgram(solidProg);
                pickupMat->SetPass(SJH::Pass::Kind::Opaque);
                SJH::Uniforms::SetVec4(*pickupMat, "baseColor", vmath::vec4(1.0f, 0.85f, 0.2f, 1.0f));

                // 벽 4개 — 약 10×10 단위 arena. Mesh::CreatePlane 은 XZ 평면 1×1 → Scale 로 half×2 매칭.
                const float arena = 10.0f;
                const float wallH = 0.5f;
                auto spawnWall = [&](const char *name, vmath::vec2 center, vmath::vec2 half) {
                    auto a = TopdownShooter::Physics::CreateWallActor(name, mPhysics.World(), center, half);
                    a->GetTransform().Scale = vmath::vec3(half[0] * 2.0f, 1.0f, half[1] * 2.0f);
                    a->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), wallMat);
                    dir.Root().AddChild(std::move(a));
                };
                spawnWall("WallTop", vmath::vec2(0.0f, +arena), vmath::vec2(arena, wallH));
                spawnWall("WallBottom", vmath::vec2(0.0f, -arena), vmath::vec2(arena, wallH));
                spawnWall("WallLeft", vmath::vec2(-arena, 0.0f), vmath::vec2(wallH, arena));
                spawnWall("WallRight", vmath::vec2(+arena, 0.0f), vmath::vec2(wallH, arena));

                // Pickup Sensor — (0, +3) 위치. Player 가 W 키로 진입 시 OnTriggerEnter 로그 검증.
                {
                    auto p = TopdownShooter::Physics::CreatePickupActor(
                        "PickupTest", mPhysics.World(), vmath::vec2(0.0f, 3.0f), vmath::vec2(0.8f, 0.8f));
                    p->GetTransform().Scale = vmath::vec3(1.6f, 1.0f, 1.6f);
                    p->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), pickupMat);
                    dir.Root().AddChild(std::move(p));
                }
```

→ 다음으로 치환:

```cpp
                // === Physics 초기화 ===
                mPhysics.Init();

                // === Stage 형성 — walls + pickups + StageState Component 가 child/component 인 일반 Actor 반환.
                //     plane mesh / wallMat / pickupMat / simple.vs/fs Program 은 Stage Builder 가 registry 에 자동 등록.
                auto stage = TopdownShooter::Stage::CreateStageActor({
                    /*world=*/    &mPhysics.World(),
                    /*registry=*/ &reg,
                });
                dir.Root().AddChild(std::move(stage));
```

- [ ] **Step 3: 멤버 + shutdown() 정리**

기존 `SJH::MeshUPtr mPlane;` ([apps/_MyApp_/main.cpp:252](apps/_MyApp_/main.cpp#L252)) 멤버 제거.

shutdown() 안 `mPlane.reset();` ([:214](apps/_MyApp_/main.cpp#L214)) 줄 제거.

---

## Task 10: 기존 Physics factory 파일 삭제

**Files:**
- Delete: `<apps>/_MyApp_/src/Physics/wall_factory.h`
- Delete: `apps/_MyApp_/src/Bootstrap/pickup_factory.h`

- [ ] **Step 1: 파일 삭제**

```bash
rm <apps>/_MyApp_/src/Physics/wall_factory.h
rm apps/_MyApp_/src/Bootstrap/pickup_factory.h
```

`apps/_MyApp_/src/Physics/CMakeLists.txt` 의 source 목록은 *변경 불요* — 해당 두 파일이 헤더 only 였고 CMakeLists 에 명시되지 않았음 (확인 완료).

---

## Task 11: 빌드 + 시각 회귀 검증

- [ ] **Step 1: clean 후 configure (선택 — 첫 빌드라면)**

```bash
cmake --preset ninja
```

Expected: configure 단계 통과, Stage/CMakeLists.txt 발견.

- [ ] **Step 2: 빌드**

```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 빌드 통과. `MyApp::Stage` STATIC 생성 + `_MyApp_` 실행파일 link 성공.

빌드 실패 시 점검 순서:
1. include path — `<Stage>/Factories/wall_factory.h` 가 `<Physics>/filter.h` 등 include 가능한지 (Stage/CMakeLists.txt 의 PUBLIC include_directories `${CMAKE_CURRENT_SOURCE_DIR}/..` 확인)
2. 네임스페이스 — `TopdownShooter::Stage::Factories::CreateWallActor` 가 main.cpp 또는 StageBuilder.cpp 에서 정확히 호출되는지
3. MyApp::Physics PUBLIC link — Stage 가 Physics 헤더를 자기 PUBLIC 헤더에서 include 하니 PUBLIC dependency 명시 확인

- [ ] **Step 3: 실행**

```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

- [ ] **Step 4: 시각 회귀 체크리스트 (사용자 직접)**

- [ ] arena 4 벽 표시 (회색, 10×10 단위)
- [ ] Pickup Sensor 노란색 사각형 (0, +3 위치)
- [ ] Player 캐릭터 sprite 정상 (TestPattern atlas, 16-frame loop)
- [ ] Player WASD 이동 — 벽 충돌 시 정지
- [ ] Pickup Sensor 진입 시 `[Pickup] OnTriggerEnter — other='PlayerSprite'` spdlog 출력
- [ ] Camera follow 정상 (Player 추적)
- [ ] 변경 *전* 과 동일 결과 — 행위 동등성

이 중 한 항목이라도 실패 시 — Stage 안의 자원 등록 키 / Pass / Material 셋업이 기존 main.cpp 와 같은지 비교.

---

## Task 12: Commit (사용자 명시 승인 시만)

> CLAUDE.md 컨벤션 — "commit 은 사용자가 명시 요청할 때만". Task 11 시각 회귀 사용자 확인 후, 사용자가 *명시적으로 commit 트리거* 하면 진행.

- [ ] **Step 1: 사용자 commit 트리거 대기**
- [ ] **Step 2: 사용자 승인 후 변경 일괄 commit**

```bash
git add apps/_MyApp_/src/Stage/ apps/_MyApp_/src/CMakeLists.txt apps/_MyApp_/main.cpp
git rm <apps>/_MyApp_/src/Physics/wall_factory.h apps/_MyApp_/src/Bootstrap/pickup_factory.h

git commit -m "$(cat <<'EOF'
refactor(_MyApp_): wall/pickup spawn 로직을 Stage 모듈로 응집

main.cpp 의 startup() 에서 30+ 줄 boilerplate (plane mesh / 두 material / 4 walls / Pickup Sensor) 를
Stage::CreateStageActor(StageConfig) 1 호출로 치환. Physics/{wall,pickup}_factory.h 는 Stage/Factories/
하위로 이동 (namespace TopdownShooter::Stage::Factories). EStageStatus enum + Components::StageState
도입 (FSM 활성화는 후속). MyApp::Stage STATIC 신설 + MyApp::Client umbrella 합류.

Spec: doc/superpowers/specs/2026-05-26-stage-builder-refactor-design.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review (writing-plans 가이드)

**1. Spec coverage** — spec §3 (모듈 레이아웃) → Task 1~7, §4 (책임 경계) → Task 6 StageBuilder.cpp, §6 (StageConfig 시그니처) → Task 4, §7 (main.cpp 변경) → Task 9, §8 (작업 순서) → Task 1~11, §9 (시각 회귀) → Task 11 Step 4, §10 (비범위) → 명시. 누락 없음.

**2. Placeholder scan** — TBD / TODO / "implement later" / "fill in details" / "similar to Task N" 모두 없음. 모든 코드 step 이 실제 코드 블록 포함.

**3. Type consistency** —
- `EStageStatus` (Title/Combat/Boss, uint64_t 단일 비트) — Task 3 정의, Task 4 / 5 / 6 에서 동일 사용.
- `StageConfig` 필드 (world / registry / arenaHalfExtent / wallThickness / pickupPositions / startStatus) — Task 4 정의, Task 6 에서 동일 접근.
- `CreateStageActor(const StageConfig&)` 반환 `std::unique_ptr<SJH::Scene::Actor>` — Task 6 헤더 / 구현 / Task 9 main.cpp 호출 모두 일치.
- 네임스페이스 `TopdownShooter::Stage::Factories::CreateWallActor` / `CreatePickupActor` — Task 1 / 2 정의, Task 6 (StageBuilder.cpp) 에서 동일 호출.
- Registry key — `kPlaneKey = "stage_plane"`, `kProgKey = "stage_solid_plane"`, `kWallMatKey = "stage_wall"`, `kPickupMatKey = "stage_pickup"` — Task 6 cpp 안에서 단일 정의.
- CMake target `MyApp::Stage` ALIAS `myapp_stage` STATIC — Task 7 정의, Task 8 합류, Task 11 빌드.

**Self-review 통과.**
