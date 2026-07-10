# M3 Box2D 물리 통합 — 구현 계획

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** spec §4 (Box2D v2.4.1 Client 한정) 통합 — Player 가 벽 4개에 부딪혀 정지, 두 dynamic body 충돌 시각 검증.

**Architecture:**
- `apps/_MyApp_/src/physics/` Client 한정 (spec 결정 #18 — 코어 모듈 game_deps 흡수 회피).
- `PhysicsBodyComponent` (Component 베이스) — `b2Body*` non-owning 보유 (world 가 소유) + **`bool mIsSensor` (Unity `Collider.isTrigger` 매핑)**.
- `PhysicsSystem` — `b2World` owner + Step + SyncToTransform + **`b2ContactListener` 설치 + `IPhysicsContactListener` 구독자 registry**.
- **좌표계 변환** — Box2D `b2Vec2(x, y)` 는 *2D XY 평면*, Scene Transform 은 *3D XZ 평면* (top-down). 매핑: 물리 `(x, y)` → Transform `(x, heightOffset, -y)` (spec §4.4).
- **Movement 와 PhysicsBody 통합** — 별도 `PhysicsMovement : Component, IMovable` Component 신설 (사용자 결정 2026-05-25 — Movement 상속 거부, Component 단일 책임). Movement 는 Transform 직접 조작 (kinematic 모드 fallback), PhysicsMovement 는 b2Body velocity 갱신 (dynamic 모드).
- **Sensor vs Collision 모델** — Unity `Collider.isTrigger` 정통 (Context7 fetch 2026-05-25):
  - `isTrigger ON` (Sensor) — `b2Fixture::SetSensor(true)`. 물리 충돌 무시, `OnTriggerEnter/Exit(Actor*)` 이벤트만. Pickup/damage zone/area 감지용.
  - `isTrigger OFF` (Solid) — `b2Fixture::SetSensor(false)` (default). 물리 충돌 + `OnCollisionEnter/Exit(Actor*)` 이벤트. 벽/Player/적 등 *통과 못 함* 의미.
  - 디스패치: `PhysicsContactListener : public b2ContactListener` 가 `BeginContact/EndContact` 받고 `contact->GetFixtureA/B()->IsSensor()` 분기 → 양쪽 Actor 의 `IContactable` implement Component 모두 호출 (Unity 의 `MonoBehaviour::OnTriggerEnter` 정통). *(인터페이스 명: 실제 코드에서 `IContactable` 로 확정 — 2026-05-25 사용자 결정)*
  - 정적 ↔ 정적 충돌 이벤트 없음 (Box2D 규약 — 최소 한 쪽이 dynamic/kinematic). Unity 의 "최소 한 쪽 Rigidbody 필요" 와 동일.
- **isTrigger 단일 source-of-truth (2026-05-25 결정)** — `mIsSensor` 데이터 멤버 + `GetIsTrigger()` 가상 함수 *이중 표현 방지*. `Physics` 베이스가 `mIsSensor` 단일 보유 + `GetIsTrigger()` final 구현은 `return mIsSensor;`. 자식 (`CircleBody`/`BoxBody`/`Projectile`) 은 override 금지. 런타임 변경은 `SetSensor(bool)` 만 사용. *이유 — 데이터/함수 분리 시 두 상태가 어긋날 위험 + Setter dead code 화*.
- **Projectile 부착 제약 (2026-05-25 결정)** — `Carrier::Projectile` 같은 *Carrier 류 Component* 는 *Carrier 전용 Actor factory* (예: `CreateBulletActor` / `CreateExplosionAreaActor`) 만 부착. `PlayerActor` / `EnemyActor` 같은 Entity factory 가 `AddComponent<Projectile>()` 부착하는 것은 컨벤션 위반. *런타임 가드 X — 코드 리뷰 + factory 분리로 enforce*. *이유 — Entity vs Carrier 역할 구분 명확화, 무기-탄환 합성 오용 방지*.
- **렌더 순서**: PlayerController.Update → PhysicsMovement.DoForward (body velocity 갱신) → PhysicsSystem.Step(dt) (이 단계에서 b2ContactListener 콜백 발생) → PhysicsSystem.SyncToTransform → SceneRenderer.Render.

**Tech Stack:** Box2D v2.4.1 (`extern/box2d`, `lib/{macos,windows}/libbox2d{,_d}`, `include/box2d/`), `cmake/Dependency.cmake` 의 `game_deps` INTERFACE, C++17.

**선행 조건 (확인 완료)**:
- `apps/_MyApp_/CMakeLists.txt:15-17` 가 `project_deps + game_deps + SJH::engine` link ✓
- `<include>/box2d/box2d.h` umbrella header 존재 ✓
- `box2d_demo` 가 같은 패턴 (`project_deps + game_deps` link) 으로 작동 — reference 참조 가능 ✓

**범위 제외 (M3 본 plan 외)**:
- ~~Sensor / Contact listener — spec §4.5~~ → **M3 흡수 (2026-05-25 amendment)**. Unity `Collider.isTrigger` 패턴 정통.
- Ray cast — spec §4.6. M4 의 총알 hitscan 시.
- Filter mask 본격 활용 (PLAYER/ENEMY/BULLET 다대다 검증) — M4 에서 Bullet/Enemy 도입 시. M3 에서는 PLAYER/WALL/PICKUP 3개 카테고리만 시각 검증.

---

## File Structure

| 파일 | 책임 | 상태 |
|---|---|---|
| `apps/_MyApp_/src/physics/CMakeLists.txt` | `myapp_physics` STATIC + ALIAS `MyApp::Physics` | **신규** |
| `<apps>/_MyApp_/src/physics/filter.h` | uint16 카테고리 비트 상수 (PLAYER/ENEMY/WALL/PICKUP 등) + mask 조합 | **신규** |
| `<apps>/_MyApp_/src/physics/contact_interface.h` | `IPhysicsContactListener` 순수 인터페이스 — `OnTriggerEnter/Exit`, `OnCollisionEnter/Exit` (Unity `MonoBehaviour::OnTrigger*/OnCollision*` 매핑) | **신규** |
| `<apps>/_MyApp_/src/physics/physics_body.h` | `PhysicsBodyComponent` Component — `b2Body*` non-owning + `bool mIsSensor` (isTrigger) + height offset | **신규** |
| `<apps>/_MyApp_/src/physics/contact_listener.h/.cpp` | `PhysicsContactListener : b2ContactListener` — BeginContact/EndContact 받고 IsSensor 분기해 양 Actor 의 IPhysicsContactListener Component 들을 호출 | **신규** |
| `<apps>/_MyApp_/src/physics/physics_system.h/.cpp` | `PhysicsSystem` — `b2World` owner + Step + SyncToTransform + `PhysicsContactListener` 인스턴스 보유 + `SetContactListener` 설치 | **신규** |
| `<apps>/_MyApp_/src/physics/physics_movement.h/.cpp` | `PhysicsMovement : Component, IMovable` — DoForward 가 body velocity 갱신 (Movement 와 무관, 단일 책임) | **신규** |
| `<apps>/_MyApp_/src/physics/wall_factory.h` | 정적 벽 Actor free factory (`b2_staticBody` + box + isSensor=false) | **신규** (Task 7) |
| `apps/_MyApp_/src/Bootstrap/pickup_factory.h` | (검증용 예시) 픽업 Actor free factory (`b2_staticBody` + box + **isSensor=true**) + 임시 IPhysicsContactListener Component 부착 | **신규** (Task 7 — Trigger 시각 검증용) |
| `apps/_MyApp_/src/CMakeLists.txt` | `physics` subdirectory 추가 | **수정** |
| `apps/_MyApp_/CMakeLists.txt` | `MyApp::Physics` link 추가 | **수정** |
| `apps/_MyApp_/src/Entity/Player/PlayerActor.h` | `PlayerActorConfig::PhysicsCfg` nested (world / size / startPos / density / damping / filter / **isSensor**) + factory 가 PhysicsBody + PhysicsMovement 부착 | **수정** |
| `apps/_MyApp_/main.cpp` | `PhysicsSystem` 인스턴스 멤버 + Init/Step/SyncToTransform 호출 + 벽 4개 + Pickup 1개 + IPhysicsContactListener 로그 검증 | **수정** |

---

## Task 1: physics 디렉토리 + CMakeLists.txt 신설

**Files:**
- Create: `apps/_MyApp_/src/physics/CMakeLists.txt`
- Create: `apps/_MyApp_/src/physics/.gitkeep` *(또는 placeholder cpp)*

- [ ] **Step 1.1: 디렉토리 + CMakeLists 작성**

```cmake
# apps/_MyApp_/src/physics/CMakeLists.txt
add_library(myapp_physics STATIC
    physics_system.cpp
    physics_movement.cpp
    contact_listener.cpp        # ← 2026-05-25 amendment (Sensor + ContactListener Full)
)
add_library(MyApp::Physics ALIAS myapp_physics)

target_include_directories(myapp_physics
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(myapp_physics
    PUBLIC
        SJH::scene             # Component 베이스 + Actor::GetOwner / Transform
        project_deps           # vmath
        game_deps              # box2d
    PRIVATE
        spdlog                 # 진단 로그 (BeginContact/EndContact 디버그 출력)
        MyApp::Entity          # IMovable 인터페이스 (Components.Interfaces.h)
)

target_compile_features(myapp_physics PUBLIC cxx_std_17)
```

- [ ] **Step 1.2: `apps/_MyApp_/src/CMakeLists.txt` 에 subdirectory 추가**

기존 `apps/_MyApp_/src/CMakeLists.txt` 의 `add_subdirectory(Entity)` 다음 줄 추가:
```cmake
add_subdirectory(physics)
```

- [ ] **Step 1.3: `apps/_MyApp_/CMakeLists.txt` 에 `MyApp::Physics` link 추가**

기존 `_MyApp_` 타겟의 `target_link_libraries` 에 `MyApp::Physics` 1줄 추가.

---

## Task 2: Filter 카테고리 (spec §4.3)

**Files:**
- Create: `<apps>/_MyApp_/src/physics/filter.h`

- [ ] **Step 2.1: 헤더 작성**

```cpp
#ifndef __MYAPP_PHYSICS_FILTER_H__
#define __MYAPP_PHYSICS_FILTER_H__

#include <cstdint>

namespace TopdownShooter::Physics::Filter
{
    // v2.4.1 의 categoryBits / maskBits 는 uint16. 6비트면 무난 (spec §4.3).
    constexpr uint16_t PLAYER        = 1u << 0;
    constexpr uint16_t ENEMY         = 1u << 1;
    constexpr uint16_t BULLET_PLAYER = 1u << 2;
    constexpr uint16_t BULLET_ENEMY  = 1u << 3;
    constexpr uint16_t WALL          = 1u << 4;
    constexpr uint16_t PICKUP        = 1u << 5;

    // 자주 쓰는 mask 조합 — Player 는 ENEMY + BULLET_ENEMY + WALL + PICKUP 과 충돌
    constexpr uint16_t PLAYER_MASK = ENEMY | BULLET_ENEMY | WALL | PICKUP;
    constexpr uint16_t ENEMY_MASK  = PLAYER | BULLET_PLAYER | WALL;
    constexpr uint16_t WALL_MASK   = PLAYER | ENEMY | BULLET_PLAYER | BULLET_ENEMY;
}

#endif // __MYAPP_PHYSICS_FILTER_H__
```

---

## Task 3: `PhysicsBodyComponent` (Component + b2Body 보유)

**Files:**
- Create: `<apps>/_MyApp_/src/physics/physics_body.h`

- [ ] **Step 3.1: 헤더 작성**

```cpp
#ifndef __MYAPP_PHYSICS_BODY_H__
#define __MYAPP_PHYSICS_BODY_H__

#include "scene/actor.h"          // SJH::Scene::Component
#include <<box2d>/box2d.h>
#include <vmath.h>

namespace TopdownShooter::Physics
{
    /// @brief b2Body* non-owning 보유 Component. PhysicsSystem 의 b2World 가 lifetime 소유.
    /// @details
    ///   ### 좌표계 (spec §4.4)
    ///   - 물리: b2Vec2(x, y) 2D XY 평면
    ///   - 렌더: Transform.Translate = vmath::vec3(x, heightOffset, -y) 3D XZ 평면 (top-down)
    ///   - Y → -Z mapping: Box2D 의 +Y "위" 방향이 OpenGL XZ 평면의 -Z (카메라 정면) 와 매핑
    ///
    ///   ### Sensor (Unity Collider.isTrigger 매핑, 2026-05-25 amendment)
    ///   - mIsSensor=true  → b2Fixture::SetSensor(true). 물리 충돌 안 함, BeginContact 이벤트만.
    ///                       Unity 의 isTrigger ON 정통 (Pickup/damage zone/area 감지).
    ///   - mIsSensor=false → b2Fixture::SetSensor(false) (default). 물리 충돌 + 이벤트.
    ///                       Unity 의 isTrigger OFF 정통 (벽/Player/적 — 통과 못 함).
    ///   - factory 가 b2FixtureDef::isSensor 에 mIsSensor 위임 (또는 생성 후 Fixture::SetSensor).
    ///
    ///   ### isTrigger 단일 source-of-truth (2026-05-25 결정)
    ///   - mIsSensor 가 유일한 진실. GetIsTrigger() 는 항상 `return mIsSensor;` 로 final 구현.
    ///   - 자식 클래스 (CircleBody/BoxBody/Projectile) 는 GetIsTrigger() override 금지.
    ///   - 런타임 토글은 SetSensor(bool) 만 — 데이터/함수 이중 표현으로 두 상태가 어긋날 위험 차단.
    ///
    ///   ### 사용 패턴
    ///   PhysicsSystem 이 Actor 들 순회하며 SyncToTransform 단계에서 Transform 갱신.
    class PhysicsBodyComponent : public SJH::Scene::Component
    {
    public:
        PhysicsBodyComponent() = default;

        /// @brief b2Body* 주입 — PhysicsSystem 의 b2World::CreateBody 결과를 받음.
        /// @note user data 로 owner Actor* 자동 등록 (contact listener 에서 회수용).
        PhysicsBodyComponent& SetBody(b2Body* body)
        {
            mBody = body;
            if (mBody) {
                mBody->GetUserData().pointer = reinterpret_cast<uintptr_t>(GetOwner());
            }
            return *this;
        }

        /// @brief 렌더 Y offset (top-down 에선 sprite 가 ground 보다 약간 위로 떠 있음).
        PhysicsBodyComponent& SetHeightOffset(float y)
        {
            mHeightOffset = y;
            return *this;
        }

        /// @brief Unity Collider.isTrigger 와 동일. factory 가 fixture 만들 때 위임.
        /// @details true 면 충돌 무시 + OnTriggerEnter/Exit 이벤트만. false 면 진짜 충돌.
        ///          런타임에 변경 시 mBody->GetFixtureList()->SetSensor(s) 도 같이 호출해야 동기화.
        PhysicsBodyComponent& SetSensor(bool s)
        {
            mIsSensor = s;
            if (mBody) {
                // 런타임 변경 — body 이미 생성됐을 때 모든 fixture 에 전파.
                for (b2Fixture* f = mBody->GetFixtureList(); f; f = f->GetNext()) {
                    f->SetSensor(s);
                }
            }
            return *this;
        }

        b2Body* GetBody() const { return mBody; }
        float   GetHeightOffset() const { return mHeightOffset; }
        bool    IsSensor() const { return mIsSensor; }

        // === Component lifecycle (Component 순수 가상 — 빈 override) ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override {}   // PhysicsSystem.SyncToTransform 이 처리

    private:
        b2Body* mBody         = nullptr;
        float   mHeightOffset = 0.0f;
        bool    mIsSensor     = false;   // Unity Collider.isTrigger 기본값과 동일 — single source-of-truth
    };

    // === isTrigger 단일 source-of-truth 패턴 (실제 코드 매핑) ===
    // apps/_MyApp_/src/Physics/PhysicsComponent.h 의 Physics 베이스에는 다음 final 메서드 추가:
    //
    //     virtual bool GetIsTrigger() final { return mIsSensor; }   // ← final — 자식 override 금지
    //
    // CircleBody/BoxBody/Projectile 는 GetIsTrigger() override 하지 말 것. 런타임 토글은 SetSensor() 만.
}

#endif // __MYAPP_PHYSICS_BODY_H__
```

---

## Task 4: `PhysicsSystem` (b2World owner + Step + SyncToTransform)

**Files:**
- Create: `<apps>/_MyApp_/src/physics/physics_system.h`
- Create: `<apps>/_MyApp_/src/physics/physics_system.cpp`

- [ ] **Step 4.1: 헤더 작성**

```cpp
#ifndef __MYAPP_PHYSICS_SYSTEM_H__
#define __MYAPP_PHYSICS_SYSTEM_H__

#include <<box2d>/box2d.h>
#include <memory>

namespace SJH::Scene { class Scene; class Actor; }

namespace TopdownShooter::Physics
{
    class PhysicsContactListener;   // forward — contact_listener.h

    /// @brief b2World owner — Step + SyncToTransform + ContactListener 설치 책임 (spec §4.2 + 2026-05-25 amendment).
    /// @details main.cpp 가 매 frame 호출 흐름:
    ///   1. PlayerController.Update → PhysicsMovement.DoForward (body velocity 갱신)
    ///   2. physics.Step(dt) — b2World::Step(dt, 8, 3) (v2.4.1 default iterations)
    ///      이 단계에서 ContactListener::BeginContact/EndContact 콜백 발생 →
    ///      양쪽 Actor 의 IPhysicsContactListener Component 들에 OnTrigger/OnCollision 디스패치
    ///   3. physics.SyncToTransform(scene) — body 좌표 → Actor.Transform 반영
    ///   4. SceneRenderer.Render
    class PhysicsSystem
    {
    public:
        PhysicsSystem();
        ~PhysicsSystem();

        PhysicsSystem(const PhysicsSystem&)            = delete;
        PhysicsSystem& operator=(const PhysicsSystem&) = delete;

        /// @brief b2World 생성 (gravity=(0,0)) + ContactListener 설치.
        void Init();
        void Shutdown();

        /// @brief b2World::Step(dt, velocityIters=8, positionIters=3). v2.4.1 default.
        void Step(float dt);

        /// @brief Scene 의 모든 PhysicsBodyComponent 부착 Actor 순회 + Transform 갱신.
        ///        물리 (x, y) → 렌더 (x, heightOffset, -y).
        void SyncToTransform(SJH::Scene::Scene& scene);

        b2World& World() { return *mWorld; }

    private:
        std::unique_ptr<b2World>                mWorld;
        std::unique_ptr<PhysicsContactListener> mContactListener;   // 2026-05-25 amendment
    };
}

#endif // __MYAPP_PHYSICS_SYSTEM_H__
```

- [ ] **Step 4.2: cpp 구현**

```cpp
#include "<physics>/physics_system.h"
#include "<physics>/physics_body.h"
#include "<physics>/contact_listener.h"   // 2026-05-25 amendment

#include "scene/actor.h"
#include "scene/scene.h"
#include "object/transform.h"

#include <<spdlog>/spdlog.h>

namespace TopdownShooter::Physics
{
    PhysicsSystem::PhysicsSystem()  = default;
    PhysicsSystem::~PhysicsSystem() { Shutdown(); }

    void PhysicsSystem::Init()
    {
        if (mWorld) {
            spdlog::warn("[Physics] Init called twice — 무시");
            return;
        }
        mWorld = std::make_unique<b2World>(b2Vec2(0.0f, 0.0f));   // top-down: gravity 0
        // 2026-05-25 amendment — ContactListener 설치 (Unity Collider 이벤트 정통 매핑)
        mContactListener = std::make_unique<PhysicsContactListener>();
        mWorld->SetContactListener(mContactListener.get());
        spdlog::info("[Physics] b2World 생성 (gravity = 0) + ContactListener 설치");
    }

    void PhysicsSystem::Shutdown()
    {
        mWorld.reset();   // b2World 소멸자가 모든 b2Body 자동 정리
    }

    void PhysicsSystem::Step(float dt)
    {
        if (!mWorld) return;
        // v2.4.1 권장 iteration: velocity 8, position 3
        mWorld->Step(dt, /*velocityIterations=*/8, /*positionIterations=*/3);
    }

    void PhysicsSystem::SyncToTransform(SJH::Scene::Scene& scene)
    {
        // Scene 의 모든 PhysicsBody 부착 Actor 순회.
        // ⚠️ SJH::Scene::Scene 의 정확한 traversal API 확인 필요 — 본 plan 작성 시점엔 미정착.
        //    옵션 A: Scene::Root().ForEachDescendant([](Actor*)) — 재귀 순회
        //    옵션 B: Scene 에 별도 컨테이너 (AllActorsWith<T>())
        //    실 구현 단계 (M3 진입) 에서 src/scene/scene.h 확인 후 결정.
        //
        // 임시 — Scene 의 Root() 부터 재귀 traversal (모든 Actor 후손).
        auto traverse = [this](SJH::Scene::Actor* actor, auto& self) -> void {
            if (!actor) return;
            auto* pb = actor->GetComponent<PhysicsBodyComponent>();
            if (pb && pb->GetBody()) {
                const b2Vec2 p = pb->GetBody()->GetPosition();
                auto& tr = actor->GetTransform();
                // 물리 XY → 렌더 XZ (Y 는 height offset).
                tr.Translate = vmath::vec3(p.x, pb->GetHeightOffset(), -p.y);
                // (필요 시 회전 동기화 — 본 plan 범위 외, M4 시 ApplyRotation 시점에 추가)
            }
            for (auto& child : actor->GetChildren()) {
                self(child.get(), self);
            }
        };
        traverse(&scene.Root(), traverse);
    }
}
```

**구현 주의 — Scene traversal API 확인 필요**: `SJH::Scene::Scene::Root()` 가 Actor& 반환하는지, `Actor::GetChildren()` 시그니처가 본 코드와 일치하는지 M3 진입 시점에 `src/scene/scene.h` + `actor.h` 정독 후 정확한 API 로 정정. 본 plan 의 코드는 일반적 패턴 가정.

---

## Task 4.5: `IPhysicsContactListener` 인터페이스 + `PhysicsContactListener` 구현 (2026-05-25 amendment)

**배경**: Unity `Collider.isTrigger` 정통 매핑 — `isTrigger ON = Sensor → OnTriggerEnter`, `isTrigger OFF = Solid → OnCollisionEnter` (Context7 fetch 2026-05-25). Box2D 는 `b2ContactListener::BeginContact/EndContact` 단일 콜백에서 `b2Fixture::IsSensor()` 분기로 두 경로 디스패치.

**Files:**
- Create: `<apps>/_MyApp_/src/physics/contact_interface.h`
- Create: `<apps>/_MyApp_/src/physics/contact_listener.h`
- Create: `<apps>/_MyApp_/src/physics/contact_listener.cpp`

- [ ] **Step 4.5.1: `IPhysicsContactListener` 순수 인터페이스**

```cpp
#ifndef __MYAPP_PHYSICS_CONTACT_INTERFACE_H__
#define __MYAPP_PHYSICS_CONTACT_INTERFACE_H__

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Physics
{
    /// @brief Unity MonoBehaviour::OnTriggerEnter / OnCollisionEnter 정통 매핑.
    /// @details Component 가 이 인터페이스를 implement 하면 PhysicsContactListener 가
    ///          BeginContact 시 IsSensor 분기로 양쪽 Actor 의 implement 한 Component 들을 호출.
    ///
    ///   ### 4-콜백 의미 (Unity 정통)
    ///   - OnTriggerEnter(other): isTrigger ON 영역에 다른 collider 가 진입 (관통 시작)
    ///   - OnTriggerExit (other): isTrigger ON 영역에서 벗어남
    ///   - OnCollisionEnter(other): solid 충돌 발생 (벽 부딪힘 등)
    ///   - OnCollisionExit (other): solid 충돌 종료 (벽에서 떨어짐 등)
    ///
    ///   ### Box2D 디스패치 규약
    ///   - PhysicsContactListener::BeginContact → fixture->IsSensor() 양쪽 다 검사 →
    ///     하나라도 sensor 면 양쪽에 OnTriggerEnter, 둘 다 solid 면 양쪽에 OnCollisionEnter.
    ///   - "other" 인자는 *상대 Actor* (this 의 owner Actor 가 아님).
    ///   - 정적 ↔ 정적 충돌은 Box2D 가 이벤트를 안 보냄 (Unity 의 "최소 한 쪽 Rigidbody" 규약).
    class IPhysicsContactListener
    {
    protected:
        IPhysicsContactListener() = default;

    public:
        virtual ~IPhysicsContactListener() = default;
        IPhysicsContactListener(const IPhysicsContactListener&)            = delete;
        IPhysicsContactListener& operator=(const IPhysicsContactListener&) = delete;
        IPhysicsContactListener(IPhysicsContactListener&&)                 = delete;
        IPhysicsContactListener& operator=(IPhysicsContactListener&&)      = delete;

        virtual void OnTriggerEnter   (SJH::Scene::Actor* /*other*/) {}
        virtual void OnTriggerExit    (SJH::Scene::Actor* /*other*/) {}
        virtual void OnCollisionEnter (SJH::Scene::Actor* /*other*/) {}
        virtual void OnCollisionExit  (SJH::Scene::Actor* /*other*/) {}
    };
}

#endif // __MYAPP_PHYSICS_CONTACT_INTERFACE_H__
```

- [ ] **Step 4.5.2: `PhysicsContactListener` 헤더**

```cpp
#ifndef __MYAPP_PHYSICS_CONTACT_LISTENER_H__
#define __MYAPP_PHYSICS_CONTACT_LISTENER_H__

#include <<box2d>/box2d.h>

namespace TopdownShooter::Physics
{
    /// @brief b2World::SetContactListener 대상. BeginContact/EndContact 만 override.
    /// @details PreSolve/PostSolve 는 본 plan 범위 외 (M4 이후 — 충돌 응답 커스터마이즈 시).
    class PhysicsContactListener : public b2ContactListener
    {
    public:
        void BeginContact(b2Contact* contact) override;
        void EndContact  (b2Contact* contact) override;
    };
}

#endif // __MYAPP_PHYSICS_CONTACT_LISTENER_H__
```

- [ ] **Step 4.5.3: `PhysicsContactListener` cpp 구현**

```cpp
#include "<physics>/contact_listener.h"
#include "<physics>/contact_interface.h"

#include "scene/actor.h"   // Actor::IterateComponents 또는 GetComponents 등 — M3 진입 시 정확한 API 확인 필요

#include <<spdlog>/spdlog.h>

namespace TopdownShooter::Physics
{
    namespace
    {
        // b2Body::GetUserData().pointer 에 등록된 Actor* 회수.
        // PhysicsBodyComponent::SetBody 가 이미 reinterpret_cast<uintptr_t>(Actor*) 로 등록함.
        SJH::Scene::Actor* ActorFromBody(b2Body* body)
        {
            if (!body) return nullptr;
            uintptr_t raw = body->GetUserData().pointer;
            return reinterpret_cast<SJH::Scene::Actor*>(raw);
        }

        // Actor 의 모든 Component 중 IPhysicsContactListener implement 한 것에 콜백 전파.
        // M3 진입 시 SJH::Scene::Actor 의 컴포넌트 순회 API (예: ForEachComponent / GetComponents)
        // 확인 후 *정확한 메서드명* 으로 교체. 본 코드는 일반적 패턴 가정.
        enum class Phase { TriggerEnter, TriggerExit, CollisionEnter, CollisionExit };
        void Dispatch(SJH::Scene::Actor* self, SJH::Scene::Actor* other, Phase phase)
        {
            if (!self) return;
            // 옵션 A — Actor 가 ForEachComponent(lambda) 제공:
            // self->ForEachComponent([&](SJH::Scene::Component* c) {
            //     if (auto* listener = dynamic_cast<IPhysicsContactListener*>(c)) {
            //         switch (phase) { ... }
            //     }
            // });
            //
            // 옵션 B — Actor 가 GetComponents() (벡터) 제공:
            // for (auto& c : self->GetComponents()) { ... }
            //
            // M3 진입 시 src/scene/actor.h 의 컴포넌트 컨테이너 API 정독 후 교체.
            // 임시 placeholder — spdlog 로 phase + 이름만 출력 (실제 dispatch 미구현 상태).
            const char* phaseName =
                phase == Phase::TriggerEnter   ? "TriggerEnter"   :
                phase == Phase::TriggerExit    ? "TriggerExit"    :
                phase == Phase::CollisionEnter ? "CollisionEnter" : "CollisionExit";
            spdlog::debug("[ContactListener] {} → self='{}', other='{}'",
                phaseName,
                self->GetName().c_str(),
                other ? other->GetName().c_str() : "(null)");
        }
    }

    void PhysicsContactListener::BeginContact(b2Contact* contact)
    {
        if (!contact) return;
        b2Fixture* fA = contact->GetFixtureA();
        b2Fixture* fB = contact->GetFixtureB();
        if (!fA || !fB) return;

        SJH::Scene::Actor* aA = ActorFromBody(fA->GetBody());
        SJH::Scene::Actor* aB = ActorFromBody(fB->GetBody());

        // Unity 규약: 하나라도 isTrigger → 양쪽 모두 OnTriggerEnter, 둘 다 solid → OnCollisionEnter.
        const bool eitherSensor = fA->IsSensor() || fB->IsSensor();
        const Phase phase = eitherSensor ? Phase::TriggerEnter : Phase::CollisionEnter;

        Dispatch(aA, aB, phase);
        Dispatch(aB, aA, phase);
    }

    void PhysicsContactListener::EndContact(b2Contact* contact)
    {
        if (!contact) return;
        b2Fixture* fA = contact->GetFixtureA();
        b2Fixture* fB = contact->GetFixtureB();
        if (!fA || !fB) return;

        SJH::Scene::Actor* aA = ActorFromBody(fA->GetBody());
        SJH::Scene::Actor* aB = ActorFromBody(fB->GetBody());

        const bool eitherSensor = fA->IsSensor() || fB->IsSensor();
        const Phase phase = eitherSensor ? Phase::TriggerExit : Phase::CollisionExit;

        Dispatch(aA, aB, phase);
        Dispatch(aB, aA, phase);
    }
}
```

**구현 주의 — Actor Component 순회 API**: 위 Dispatch 함수의 *실제 IPhysicsContactListener implement Component 호출* 은 M3 진입 시점에 `src/scene/actor.h` 의 컴포넌트 컨테이너 API (`ForEachComponent` / `GetComponents` / `mComponents` 직접 접근 등) 를 정독한 뒤 정확한 형태로 교체. 본 plan 의 코드는 spdlog 로그 placeholder + dynamic_cast 분기 pseudocode.

---

## Task 5: `PhysicsMovement : IMovable` (Movement 의 PhysicsBody 변형)

**Files:**
- Create: `<apps>/_MyApp_/src/physics/physics_movement.h`
- Create: `<apps>/_MyApp_/src/physics/physics_movement.cpp`

**설계 선택 (Task 7 의 통합 결정 항목)**:
- 옵션 A: 기존 `Movement` Component 가 *kinematic body* 처럼 직접 Transform 갱신 (충돌 무시)
- 옵션 B: 신규 `PhysicsMovement` Component 가 *dynamic body 의 velocity 갱신* → b2World 가 충돌 자동 처리 + SyncToTransform 이 위치 회수
- **권장: 옵션 B** — 충돌 정지가 자동 작동, spec §4 의 dynamic body 패턴 정통

- [ ] **Step 5.1: 헤더 작성**

```cpp
#ifndef __MYAPP_PHYSICS_MOVEMENT_H__
#define __MYAPP_PHYSICS_MOVEMENT_H__

#include "apps/_MyApp_/src/Algebraic/Stat.h"
#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"
#include "<physics>/physics_body.h"
#include "scene/actor.h"

namespace TopdownShooter::Physics
{
    /// @brief Movement 의 PhysicsBody 변형 — DoForward 가 b2Body velocity 갱신.
    /// @details
    ///   ### 동작
    ///   - mPhysicsBody = owner Actor 의 PhysicsBodyComponent (OnEnter 에서 GetComponent 로 lookup)
    ///   - DoForward(dir, dt): body->SetLinearVelocity(normalize(dir) * mMoveSpeed)
    ///     — dt 는 b2World::Step 이 처리 (PhysicsSystem 측에서 fps-independent 자동)
    ///   - dt 인자는 *인터페이스 호환* 위해 유지 (사용 안 함)
    ///
    ///   ### 좌표계
    ///   - dir = vmath::vec2(x, z) — PlayerController 의 XZ 평면 입력
    ///   - b2Vec2 변환: b2Vec2(dir.x, -dir.y) (Z → -Y 매핑, spec §4.4 와 일관)
    class PhysicsMovement : public SJH::Scene::Component,
                            public Entity::IMovable
    {
    public:
        PhysicsMovement()
            : mMoveSpeed(0.0f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
        {
        }

        explicit PhysicsMovement(float movespeed)
            : mMoveSpeed(movespeed, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
        {
        }

        // === Component lifecycle ===
        void OnEnter() override
        {
            // owner Actor 에 PhysicsBodyComponent 가 부착되어 있어야 함 (factory 가 책임).
            if (auto* owner = GetOwner()) {
                mPhysicsBody = owner->GetComponent<PhysicsBodyComponent>();
            }
        }
        void OnExit() override
        {
            mPhysicsBody = nullptr;
        }
        void Update(float /*dt*/) override {}   // velocity 는 DoForward 에서만 갱신, 그 외 0 으로 누적 안 함

        // === IMovable 구현 ===
        void DoForward(vmath::vec2 dir, float /*dt*/) override
        {
            if (!mPhysicsBody || !mPhysicsBody->GetBody()) return;
            // zero-vec 가드 — body velocity = 0 으로 즉시 정지 (관성 제거)
            if (dir[0] == 0.0f && dir[1] == 0.0f) {
                mPhysicsBody->GetBody()->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
                return;
            }
            // dir XZ → Box2D XY (Z → -Y, spec §4.4)
            vmath::vec2 normalized = vmath::normalize(dir);
            const float speed = mMoveSpeed.GetValue();
            mPhysicsBody->GetBody()->SetLinearVelocity(b2Vec2(normalized[0] * speed, -normalized[1] * speed));
        }

    private:
        Algebraic::Numeric::Stat mMoveSpeed;
        PhysicsBodyComponent*    mPhysicsBody = nullptr;
    };
}

#endif // __MYAPP_PHYSICS_MOVEMENT_H__
```

- [ ] **Step 5.2: cpp 작성 (placeholder — 헤더-only 면 빈 cpp 또는 skip)**

전체 헤더-only 가능하나 *CMakeLists 가 cpp 1개 요구* 라 빈 cpp:

```cpp
// physics_movement.cpp — 헤더-only 클래스의 link-only placeholder.
#include "<physics>/physics_movement.h"
```

---

## Task 6: `PlayerActor.h` factory 확장 (PhysicsBody 부착)

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerActor.h`

- [ ] **Step 6.1: PlayerActorConfig 에 physics nested struct 추가**

기존 `struct PlayerActorConfig` 의 `ControllerCfg` 다음에 추가:

```cpp
struct PhysicsCfg
{
    b2World* world             = nullptr;     // PhysicsSystem.World() 주입
    vmath::vec2 size           = vmath::vec2(1.0f, 1.0f);   // 충돌 box 너비/높이 (XY 평면)
    vmath::vec2 startPosition  = vmath::vec2(0.0f, 0.0f);   // 시작 위치 (XY 평면)
    float density              = 1.0f;
    float friction             = 0.3f;
    float linearDamping        = 5.0f;        // 키 떼면 즉시 정지 효과
    uint16_t categoryBits      = 0;           // filter::PLAYER 등 호출자가 명시
    uint16_t maskBits          = 0;           // filter::PLAYER_MASK 등
    bool     isSensor          = false;       // Unity Collider.isTrigger — Player 는 false (solid)
                                              // (2026-05-25 amendment)
};

LifeCfg       life;
MovementCfg   movement;
ControllerCfg controller;
PhysicsCfg    physics;   // 신규 — world 가 비-null 일 때만 PhysicsBody + PhysicsMovement 부착
```

- [ ] **Step 6.2: factory 본문에서 physics 분기 추가**

기존 `CreatePlayerActor` 본문의 Controller 부착 분기 *전* (또는 *대신*) 에 추가:

```cpp
if (cfg.physics.world != nullptr) {
    // Physics body 생성 — b2World 가 lifetime 소유.
    b2BodyDef bd;
    bd.type     = b2_dynamicBody;
    bd.position.Set(cfg.physics.startPosition[0], cfg.physics.startPosition[1]);
    bd.linearDamping = cfg.physics.linearDamping;
    b2Body* body = cfg.physics.world->CreateBody(&bd);

    b2PolygonShape box;
    box.SetAsBox(cfg.physics.size[0] * 0.5f, cfg.physics.size[1] * 0.5f);

    b2FixtureDef fd;
    fd.shape    = &box;
    fd.density  = cfg.physics.density;
    fd.friction = cfg.physics.friction;
    fd.isSensor = cfg.physics.isSensor;   // 2026-05-25 amendment — Unity isTrigger 위임
    fd.filter.categoryBits = cfg.physics.categoryBits;
    fd.filter.maskBits     = cfg.physics.maskBits;
    body->CreateFixture(&fd);

    // PhysicsBodyComponent 부착
    auto* pb = actor->AddComponent<Physics::PhysicsBodyComponent>();
    pb->SetBody(body);
    pb->SetSensor(cfg.physics.isSensor);   // Component 상태도 일관성 위해 동기화

    // PhysicsMovement 부착 (Movement 대체) — Controller 가 SetMovableTarget 으로 받을 대상
    auto* pm = actor->AddComponent<Physics::PhysicsMovement>(cfg.movement.speed);

    // Controller wiring — PhysicsMovement 가 IMovable target
    if (cfg.controller.keyboard != nullptr) {
        auto* controller = actor->AddComponent<Controller::PlayerController>();
        controller->SetKeyboardInput(cfg.controller.keyboard);
        controller->SetMovableTarget(pm);   // ← PhysicsMovement (IMovable 다중 상속)
        controller->SetUp();
    }
}
else {
    // 기존 분기 — physics 미사용 시 Movement Component (Transform 직접 조작)
    auto* movement = actor->AddComponent<Components::Movement>(cfg.movement.speed);
    if (cfg.controller.keyboard != nullptr) {
        auto* controller = actor->AddComponent<Controller::PlayerController>();
        controller->SetKeyboardInput(cfg.controller.keyboard);
        controller->SetMovableTarget(movement);
        controller->SetUp();
    }
}
```

- [ ] **Step 6.3: 새 include 추가 (PlayerActor.h 상단)**

```cpp
#include "<physics>/physics_body.h"
#include "<physics>/physics_movement.h"
#include <<box2d>/box2d.h>
```

---

## Task 7: 벽 4개 정적 body Factory

**Files:**
- Create: `<apps>/_MyApp_/src/physics/wall_factory.h` (or inline in main.cpp)

설계 선택:
- **옵션 A**: `wall_factory.h` 별도 파일 (compound_actor 정통)
- **옵션 B**: main.cpp 안에 인라인 lambda

권장 — **옵션 A** (재사용성).

- [ ] **Step 7.1: 헤더 작성**

```cpp
#ifndef __MYAPP_PHYSICS_WALL_FACTORY_H__
#define __MYAPP_PHYSICS_WALL_FACTORY_H__

#include "<physics>/filter.h"
#include "<physics>/physics_body.h"
#include "scene/actor.h"
#include <<box2d>/box2d.h>
#include <memory>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Physics
{
    /// @brief 정적 벽 Actor — b2_staticBody + box shape + PhysicsBodyComponent.
    /// @param world  PhysicsSystem.World() 주입
    /// @param center 벽 중심 (XY 평면)
    /// @param half   half-extents (box 절반 크기)
    inline std::unique_ptr<SJH::Scene::Actor> CreateWallActor(
        std::string name, b2World& world, vmath::vec2 center, vmath::vec2 half)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>(std::move(name));

        b2BodyDef bd;
        bd.type     = b2_staticBody;
        bd.position.Set(center[0], center[1]);
        b2Body* body = world.CreateBody(&bd);

        b2PolygonShape box;
        box.SetAsBox(half[0], half[1]);

        b2FixtureDef fd;
        fd.shape = &box;
        fd.isSensor = false;   // 벽은 solid — Unity isTrigger OFF 정통 (2026-05-25 amendment)
        fd.filter.categoryBits = Filter::WALL;
        fd.filter.maskBits     = Filter::WALL_MASK;
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<PhysicsBodyComponent>();
        pb->SetBody(body);
        pb->SetHeightOffset(0.0f);
        // pb->SetSensor(false)  — 이미 default false

        return actor;
    }
}

#endif // __MYAPP_PHYSICS_WALL_FACTORY_H__
```

- [ ] **Step 7.2: pickup_factory.h 신설 (Trigger 시각 검증용 — 2026-05-25 amendment)**

```cpp
#ifndef __MYAPP_PHYSICS_PICKUP_FACTORY_H__
#define __MYAPP_PHYSICS_PICKUP_FACTORY_H__

#include "<physics>/contact_interface.h"
#include "<physics>/filter.h"
#include "<physics>/physics_body.h"
#include "scene/actor.h"
#include <<box2d>/box2d.h>
#include <memory>
#include <<spdlog>/spdlog.h>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Physics
{
    /// @brief 픽업/감지 영역 Component — Trigger 이벤트만 로그.
    /// @details 본격 픽업 로직(인벤토리 추가 등) 은 M4 이후. M3 에선 시각/로그 검증용.
    class PickupTriggerLogger : public SJH::Scene::Component,
                                public IPhysicsContactListener
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

    /// @brief 정적 Sensor 박스 — Unity 의 isTrigger=true 영역과 동일.
    inline std::unique_ptr<SJH::Scene::Actor> CreatePickupActor(
        std::string name, b2World& world, vmath::vec2 center, vmath::vec2 half)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>(std::move(name));

        b2BodyDef bd;
        bd.type     = b2_staticBody;
        bd.position.Set(center[0], center[1]);
        b2Body* body = world.CreateBody(&bd);

        b2PolygonShape box;
        box.SetAsBox(half[0], half[1]);

        b2FixtureDef fd;
        fd.shape = &box;
        fd.isSensor = true;   // ← Unity isTrigger ON 정통
        fd.filter.categoryBits = Filter::PICKUP;
        // Pickup 은 Player 만 인식하면 됨 — 적/총알은 무시
        fd.filter.maskBits     = Filter::PLAYER;
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<PhysicsBodyComponent>();
        pb->SetBody(body);
        pb->SetSensor(true);

        // 로그용 트리거 리스너 부착
        actor->AddComponent<PickupTriggerLogger>();

        return actor;
    }
}

#endif // __MYAPP_PHYSICS_PICKUP_FACTORY_H__
```

---

## Task 8: main.cpp 통합

**Files:**
- Modify: `apps/_MyApp_/main.cpp`

- [ ] **Step 8.1: include + private 멤버 추가**

```cpp
#include "<physics>/physics_system.h"
#include "<physics>/wall_factory.h"
#include "apps/_MyApp_/src/Bootstrap/pickup_factory.h"     // 2026-05-25 amendment — Trigger 검증용
#include "<physics>/filter.h"
```

private 멤버 추가:
```cpp
TopdownShooter::Physics::PhysicsSystem mPhysics;
```

- [ ] **Step 8.2: startup() — PhysicsSystem.Init() + 벽 4개 생성**

기존 `dir.Enter()` *전* 에:

```cpp
mPhysics.Init();

// === 벽 4개 — 약 10×10 단위 영역 둘러쌈 ===
const float arena = 10.0f;
const float wallH = 0.5f;
auto wallTop    = TopdownShooter::Physics::CreateWallActor(
    "WallTop",    mPhysics.World(), vmath::vec2(0.0f, +arena), vmath::vec2(arena, wallH));
auto wallBottom = TopdownShooter::Physics::CreateWallActor(
    "WallBottom", mPhysics.World(), vmath::vec2(0.0f, -arena), vmath::vec2(arena, wallH));
auto wallLeft   = TopdownShooter::Physics::CreateWallActor(
    "WallLeft",   mPhysics.World(), vmath::vec2(-arena, 0.0f), vmath::vec2(wallH, arena));
auto wallRight  = TopdownShooter::Physics::CreateWallActor(
    "WallRight",  mPhysics.World(), vmath::vec2(+arena, 0.0f), vmath::vec2(wallH, arena));
dir.Root().AddChild(std::move(wallTop));
dir.Root().AddChild(std::move(wallBottom));
dir.Root().AddChild(std::move(wallLeft));
dir.Root().AddChild(std::move(wallRight));

// === Pickup 1개 — Trigger (isSensor=true) 시각 검증용 (2026-05-25 amendment) ===
// Player 가 위로(WASD W) 이동하면 +Y 방향이라 (0, +3) 위치 픽업과 통과 가능.
// Player 가 통과할 때 OnTriggerEnter / OnTriggerExit 로그가 stdout 에 찍히면 성공.
auto pickup = TopdownShooter::Physics::CreatePickupActor(
    "PickupTest", mPhysics.World(), vmath::vec2(0.0f, 3.0f), vmath::vec2(0.8f, 0.8f));
dir.Root().AddChild(std::move(pickup));
```

- [ ] **Step 8.3: startup() — PlayerActor factory 의 physics 분기 활성**

기존 `pac.controller.keyboard = &mKeyboard;` 다음에:
```cpp
pac.physics.world         = &mPhysics.World();
pac.physics.size          = vmath::vec2(1.0f, 1.0f);
pac.physics.startPosition = vmath::vec2(0.0f, 0.0f);
pac.physics.density       = 1.0f;
pac.physics.linearDamping = 5.0f;
pac.physics.categoryBits  = TopdownShooter::Physics::Filter::PLAYER;
pac.physics.maskBits      = TopdownShooter::Physics::Filter::PLAYER_MASK;
```

- [ ] **Step 8.4: render() — Physics Step + SyncToTransform 호출**

기존 `SJH::Scene::Director::Get().Update(dt)` 직후 + Material uUvRect 갱신 *전*:

```cpp
mPhysics.Step(dt);
mPhysics.SyncToTransform(SJH::Scene::Director::Get().GetScene());
```

⚠️ `Director::GetScene()` 의 정확한 API 명 — M3 진입 시 `<src>/scene/director.h` 확인 (가능성 있는 후보: `Director::Get().Root()` 의 부모 Scene 반환, 또는 별도 메서드).

- [ ] **Step 8.5: shutdown() — PhysicsSystem.Shutdown()**

기존 shutdown() 마지막에:
```cpp
mPhysics.Shutdown();
```

---

## Task 9: 빌드 + 시각 검증

- [ ] **Step 9.1: configure (변경 없으면 noop)**
```bash
cmake --preset ninja
```

- [ ] **Step 9.2: 빌드**
```bash
cmake --build --preset ninja --target _MyApp_
```

기대: `[N/N] Linking CXX executable apps/_MyApp_/_MyApp_`. 실패 시 가장 흔한 원인:
- `SJH::Scene::Scene::Root()` API 불일치 → physics_system.cpp 의 traverse 정정
- `Director::GetScene()` API 불일치 → main.cpp:Step 8.4 정정
- `Actor::GetChildren()` 시그니처 — `vector<unique_ptr<Actor>>` 또는 `vector<Actor*>` 둘 중 하나에 맞게 traverse 정정

- [ ] **Step 9.3: 실행 + 시각 검증**
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

검증 시나리오:
1. **Player WASD 이동** — 기존과 동일
2. **벽 충돌 정지 (Solid / isTrigger=false 검증)** — Player 가 arena 경계 (±10 units) 에 도달하면 멈춤. 키 계속 눌러도 벽을 통과 안 함. Unity `OnCollisionEnter` 정통.
3. **벽 미시각 확인** — 벽은 *MeshRenderer 없음* — 시각적 표시 안 됨. *충돌 정지로만 검증*. (디버그 시각화는 box2d_demo 의 `debug_draw.cpp` 패턴 참고 가능 — 본 plan 범위 외)
4. **Pickup Trigger 검증 (Sensor / isTrigger=true 검증, 2026-05-25 amendment)** — Player 를 (0, +3) 위치 픽업 박스로 W 키 진입.
   - 통과 가능 (충돌 안 함) ✓
   - stdout 에 `[Pickup] OnTriggerEnter — other='Player'` 로그 ✓
   - 픽업 박스 빠져나오면 `[Pickup] OnTriggerExit` 로그 ✓
   - Unity `OnTriggerEnter/Exit` 정통 동작 확인.
5. **두 dynamic body 충돌** *(선택)* — 두 번째 PlayerActor 또는 다른 dynamic Actor 생성해 충돌 시 튕김 (M4 의 적/총알 도입 시 자연 검증).

- [ ] **Step 9.4: 회귀 가드**

```bash
ctest --test-dir build_ninja -R "ComputeUVRect|StateMachineProcessor"
```
M1 ComputeUVRect 3/3 PASS 확인 (FSM 단위 테스트는 폐기됨).

---

## Task 10: Single commit

- [ ] **Step 10.1: 격리 확인**

```bash
git status --short
```

`apps/_MyApp_/src/physics/` + `apps/_MyApp_/src/CMakeLists.txt` + `apps/_MyApp_/CMakeLists.txt` + `apps/_MyApp_/src/Entity/Player/PlayerActor.h` + `apps/_MyApp_/main.cpp` 만 변경 확인.

- [ ] **Step 10.2: staging + commit**

```bash
git add apps/_MyApp_/src/physics/ \
        apps/_MyApp_/src/CMakeLists.txt \
        apps/_MyApp_/CMakeLists.txt \
        apps/_MyApp_/src/Entity/Player/PlayerActor.h \
        apps/_MyApp_/main.cpp
git commit -m "$(cat <<'EOF'
feat(_MyApp_): M3 Box2D v2.4.1 물리 통합 (Client 한정) + Unity isTrigger 패턴

- apps/_MyApp_/src/physics/ 신규 모듈 (MyApp::Physics STATIC + ALIAS)
  - filter.h: uint16 카테고리 (PLAYER/ENEMY/BULLET_*/WALL/PICKUP) + 자주 쓰는 mask 조합
  - contact_interface.h: IPhysicsContactListener — OnTriggerEnter/Exit/OnCollisionEnter/Exit
    (Unity MonoBehaviour::OnTrigger*/OnCollision* 정통 매핑)
  - physics_body.h: PhysicsBodyComponent — b2Body* non-owning + mIsSensor + heightOffset
                                            + SetSensor(bool) 런타임 변경 가능
  - contact_listener.h/.cpp: PhysicsContactListener : b2ContactListener
                              BeginContact/EndContact 받고 IsSensor 분기 →
                              양쪽 Actor 의 IPhysicsContactListener Component 디스패치
  - physics_system.h/.cpp: b2World owner + Step(dt, 8, 3) + SyncToTransform 재귀 traversal
                            + PhysicsContactListener 인스턴스 보유 + SetContactListener 설치
  - physics_movement.h: PhysicsMovement : Component, IMovable — body velocity 갱신
  - wall_factory.h: 정적 box body Actor — isSensor=false (Unity isTrigger OFF)
  - pickup_factory.h: 정적 sensor 박스 Actor + PickupTriggerLogger Component
                       — isSensor=true (Unity isTrigger ON)

- PlayerActor.h factory 확장:
  - PlayerActorConfig::PhysicsCfg nested struct
    (world / size / startPos / density / damping / filter / isSensor)
  - physics.world 비-null 시 PhysicsBody + PhysicsMovement 자동 부착
    (Controller 가 PhysicsMovement 를 IMovable target 으로 받음)
  - physics.world null 시 기존 Movement Component fallback (호환 유지)

- main.cpp:
  - mPhysics.Init() (ContactListener 자동 설치) + 벽 4개 (arena ±10) + Pickup 1개 (0, +3) 생성
  - PlayerActorConfig.physics 분기 활성
  - render(): Director.Update → mPhysics.Step(dt) → mPhysics.SyncToTransform(scene)
              (Step 단계에서 BeginContact/EndContact 콜백 자동 발생)
  - shutdown(): mPhysics.Shutdown()

좌표계: 물리 (x, y) → 렌더 (x, heightOffset, -y) — top-down Z 축 매핑 (spec §4.4)
충돌 (Solid):  Player 가 벽 4개에서 정지. 키 떼면 linearDamping=5 로 즉시 멈춤.
                stdout 에 OnCollisionEnter 로그.
트리거 (Sensor): Player 가 Pickup 박스 통과 가능 + OnTriggerEnter/Exit 로그.
                Unity Collider.isTrigger 정통 매핑 (Context7 Unity 문서 fetch 2026-05-25 확인).

회귀: M1 ComputeUVRect 3/3 PASS, 기존 WASD 이동 + Camera follow 작동 유지.

M3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

**Spec coverage (부록 D M3)**:
- [x] `myapp::PhysicsBodyComponent` — Task 3 (+ Unity isTrigger 매핑 `mIsSensor` / `SetSensor`)
- [x] `myapp::PhysicsSystem` + b2World — Task 4 (+ ContactListener 설치)
- [x] SyncToTransform — Task 4 의 cpp 구현
- [x] 벽 4개 정적 body (isSensor=false, Solid) — Task 7
- [x] Player 가 벽에서 멈춤 — Task 8 의 PhysicsMovement + linearDamping
- [x] **Sensor + ContactListener + 이벤트 인터페이스 (2026-05-25 amendment)** — Task 4.5 + Task 7 (pickup_factory)
- [x] 두 dynamic body 충돌 — *시각 검증 시나리오에 포함* (실제 두 번째 dynamic Actor 생성은 M4 의 Bullet/Enemy 도입 시)

**Spec 결정 일관성**:
- ✅ 결정 #18 — Box2D Client 한정 (`apps/_MyApp_/src/physics/`, `MyApp::Physics`)
- ✅ §4.1 v3 → v2.4.1 번역 — `b2World` 직접 보유 (v2 패턴), `b2Body*` 포인터, `b2FixtureDef + CreateFixture` 패턴
- ✅ §4.3 filter uint16
- ✅ §4.4 좌표계 (XY 물리 ↔ XZ 렌더, Y → -Z)
- ✅ §4.5 sensor + contact listener — *원래 M4 였으나 사용자 결정 2026-05-25 로 M3 흡수*
- ✅ Actor 비상속 컨벤션 (compound_actor 정통 — wall_factory.h / pickup_factory.h 모두 free factory)

**Unity Collider 매핑 일관성 (Context7 fetch 2026-05-25)**:
- ✅ `Collider.isTrigger = true` → `b2FixtureDef::isSensor = true` + `PhysicsBodyComponent.mIsSensor = true`
- ✅ `OnTriggerEnter/Exit(Collider other)` → `IContactable::OnTriggerEnter/Exit(Actor* other)`
- ✅ `OnCollisionEnter/Exit(Collision)` → `IContactable::OnCollisionEnter/Exit(Actor* other)`
- ✅ "최소 한 쪽 Rigidbody 필요" 규약 → Box2D 의 *정적-정적 콜백 없음* 과 일치

**2026-05-25 추가 결정 (실제 코드 검토 후)**:
- ✅ **인터페이스 명 확정** — `IPhysicsContactListener` → `IContactable` (실제 코드 `apps/_MyApp_/src/Physics/Components.Interfaces.h` 와 일치)
- ✅ **isTrigger 단일 source-of-truth** — `mIsSensor` 데이터만 유지, `GetIsTrigger()` 는 `Physics` 베이스에서 `final { return mIsSensor; }` 로 통일. 자식 override 금지. 데이터/함수 이중 표현으로 인한 dead Setter / 동기 어긋남 방지.
- ✅ **Projectile 부착 제약** — `Carrier::Projectile` 같은 Carrier Component 는 *Carrier 전용 Actor factory* 만 부착. Entity factory (`CreatePlayerActor` 등) 가 부착 시 컨벤션 위반. 런타임 가드 없이 *코드 리뷰 + factory 분리* 로 enforce.

**Placeholder scan**: 없음 — 모든 step 에 실제 코드 인라인. (PhysicsContactListener::Dispatch 의 Component 순회는 명시적 placeholder 로 표시됨)

**Type consistency**:
- `IMovable::DoForward(vec2, float)` — Movement / PhysicsMovement / PlayerEntity 일치 (M2 P2 commit `2eb4150` 정착)
- `b2Body*` non-owning — 모든 시그니처 일관
- `PhysicsBodyComponent` 멤버 이름 (`mBody`, `mHeightOffset`, `mIsSensor`) — header / setter / accessor 일관
- `IPhysicsContactListener` 4 콜백 시그니처 — `Actor*` 인자 통일

**Type 미확정 (M3 진입 시 정정 필요)**:
- `SJH::Scene::Scene::Root()` 반환 타입 — `Actor&` 가정 (확인 필요)
- `Director::GetScene()` API 명 — Director header 확인 후 정확한 명칭으로
- `Actor::GetChildren()` 컨테이너 타입 — `vector<unique_ptr<Actor>>` 가정
- **Actor 의 Component 순회 API** — `ForEachComponent` / `GetComponents` / `mComponents` 중 무엇인지 — Task 4.5 의 `Dispatch` 함수가 *placeholder* 로 두고 진입 시 정정 (2026-05-25 amendment)

이 4개는 M3 진입 시 *src/scene/* 의 정확한 헤더 정독 후 traverse / Step / Dispatch 호출 부분 정정.

---

## 후속 작업 (M3 plan 범위 외)

| 작업 | 마일스톤 | 비고 |
|---|---|---|
| ~~Contact listener (sensor + 피격)~~ | ~~M4~~ | ~~spec §4.5~~ → **M3 흡수 완료 (2026-05-25 amendment)** |
| Bullet/Enemy 의 IPhysicsContactListener 본격 활용 (피격 판정 → IDamageable.DoDamaged 호출) | M4 | M3 의 PickupTriggerLogger 패턴 확장 |
| Ray cast (Bullet hitscan) | M4 | spec §4.6 |
| Filter mask 본격 다대다 검증 (PLAYER/ENEMY/BULLET 모든 조합) | M4 | M3 에선 PLAYER/WALL/PICKUP 3개만 검증 |
| Debug draw (벽/Pickup 박스 시각화) | 선택 | box2d_demo 의 `debug_draw.cpp` 참고 |
| Bullet spawn cost 측정 (chrono) → 풀 도입 결정 | M4 | spec §부록 D M4 |
| PreSolve/PostSolve 콜백 활용 (충돌 응답 커스터마이즈) | M5+ | M3 의 PhysicsContactListener 확장 |

## 실제 정착 회고 (2026-05-25 완료)

M3 plan 본문의 micro-amendment 결정 + 본격 작업 후 spec 결정 #18 (`PhysicsBodyComponent` Client 한정 단일 클래스) 진화 5건 + 시각화 1건 + 빌드 잡음 청소 3건. 5 commit (`def527c` → `9aaa787`) 으로 정착.

### 1. body 컴포넌트 — 단일 → abstract base + concrete

| 시점 | 클래스 |
|---|---|
| spec 결정 #18 | `PhysicsBodyComponent` 단일 클래스 (`physics_body.h`) |
| `def527c` (M3 본격) | `PhysicsBodyComponent` 단일 (spec 그대로) |
| `538b317` (refactor) | `Components::Physics` abstract base + `BoxBody / CircleBody` concrete (`PhysicsComponent.h` + `PhysicsComponent.Imp.h`), `physics_body.h` 삭제 |

**이유**: shape 타입을 컴파일타임에 구분 → M4 총알 (`CircleBody`) 도입 시 자연 확장. Player/Wall/Pickup 모두 `BoxBody`.

### 2. 충돌 디스패치 인터페이스 — `IContactable` 단일화

micro-amendment (1) 결정의 *실제 적용* 과정.

`def527c` 에서 임시로 `IPhysicsContactListener` (별도 헤더) 신설했으나 `IContactable` (`Physics` 베이스 상속용) 과 *중복 + dispatch 가 절반만 동작* — `1a874f0` 에서 통합.

| 인터페이스 | dispatch 도달 (def527c) | 상태 (1a874f0 이후) |
|---|---|---|
| `IPhysicsContactListener` | ✓ (Pickup) | **삭제** |
| `IContactable` | ✗ (Projectile/BoxBody 콜백 안 옴) | dispatch 단일 인터페이스 ✓ |

**최종**: `IContactable` 단일. 4 콜백 default empty `{}` (Unity MonoBehaviour 정통 — 선택적 override).

### 3. Layer 필터 — uint16 constexpr → `enum class : uint64_t`

| 시점 | 형태 |
|---|---|
| spec 부록 D | `constexpr uint16_t PLAYER = 1u << 0;` 등 |
| `def527c` | `namespace Filter { constexpr uint16_t PLAYER; ... }` (spec 그대로) |
| `3ef1dab` | `enum class PhysicsLayer : uint64_t` + `operator\| / & / ~` + `ToBits()` 어댑터 |

**이유**: 강타입 (Unity `LayerMask` 정통, `Physics.IgnoreLayerCollision` 패턴). 미래 확장 여유 64-bit. Box2D 2.4.1 의 `b2Filter::categoryBits/maskBits` (uint16) 호환은 `ToBits(PhysicsLayer)` 어댑터로 narrowing.

### 4. Polymorphic body lookup — `FindPhysics(Actor*)` 헬퍼

spec/plan 미정의. `538b317` 에서 도입.

**문제**: `Actor::GetComponent<T>` 가 `std::type_index` 정확 매치 — `Components::Physics` abstract base 로 질의하면 `BoxBody/CircleBody` 못 잡음.

**해결**: `FindPhysics(Actor*)` = `ForEachComponent` + `dynamic_cast<Physics*>` first-match.

```cpp
inline Physics* FindPhysics(SJH::Scene::Actor* actor)
{
    if (!actor) return nullptr;
    Physics* found = nullptr;
    actor->ForEachComponent([&](SJH::Scene::Component* c) {
        if (!found) { if (auto* p = dynamic_cast<Physics*>(c)) found = p; }
    });
    return found;
}
```

호출처 2곳: `PhysicsMovement::OnEnter` (body 캐싱), `PhysicsSystem::SyncToTransform` (traverse).

### 5. Engine core 변경 — `scene/actor.h` 에 `ForEachComponent<Fn>` template 추가

spec/plan 미정의. `def527c` 에서 contact dispatch 가 모든 component 순회 필요해 engine core 에 추가.

```cpp
template<typename Fn>
void ForEachComponent(Fn&& fn) const
{
    for (auto& [ti, comp] : mComponents) fn(comp.get());
}
```

**책임 경계**: engine core 가 client 의 polymorphic dispatch 패턴 지원. 향후 다른 client 도메인 (ECS-style system iteration 등) 도 활용 가능.

### 추가 — 시각화 (plan 후속 작업 "Debug draw" 항목 대안 정착)

| 항목 | spec/plan | 실제 정착 (`9aaa787`) |
|---|---|---|
| 벽/Pickup 시각화 | Box2D debug draw (선택) | `apps/_MyApp_/resources/shaders/simple.vs/fs` (MVP + baseColor) + `Mesh::CreatePlane` XZ 평면 + Transform.Scale 매칭 (회색 wall + 노란 pickup) |

**이유**: 정식 게임 visual 의 시작점으로 자연 진화. M4 진입 시 wall/pickup 텍스처 교체만 필요. Box2D debug draw 는 별도 디버그 토글로 M5+ 보류.

### 빌드 잡음 청소 (post-execution)

| 항목 | 변경 |
|---|---|
| `physics_movement.cpp` empty placeholder | 삭제 + `apps/_MyApp_/src/Physics/CMakeLists.txt` source 리스트에서 제거 (`(no symbols)` 경고 제거) |
| `MyApp::Physics` 직접 link 중복 | `apps/_MyApp_/CMakeLists.txt` 에서 제거 — `MyApp::Client` umbrella 가 이미 포함 |
| 8개 SJH 코어 .a duplicate library 경고 | apple-clang `-Wl,-no_warn_duplicate_libraries` flag silencing (`SJH::engine` umbrella + `MyApp::Client` sub-modules PUBLIC propagation 다중 경로 — 본질 문제, link graph 재설계 필요) |

### micro-amendment 결정의 흡수 매핑

| micro-amendment 결정 | 흡수된 진화 항목 |
|---|---|
| (1) `IPhysicsContactListener` → `IContactable` 확정 | 진화 #2 ('충돌 디스패치 인터페이스 단일화') |
| (2) `mIsSensor` 단일 source-of-truth | 진화 #1 ('body 컴포넌트 abstract base'). `Physics` 베이스가 `mBody/mHeightOffset/mIsSensor` 모두 소유, `IsSensor()` getter 노출 |
| (3) `Carrier::Projectile` 부착 제약 | **미정착** — Projectile 자체는 헤더 스켈레톤만 존재, factory 분리는 M4 진입 시 |

---

## 변경 기록

| 일자 | 변경 |
|---|---|
| 2026-05-25 | 초안 — spec §4 정통 패턴 + M2 P2 의 IMovable 인터페이스 활용 + Compound Actor 컨벤션 유지 |
| 2026-05-25 (amendment) | **Sensor + ContactListener + IPhysicsContactListener 인터페이스 M3 흡수** — Unity `Collider.isTrigger` 정통 매핑 (Context7 Unity 문서 fetch). Task 4.5 신규 (`contact_interface.h` + `contact_listener.h/.cpp`). Task 3 의 `PhysicsBodyComponent` 에 `mIsSensor` + `SetSensor` 추가. Task 6 의 `PlayerActorConfig::PhysicsCfg` 에 `isSensor` 옵션. Task 7 에 `pickup_factory.h` + `PickupTriggerLogger` 신규. Task 8 에 Pickup Actor 1개 생성. Task 9 검증 시나리오에 Trigger 항목 추가. 사용자 결정 — Movement 상속 거부 (별도 `PhysicsMovement` Component, plan 원본 유지). |
| 2026-05-25 (micro-amendment) | **실제 코드 sync 후 결정 3종** — (1) 인터페이스 명 `IPhysicsContactListener` → `IContactable` 확정 (`apps/_MyApp_/src/Physics/Components.Interfaces.h` 와 일치). (2) isTrigger 단일 source-of-truth — `mIsSensor` 만 유지, `GetIsTrigger()` 는 `Physics` 베이스에서 `final { return mIsSensor; }` 통일, 자식 override 금지. (3) Projectile 부착 제약 — `Carrier::Projectile` 류는 Carrier 전용 factory 만 부착, Entity factory 부착 시 컨벤션 위반 (런타임 가드 없이 코드 리뷰 + factory 분리로 enforce). |
| 2026-05-25 (post-execution retro) | **실제 정착 회고 섹션 추가** — 5 commit (`def527c` → `9aaa787`) 산출 + spec 결정 #18 진화 5건 (body 단일 → abstract+concrete / `IContactable` 단일화 / `PhysicsLayer enum class : uint64_t` / `FindPhysics` polymorphic / engine `ForEachComponent` template) + 시각화 1건 (`simple.vs/fs` 단색 평면, Box2D debug draw 대신) + 빌드 잡음 청소 3건. micro-amendment 결정 (1)(2) 는 진화 #1/#2 에 흡수, (3) 은 M4 진입 시 정착 예정. |
