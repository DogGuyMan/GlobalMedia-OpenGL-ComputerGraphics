# SP3 — Actor + Component + RenderSystem + RenderQueue Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ECS 렌더링 아키텍처 재설계 SP1~SP4 의 SP3 — **Cocos2D `cc.Node`+`cc.Component` 정통 + Unreal `AActor`+`UActorComponent` 영감** 의 OOP Actor+Component 시스템 도입. `SJH::Scene` 싱글톤 (root Actor) + `Actor` 트리 + `Component` 라이프사이클 + `MeshRenderer` 통합 컴포넌트 + `RenderSystem` (Actor 트리 traverse) + `RenderQueue` (Multi-stage sort + Flush) + `MaterialApplier` 자유함수. Mesh/Material/Model 의 `Draw`/`Apply` *완전 폐기* (Pattern Y), `src/engine/` 레거시 삭제, `apps/ecs_demo` 챕터로 시각 검증.

**Architecture:** Cocos2D 우선 (`OnEnter`/`OnExit`/`Update` lifecycle, 트리 cascade) + Unreal 영감 (`Component::SetEnabled` 토글). Unity 식 `AddComponent<T>` 템플릿 + `MeshRenderer` 통합 컴포넌트 + Transform 내장. RAII (unique_ptr) 가 자원 수명 관리, OnExit 는 lifecycle hook 만 (destroy 안 함). Actor 트리가 부모-자식 Transform 계층 자연 흡수 (초안 SP3.5 항목 무료 흡수).

**Tech Stack:** C++17, CMake (Ninja preset), STL 만 (`<memory>`, `<unordered_map>`, `<vector>`, `<string>`, `<typeindex>`, `<typeinfo>`), Catch2 v3 단위 테스트, SP1+SP2 산출물 (Shader/Program/RenderContext/Uniforms). entt 미사용 (이미 제거됨).

**Spec:** [doc/superpowers/specs/2026-05-21-sp3-ecs-render-system-design.md](../specs/2026-05-21-sp3-ecs-render-system-design.md) (ddd 2 차 재검증 patch 적용 완료, `44395f1`)

---

## Summary

11 Task. 각 Task = 1 commit. 의존 순서 (T0 prerequisite → T1~T11).

| # | Task | 변경 파일 | 핵심 결과 | 검증 |
|---|---|---|---|---|
| **T0** | entt 잔재 cleanup (prerequisite) | `cmake/Dependency.cmake`, `.claude/CLAUDE.md` | `add_library(entt INTERFACE)` + game_deps 의 entt 제거, CLAUDE.md 텍스트 정정 | 빌드 PASS, grep 잔재 없음 |
| **T1** | `src/scene/` 모듈 + Actor + Component 베이스 | `src/scene/actor.{h,cpp}`, `src/scene/CMakeLists.txt`, `src/CMakeLists.txt` | Actor 트리 + Component lifecycle (OnEnter/OnExit/Update) + ddd 2차 검증 fix 5종 반영 | `static_assert` 비복사·비이동, `is_base_of_v` |
| **T2** | Scene::Director 싱글톤 + MeshRenderer 컴포넌트 | `src/scene/scene.{h,cpp}`, `src/scene/components.{h,cpp}` | `Scene::Director::Get()` 싱글톤 (Cocos cc::Director 정통, root Actor wrapper) + `MeshRenderer` 통합 컴포넌트 | `static_assert` Director 싱글톤 |
| **T3** | 자원 정리 — Mesh/Material/Model 의 Draw/Apply 폐기 | `src/object/mesh.{h,cpp}`, `src/material/material.{h,cpp}`, `src/object/model.{h,cpp}` | `Draw()`/`Apply()` 제거 + getter 노출 (`GetVAO`, `GetRenderUnits` 등). Pattern Y 정통 | `git grep "->Draw\|->Apply"` 0 건 |
| **T4** | `src/material/material_applier.{h,cpp}` 신설 | `src/material/material_applier.{h,cpp}`, `src/material/CMakeLists.txt` | Material 의 uniform/texture 적용을 자유함수 family 로 분리 (Q4-3) | 빌드 PASS |
| **T5** | `src/render/render_queue.{h,cpp}` 신설 | `src/render/render_queue.{h,cpp}`, `src/render/CMakeLists.txt` | DrawCommand 7 필드 + Multi-stage sort + Flush (program/material 전환 감지) | 빌드 PASS |
| **T6** | `src/render/render_system.{h,cpp}` 신설 | `src/render/render_system.{h,cpp}`, `src/render/CMakeLists.txt` | Actor 트리 traverse → MeshRenderer 수집 → DrawCommand → Queue Flush | 빌드 PASS |
| **T7** | `src/scene/model_spawner.{h,cpp}` 신설 | `src/scene/model_spawner.{h,cpp}`, `src/scene/CMakeLists.txt` | Assimp Model → N 자식 Actor 펼침 helper | 빌드 PASS |
| **T8** | `src/engine/` 전체 삭제 | (삭제) `src/engine/{scene_graph,transform,camera,model_base}.{h,cpp}`, `CMakeLists.txt` | 레거시 dead code 청소 | `git ls-files src/engine/` 비어야 함 |
| **T9** | Actor 라이프사이클 단위 테스트 | `<test>/test_actor_lifecycle.cpp`, `test/CMakeLists.txt` | Catch2 v3 — AddComponent 즉시 OnEnter, OnExit 역순 cascade, SetEnabled/Update 토글, AddComponent 중복 assert, AddChild(nullptr) | `ctest -R test_actor_lifecycle` PASS |
| **T10** | `apps/ecs_demo/` 챕터 + 셰이더 + 통합 검증 | `apps/ecs_demo/{main.cpp, CMakeLists.txt, resources/shaders/}`, `apps/CMakeLists.txt` | Box Actor + MeshRenderer + Scene → RenderSystem.Render 시각 동작 | 시각 회귀 smoke test |

**핵심 invariant** — 매 Task 종료 시:
- 전체 빌드 PASS (활성 챕터 + SP1+SP2 산출물 유지)
- SP1/SP2 의 public API byte-identical (Shader/Program/RenderContext/Uniforms::Set\*)
- `git grep "->Draw()|->Apply()"` 가 T3 종료 시점부터 0 건

**의존 그래프** — T0 (prereq) → T1 (foundation) → T2 (T1 필요) → T3 (독립, 자원 정리) → T4 (T3 후라야 의미) → T5 (T3/T4 필요: DrawCommand 필드용) → T6 (T2/T5 필요) → T7 (T1/T2 필요) → T8 (independent) → T9 (T1 필요) → T10 (모든 것 필요).

---

## Task 0: entt 잔재 cleanup (prerequisite)

**Files:**
- Modify: `cmake/Dependency.cmake` (entt INTERFACE + game_deps 항목 제거)
- Modify: `.claude/CLAUDE.md` (entt 언급 2 곳 정정)

- [ ] **Step 1: `cmake/Dependency.cmake` 의 entt 항목 제거**

Run:
```bash
grep -n "entt" cmake/Dependency.cmake
```
Expected: `add_library(entt INTERFACE)` 줄 + `game_deps` 의 `entt` 참조 줄 출력. 두 줄 모두 *제거* (entt 가 game_deps 의 마지막 항목이면 줄 자체 삭제, 중간이면 `entt` 토큰만 제거).

수정 후 다시 grep — 출력 없어야 함.

- [ ] **Step 2: `.claude/CLAUDE.md` 의 EnTT 언급 정정**

`.claude/CLAUDE.md:96` 부근의 `game_deps` 설명 문장에서 `+ EnTT` 토큰 제거:

```diff
- **`game_deps`** (INTERFACE) = Box2D + Effekseer + EffekseerRendererGL + assimp(+zlibstatic) + spdlog + EnTT + Tweeny + stb. ...
+ **`game_deps`** (INTERFACE) = Box2D + Effekseer + EffekseerRendererGL + assimp(+zlibstatic) + spdlog + Tweeny + stb. ...
```

같은 문장의 `헤더 온리(EnTT/Tweeny/stb)` → `헤더 온리(Tweeny/stb)`.

`.claude/CLAUDE.md:231` 부근의 서브모듈 목록에서 `/ extern/entt /` 토큰 제거.

- [ ] **Step 3: 빌드 통과 확인**

Run:
```bash
cmake --preset ninja 2>&1 | tail -5
cmake --build --preset ninja 2>&1 | tail -10
```
Expected: configure + build 성공. entt 미존재라 영향 없어야 함.

- [ ] **Step 4: 잔재 grep 검증**

Run:
```bash
grep -rln "entt\|EnTT" cmake/ .claude/CLAUDE.md 2>/dev/null
```
Expected: 출력 없음.

> 비활성 챕터 (`apps/_MyApp_`, `apps/_deptest_`) 의 entt 사용 코드는 *건드리지 않음* — 빌드 그래프 밖이라 영향 0, 향후 챕터 활성화 시 별도 마이그레이션.

- [ ] **Step 5: 커밋**

```bash
git add cmake/Dependency.cmake .claude/CLAUDE.md
git commit -m "[remove] : entt cmake/CLAUDE.md 잔재 정리 (SP3 prereq)

extern/entt 와 include/entt 는 이미 파일 시스템에서 제거됨.
남은 잔재만 정리:
- cmake/Dependency.cmake 의 add_library(entt INTERFACE) + game_deps entt 항목 제거
- .claude/CLAUDE.md 의 EnTT 언급 2 곳 정정 (game_deps 설명 + 서브모듈 목록)

비활성 챕터 (_MyApp_, _deptest_) 의 entt 코드는 그대로 — 빌드 그래프 밖.
"
```

---

## Task 1: `src/scene/` 모듈 + Actor + Component 베이스

**Files:**
- Create: `src/scene/actor.h`
- Create: `src/scene/actor.cpp`
- Create: `src/scene/CMakeLists.txt`
- Modify: `src/CMakeLists.txt` (`add_subdirectory(scene)` 추가)

- [ ] **Step 1: 신규 헤더 작성 — `src/scene/actor.h`**

```cpp
#ifndef __SJH_SCENE_ACTOR_H__
#define __SJH_SCENE_ACTOR_H__

#include "object/transform.h"
#include <cassert>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace SJH::Scene
{
    class Actor;

    /// @brief Cocos cc.Component 정통 + Unreal UActorComponent 영감 베이스.
    /// @details
    ///   ### Lifecycle (Cocos 정통)
    ///   - @c OnEnter — Actor 가 running 상태 진입 (Cocos onEnter / Unreal BeginPlay)
    ///   - @c OnExit  — running 에서 벗어남 (Cocos onExit / Unreal EndPlay)
    ///   - @c Update(dt) — 매 프레임 (Cocos update / Unreal TickComponent)
    ///   ### Enabled (Unreal 영감)
    ///   - @c SetEnabled(false) — Update 만 정지. OnEnter/OnExit 무관.
    class Component
    {
    public:
        virtual ~Component() = default;
        virtual void OnEnter() {}
        virtual void OnExit()  {}
        virtual void Update(float dt) { (void)dt; }

        bool IsEnabled() const { return mEnabled; }
        void SetEnabled(bool e) { mEnabled = e; }
        Actor* GetOwner() const { return mOwner; }

    private:
        friend class Actor;
        Actor* mOwner   = nullptr;
        bool   mEnabled = true;
    };

    /// @brief Cocos cc.Node 정통 — 트리 + flat 컴포넌트 리스트 + 내장 Transform.
    /// @details
    ///   ### 단일 호출 contract (D-9)
    ///   @c AddComponent / @c AddChild 가 entered 부모에 대해 *construct + insert + OnEnter* 를
    ///   한 호출에서 실행. 호출자가 이 contract 를 인지해야 함.
    class Actor
    {
    public:
        explicit Actor(std::string name = "");
        ~Actor();

        Actor(const Actor&)            = delete;
        Actor& operator=(const Actor&) = delete;
        Actor(Actor&&)                 = delete;
        Actor& operator=(Actor&&)      = delete;

        // === Tree ===
        Actor* AddChild(std::unique_ptr<Actor> child);
        void   RemoveChild(Actor* child);
        Actor* GetParent() const { return mParent; }
        const std::vector<std::unique_ptr<Actor>>& GetChildren() const { return mChildren; }

        // === Components ===
        template<typename T, typename... Args>
        T* AddComponent(Args&&... args);
        template<typename T> T*   GetComponent() const;
        template<typename T> void RemoveComponent();
        void RemoveAllComponents();

        // === Identity ===
        const std::string& GetName() const { return mName; }
        void SetName(std::string n) { mName = std::move(n); }

        // === Transform (Unity 내장) ===
        Transform&       GetTransform()       { return mTransform; }
        const Transform& GetTransform() const { return mTransform; }
        vmath::mat4      GetWorldMatrix() const;

        // === Active ===
        bool IsActive()  const { return mActive; }
        void SetActive(bool a) { mActive = a; }
        bool IsEntered() const { return mEntered; }

        // === Lifecycle ===
        void OnEnter();
        void OnExit();
        void Update(float dt);

    private:
        std::string mName;
        Actor*      mParent = nullptr;
        std::vector<std::unique_ptr<Actor>> mChildren;
        std::unordered_map<std::type_index, std::unique_ptr<Component>> mComponents;
        Transform   mTransform;
        bool        mActive  = true;
        bool        mEntered = false;
    };

    // === Template 정의 (ddd 2 차 patch 적용) ===

    template<typename T, typename... Args>
    T* Actor::AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        // 중복 추가 가드 — try_emplace 가 inserted=false 이면 기존 컴포넌트 OnExit 누락 위험
        auto [it, inserted] = mComponents.try_emplace(typeid(T), nullptr);
        assert(inserted && "Duplicate component type — Actor::AddComponent<T> called twice");

        it->second = std::make_unique<T>(std::forward<Args>(args)...);
        it->second->mOwner = this;
        T* raw = static_cast<T*>(it->second.get());
        if (mEntered) raw->OnEnter();
        return raw;
    }

    template<typename T>
    T* Actor::GetComponent() const
    {
        auto it = mComponents.find(typeid(T));
        return it != mComponents.end() ? static_cast<T*>(it->second.get()) : nullptr;
    }

    template<typename T>
    void Actor::RemoveComponent()
    {
        auto it = mComponents.find(typeid(T));
        if (it == mComponents.end()) return;
        if (mEntered) it->second->OnExit();   // symmetric — OnEnter 호출된 경우만 OnExit
        mComponents.erase(it);
    }
}

#endif // __SJH_SCENE_ACTOR_H__
```

- [ ] **Step 2: 구현 파일 작성 — `src/scene/actor.cpp`**

```cpp
#include "scene/actor.h"
#include <algorithm>
#include <type_traits>

// SP3 — Actor/Component 비복사·비이동 컴파일 타임 검증
static_assert(!std::is_copy_constructible_v<SJH::Scene::Actor>,
              "SJH::Scene::Actor must be non-copy-constructible");
static_assert(!std::is_move_constructible_v<SJH::Scene::Actor>,
              "SJH::Scene::Actor must be non-move-constructible");
static_assert(std::has_virtual_destructor_v<SJH::Scene::Component>,
              "SJH::Scene::Component must have virtual destructor (polymorphic base)");

namespace SJH::Scene
{
    Actor::Actor(std::string name) : mName(std::move(name)) {}
    Actor::~Actor() = default;

    Actor* Actor::AddChild(std::unique_ptr<Actor> child)
    {
        if (!child) return nullptr;   // ddd Early Return — nullptr 방어
        child->mParent = this;
        Actor* raw = child.get();
        mChildren.push_back(std::move(child));
        if (mEntered) raw->OnEnter();
        return raw;
    }

    void Actor::RemoveChild(Actor* child)
    {
        auto it = std::find_if(mChildren.begin(), mChildren.end(),
            [&](const auto& p) { return p.get() == child; });
        if (it == mChildren.end()) return;
        if (mEntered) (*it)->OnExit();
        mChildren.erase(it);
    }

    void Actor::RemoveAllComponents()
    {
        // ddd POLA: OnExit 는 enabled 무관 cleanup hook. mEntered 일 때만 호출.
        if (mEntered)
            for (auto& [_, comp] : mComponents)
                comp->OnExit();
        mComponents.clear();
    }

    vmath::mat4 Actor::GetWorldMatrix() const
    {
        const vmath::mat4 local = mTransform.GetLocalMatrix();
        return mParent ? mParent->GetWorldMatrix() * local : local;
    }

    void Actor::OnEnter()
    {
        if (mEntered) return;
        mEntered = true;
        for (auto& [_, comp] : mComponents)
            comp->OnEnter();
        for (auto& child : mChildren)
            child->OnEnter();
    }

    void Actor::OnExit()
    {
        if (!mEntered) return;
        for (auto it = mChildren.rbegin(); it != mChildren.rend(); ++it)
            (*it)->OnExit();
        for (auto& [_, comp] : mComponents)
            comp->OnExit();
        mEntered = false;
    }

    void Actor::Update(float dt)
    {
        if (!mActive) return;
        for (auto& [_, comp] : mComponents)
            if (comp->IsEnabled()) comp->Update(dt);
        for (auto& child : mChildren)
            child->Update(dt);
    }
}
```

- [ ] **Step 3: CMake 모듈 — `src/scene/CMakeLists.txt`**

```cmake
add_library(sjhopengl_scene STATIC
    actor.cpp
)
add_library(SJH::scene ALIAS sjhopengl_scene)

target_include_directories(sjhopengl_scene
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_scene
    PUBLIC  SJH::common SJH::object project_deps
)

target_compile_features(sjhopengl_scene PUBLIC cxx_std_17)
```

- [ ] **Step 4: `src/CMakeLists.txt` 에 `add_subdirectory(scene)` 추가**

`add_subdirectory(material)` 줄 *직후* 에 다음 한 줄 추가:

```cmake
add_subdirectory(scene)
```

(나머지 줄 무수정.)

- [ ] **Step 5: 빌드 통과 확인**

```bash
cmake --build --preset ninja --target sjhopengl_scene 2>&1 | tail -10
```
Expected: PASS — `libsjhopengl_scene.a` 생성, static_assert 3 종 통과.

- [ ] **Step 6: 커밋**

```bash
git add src/scene/ src/CMakeLists.txt
git commit -m "[feat] : src/scene/ 모듈 + Actor + Component 베이스 (SP3 T1)

Cocos2D cc.Node + cc.Component 정통 + Unreal AActor + UActorComponent 영감.
ddd 2 차 재검증 patch 5 종 모두 반영:
- AddComponent 중복 추가 try_emplace + assert (silent overwrite 방지)
- RemoveComponent symmetric mEntered 가드
- AddChild(nullptr) early return
- RemoveAllComponents enabled 무관 + mEntered 가드
- 단일 호출 contract (construct + insert + OnEnter) doxygen 명시

static_assert 3 종 — Actor 비복사/비이동, Component virtual dtor.
"
```

---

## Task 2: Scene 싱글톤 + MeshRenderer 컴포넌트

**Files:**
- Create: `src/scene/scene.h`
- Create: `src/scene/scene.cpp`
- Create: `<src>/scene/components.h`
- Modify: `src/scene/CMakeLists.txt` (scene.cpp 추가)

- [ ] **Step 1: Scene::Director 싱글톤 헤더 — `src/scene/scene.h`**

> **C++ 충돌 방지** — `namespace SJH::Scene` 이 이미 Actor/Component 의 컨테이너로 사용 중. 싱글톤 클래스명을 `Scene` 으로 하면 *namespace 와 class 가 같은 식별자* 라 충돌. **Cocos `cc::Director` 정통 명명** 으로 fix.

```cpp
#ifndef __SJH_SCENE_H__
#define __SJH_SCENE_H__

#include "scene/actor.h"

namespace SJH::Scene
{
    /// @brief Cocos cc::Director 정통 — root Actor 보유 싱글톤. SP2 RenderContext::Get() 패턴과 일관.
    /// @details `namespace SJH::Scene` 의 nested 싱글톤 — 사용: `SJH::Scene::Director::Get().Root()`.
    class Director
    {
    public:
        static Director& Get();

        Actor&       Root()       { return mRoot; }
        const Actor& Root() const { return mRoot; }

        void Enter()             { mRoot.OnEnter(); }
        void Exit()              { mRoot.OnExit(); }
        void Update(float dt)    { mRoot.Update(dt); }

        Director(const Director&)            = delete;
        Director& operator=(const Director&) = delete;
        Director(Director&&)                 = delete;
        Director& operator=(Director&&)      = delete;

    private:
        Director() : mRoot("WorldRoot") {}
        ~Director() = default;

        Actor mRoot;
    };
}

#endif // __SJH_SCENE_H__
```

- [ ] **Step 2: Director 구현 — `src/scene/scene.cpp`**

```cpp
#include "scene/scene.h"
#include <type_traits>

static_assert(!std::is_copy_constructible_v<SJH::Scene::Director>,
              "SJH::Scene::Director must be non-copy-constructible (singleton)");
static_assert(!std::is_move_constructible_v<SJH::Scene::Director>,
              "SJH::Scene::Director must be non-move-constructible (singleton)");

namespace SJH::Scene
{
    Director& Director::Get()
    {
        static Director instance;   // Meyer's singleton (thread-safe in C++11+)
        return instance;
    }
}
```

- [ ] **Step 3: MeshRenderer 컴포넌트 — `<src>/scene/components.h`**

```cpp
#ifndef __SJH_SCENE_COMPONENTS_H__
#define __SJH_SCENE_COMPONENTS_H__

#include "scene/actor.h"

namespace SJH
{
    class Mesh;
    class Material;
}

namespace SJH::Scene
{
    /// @brief Unity MeshRenderer 식 통합 컴포넌트 — Mesh + Material + Visible + QueueLayer.
    /// @details RenderSystem 이 이 컴포넌트를 수집 → DrawCommand 빌드.
    class MeshRenderer : public Component
    {
    public:
        MeshRenderer() = default;
        MeshRenderer(const SJH::Mesh* mesh, const SJH::Material* material,
                     int queueLayer = 2000)
            : Mesh(mesh), Material(material), QueueLayer(queueLayer) {}

        // 모두 public Pascal — POD 식 데이터 (Unity convention).
        // 필드명이 클래스명과 같아 정의에서 SJH:: 한정자 사용.
        const SJH::Mesh*     Mesh       = nullptr;
        const SJH::Material* Material   = nullptr;
        bool                 Visible    = true;
        int                  QueueLayer = 2000;   // Unity: 2000=Opaque, 3000=Transparent
    };
}

#endif // __SJH_SCENE_COMPONENTS_H__
```

- [ ] **Step 4: CMake 갱신 — `src/scene/CMakeLists.txt`**

`add_library(sjhopengl_scene STATIC` 의 소스 목록에 `scene.cpp` 추가:

```cmake
add_library(sjhopengl_scene STATIC
    actor.cpp
    scene.cpp
)
```

(나머지 줄 무수정.)

- [ ] **Step 5: 빌드 통과 확인**

```bash
cmake --build --preset ninja --target sjhopengl_scene 2>&1 | tail -10
```
Expected: PASS — Scene + MeshRenderer 컴파일, static_assert 5 종 (Actor 3 + Scene 2) 통과.

- [ ] **Step 6: 커밋**

```bash
git add src/scene/scene.h src/scene/scene.cpp <src>/scene/components.h src/scene/CMakeLists.txt
git commit -m "[feat] : Scene::Director 싱글톤 + MeshRenderer 통합 컴포넌트 (SP3 T2)

Cocos cc::Director 정통 — namespace SJH::Scene 안에 nested 싱글톤.
namespace 와 class 같은 식별자 충돌 회피 (Scene 은 namespace, Director 는 class).
사용: SJH::Scene::Director::Get().Root().AddChild(...).

Unity MeshRenderer 식 — Mesh + Material + Visible + QueueLayer 통합.
static_assert — Director 비복사·비이동 (싱글톤).
"
```

---

## Task 3: 자원 정리 — Mesh/Material/Model 의 Draw/Apply 폐기

**Files:**
- Modify: `src/object/mesh.h`, `src/object/mesh.cpp`
- Modify: `src/material/material.h`, `<src>/material/material.cpp`
- Modify: `src/object/model.h`, `src/object/model.cpp`

- [ ] **Step 1: `Mesh::Draw()` 제거 + `GetVAO()` 추가 — `src/object/mesh.h`**

`Mesh` 클래스의 `void Draw() const;` 선언 *완전 삭제*.

신규 `GetVAO()` getter 추가 (기존 `GetVertexBuffer`/`GetIndexBuffer`/`GetPrimitiveType` 옆):

```cpp
        /// @brief VAO GL 핸들 — RenderContext::BindVAO 인자.
        GLuint GetVAO() const;

        /// @brief 인덱스 개수 — RenderContext::DrawIndexed 인자.
        GLsizei GetIndexCount() const;
```

- [ ] **Step 2: `Mesh::Draw()` 정의 제거 + getter 구현 — `src/object/mesh.cpp`**

`void Mesh::Draw() const { ... }` 정의 *완전 삭제*.

신규 getter 정의 추가 (cpp 하단):

```cpp
GLuint Mesh::GetVAO() const
{
    return mVertexLayout ? mVertexLayout->GetVAO() : 0;
}

GLsizei Mesh::GetIndexCount() const
{
    return mIndexBuffer ? static_cast<GLsizei>(mIndexBuffer->GetCount()) : 0;
}
```

(`VertexLayout::GetVAO()` 와 `Buffer::GetCount()` 는 기존 존재 — 미존재 시 추가 필요. 본 task 의 step 2.5 로 확인.)

- [ ] **Step 2.5: 의존 getter 존재 확인**

```bash
grep -n "GetVAO\|GetCount" src/layout/vertex_layout.h src/buffer/buffer.h 2>/dev/null
```
Expected: 둘 다 declared. 만약 미존재 시 *해당 클래스에 getter 추가* (별도 commit 가능하나 본 task 에 통합 권장).

- [ ] **Step 3: `Material::Apply()` 제거 — `src/material/material.h`**

`void Apply() const;` 선언 *완전 삭제*.

```cpp
        // (이전) /// @brief 보유 프로그램에 머티리얼 상태 일괄 적용.
        // (이전) void Apply() const;
        // SP3 — 제거됨. MaterialApplier 자유함수가 흡수.
```

기존 getter 모두 그대로 유지 (`GetDiffuseTexture/Unit`, `GetSpecularTexture/Unit`, `GetShininess`, `GetProgram`).

- [ ] **Step 4: `Material::Apply()` 정의 제거 — `<src>/material/material.cpp`**

`void Material::Apply() const { ... }` 정의 *완전 삭제*. 다른 코드 무수정.

- [ ] **Step 5: `Model::Draw()` 제거 + `GetRenderUnits()` 노출 — `src/object/model.h`**

`void Draw() const;` 선언 *완전 삭제*.

신규 `GetRenderUnits()` 추가:

```cpp
        /// @brief 모든 RenderUnit 의 const view — ModelSpawner 가 Actor 펼침 시 사용.
        const std::vector<RenderUnit>& GetRenderUnits() const { return mRenderUnit; }
```

- [ ] **Step 6: `Model::Draw()` 정의 제거 — `src/object/model.cpp`**

`void Model::Draw() const { ... }` 정의 *완전 삭제*.

- [ ] **Step 7: 빌드 통과 확인**

```bash
cmake --build --preset ninja --target sjhopengl_object sjhopengl_material 2>&1 | tail -10
```
Expected: PASS.

- [ ] **Step 8: legacy `->Draw()`/`->Apply()` 잔존 확인**

```bash
git grep -n "\->Draw()\|\->Apply()" -- 'src/' 'apps/' 'test/' 2>/dev/null | grep -v 'sb7' | grep -v 'samples/'
echo "---"
echo "위 출력이 없거나 sb7/samples 만이면 Pattern Y 정통 정합"
```
Expected: 출력 없음 (활성 소스).

- [ ] **Step 9: 커밋**

```bash
git add src/object/mesh.h src/object/mesh.cpp src/material/material.h <src>/material/material.cpp src/object/model.h src/object/model.cpp
git commit -m "[refactor] : Mesh/Material/Model 의 Draw/Apply 폐기 — Pattern Y 정통 (SP3 T3)

자원 클래스는 *순수 데이터*. GL 호출은 RenderContext 게이트웨이 + 향후
RenderSystem 이 수행. SP2 의 Pattern Y 결정 완성.

- Mesh::Draw() 제거 → GetVAO() / GetIndexCount() getter 노출
- Material::Apply() 제거 → 기존 getter family 유지 (MaterialApplier 가 흡수)
- Model::Draw() 제거 → GetRenderUnits() 노출 (ModelSpawner 가 펼침)

활성 빌드 타깃에서 ->Draw / ->Apply 호출 0 건 확인.
"
```

---

## Task 4: `src/render/material_applier.{h,cpp}` 신설

> **2026-05-21 patch (T5 follow-up)**: 초기 T4 가 `src/material/` 에 배치했으나 `material → render` 의존이 도입되어 T5 에서 `render → material` 의존 추가 시 *순환 의존* 발생. ddd Separation of Concerns 위반 + MSVC 링크 위험 → MaterialApplier 는 *GL 호출이 본업* 이므로 `src/render/` 로 이전하여 단방향 의존 (`render → material`) 회복. 아래 paths/CMake 가 정정된 최종형.

**Files:**
- Create: `<src>/render/material_applier.h`
- Create: `<src>/render/material_applier.cpp`
- Modify: `src/render/CMakeLists.txt` (`src/material/CMakeLists.txt` 는 T5 follow-up 에서 정리)

- [ ] **Step 1: 헤더 — `<src>/material/material_applier.h`**

```cpp
#ifndef __SJH_MATERIAL_APPLIER_H__
#define __SJH_MATERIAL_APPLIER_H__

namespace SJH
{
    class Program;
    class Material;
    class RenderContext;

    /// @brief Material 의 uniform/texture 적용을 *순수 데이터 → GL 호출* 로 분리하는 helper.
    /// @details Material 은 순수 데이터 (D-1). MaterialApplier 가 그 데이터를 읽어
    ///          Uniforms::Set* 와 RenderContext::BindTexture 발행 (Q4-3 분리).
    namespace MaterialApplier
    {
        /// @brief Material 의 sampler unit (diffuse/specular) + shininess uniform 송신.
        /// @note  prog 가 미리 bound (UseProgram) 되어 있어야 함 — POLA 호출자 책임.
        void WriteUniforms(const Program& prog, const Material& material);

        /// @brief diffuse/specular 텍스처를 sampler unit 에 바인딩.
        void BindTextures(RenderContext& rc, const Material& material);
    }
}

#endif // __SJH_MATERIAL_APPLIER_H__
```

- [ ] **Step 2: 구현 — `<src>/render/material_applier.cpp`**

```cpp
#include "<render>/material_applier.h"
#include "material/material.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "<render>/render_context.h"
#include "src/texture/texture.h"

namespace SJH::MaterialApplier
{
    void WriteUniforms(const Program& prog, const Material& material)
    {
        Uniforms::SetInt  (prog, "material.diffuse",   material.GetDiffuseUnit());
        Uniforms::SetInt  (prog, "material.specular",  material.GetSpecularUnit());
        Uniforms::SetFloat(prog, "material.shininess", material.GetShininess());
    }

    void BindTextures(RenderContext& rc, const Material& material)
    {
        // GLint (Material 의 unit) → GLuint (RenderContext::BindTexture) 명시 캐스트.
        // sampler unit 은 음수일 수 없으나 Material 이 GLint 보관 (sampler location convention).
        if (auto* tex = material.GetDiffuseTexture())
            rc.BindTexture(static_cast<GLuint>(material.GetDiffuseUnit()), tex->GetTextureID());
        if (auto* tex = material.GetSpecularTexture())
            rc.BindTexture(static_cast<GLuint>(material.GetSpecularUnit()), tex->GetTextureID());
    }
}
```

(`Texture::GetTextureID()` 는 기존 inline getter — `src/texture/texture.h:61` 에 선언.
`Material::GetDiffuseUnit()` / `GetSpecularUnit()` 은 `GLint` 반환이므로 `GLuint` 로 명시 캐스트.)

- [ ] **Step 3: CMake 갱신 — `src/render/CMakeLists.txt`** (T5 follow-up 통합형)

material_applier 이 render 모듈에 합류 + 기존 render_queue 합산:

```cmake
add_library(sjhopengl_render STATIC
    render_target.cpp
    render_context.cpp
    render_queue.cpp
    material_applier.cpp
)
add_library(SJH::render ALIAS sjhopengl_render)

target_include_directories(sjhopengl_render
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# material_applier 는 GL 호출이 본업이므로 render 모듈 소속 (SP3 T5 follow-up).
# 공개 헤더(render_queue.h / material_applier.h) 는 Material/Texture forward decl 만 →
# SJH::material + SJH::resource_registry 는 PRIVATE.
target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common SJH::program project_deps
    PRIVATE SJH::diagnostics SJH::material SJH::object SJH::resource_registry
)

target_compile_features(sjhopengl_render PUBLIC cxx_std_17)
```

`src/material/CMakeLists.txt` 는 *반대로 단순화* — material.cpp 만 남고 SJH::render 의존 0 (단방향 회복):

```cmake
add_library(sjhopengl_material STATIC
    material.cpp
)
add_library(SJH::material ALIAS sjhopengl_material)

target_include_directories(sjhopengl_material
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# material.h 는 Program/Texture 를 전방선언만 함 → PUBLIC 은 common/glad 뿐.
# material.cpp 는 빈 TU (SP3 T3: Apply() 폐기) — link 의존 0.
# MaterialApplier 는 GL 호출이 본업이므로 SP3 T5 follow-up 에서 SJH::render 로 이전 →
# 이로써 material → render 단방향 의존이 사라지고 material 이 순수 leaf 모듈로 회귀.
target_link_libraries(sjhopengl_material
    PUBLIC  SJH::common
            project_deps
)

target_compile_features(sjhopengl_material PUBLIC cxx_std_17)
```

- [ ] **Step 4: 빌드 통과 확인**

```bash
cmake --build --preset ninja --target sjhopengl_material 2>&1 | tail -10
```
Expected: PASS.

- [ ] **Step 5: 커밋**

```bash
git add <src>/material/material_applier.h <src>/material/material_applier.cpp src/material/CMakeLists.txt
git commit -m "[feat] : MaterialApplier 자유함수 family 신설 (SP3 T4)

Q4-3 분리 — Material 순수 데이터 유지 + uniform/texture 적용을 자유함수로.
- WriteUniforms(prog, mat) — sampler unit + shininess uniform
- BindTextures(rc, mat) — diffuse/specular 텍스처 → sampler unit 바인딩

RenderQueue::Flush 가 호출. ddd Separation of Concerns + Material 순수성 유지.
"
```

---

## Task 5: `src/render/render_queue.{h,cpp}` 신설

**Files:**
- Create: `<src>/render/render_queue.h`
- Create: `<src>/render/render_queue.cpp`
- Modify: `src/render/CMakeLists.txt`

- [ ] **Step 1: 헤더 — `<src>/render/render_queue.h`**

```cpp
#ifndef __SJH_RENDER_QUEUE_H__
#define __SJH_RENDER_QUEUE_H__

#include <vmath.h>
#include <cstddef>
#include <vector>

namespace SJH::Scene { class Actor; }
namespace SJH
{
    class Program;
    class Mesh;
    class Material;
    class RenderContext;

    /// @brief Cocos 식 Layer A — 한 프레임의 정렬 가능한 draw command (7 필드, Q2-3).
    struct DrawCommand
    {
        const Program*       program     = nullptr;
        const Mesh*          mesh        = nullptr;
        const Material*      material    = nullptr;
        vmath::mat4          modelMatrix = vmath::mat4::identity(); ///< 미지정 시 항등 — 디버그 가능 default.
        int                  queueLayer  = 2000;
        const Scene::Actor*  actor       = nullptr;   ///< 디버그 추적
        float                depth       = 0.0f;      ///< view-space z (back-to-front)
    };

    class RenderQueue
    {
    public:
        void Submit(const DrawCommand& cmd) { mItems.push_back(cmd); }
        void Clear()                        { mItems.clear(); }
        std::size_t Size() const            { return mItems.size(); }

        /// @brief Multi-stage sort: queueLayer → program → material → depth (back-to-front).
        void SortMultiStage();

        /// @brief 정렬된 command 발행.
        /// @details program 전환 시 UseProgram + view/proj uniform. material 전환 시
        ///          MaterialApplier::WriteUniforms + BindTextures. per-draw 는 model matrix.
        void Flush(RenderContext& rc,
                   const vmath::mat4& viewMat,
                   const vmath::mat4& projMat);

    private:
        std::vector<DrawCommand> mItems;
    };
}

#endif // __SJH_RENDER_QUEUE_H__
```

- [ ] **Step 2: 구현 — `<src>/render/render_queue.cpp`**

```cpp
#include "<render>/render_queue.h"
#include "<render>/render_context.h"
#include "<render>/material_applier.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "object/mesh.h"
#include "material/material.h"
#include <algorithm>
#include <functional>

namespace SJH
{
    void RenderQueue::SortMultiStage()
    {
        // 포인터 비교는 std::less<> 로 — raw pointer < 는 서로 다른 객체 간 UB (C++ [expr.rel]).
        // std::less<> 는 모든 포인터에 total order 를 보장.
        std::stable_sort(mItems.begin(), mItems.end(),
            [](const DrawCommand& a, const DrawCommand& b) {
                if (a.queueLayer != b.queueLayer) return a.queueLayer < b.queueLayer;
                if (a.program    != b.program)    return std::less<const Program*>{}(a.program, b.program);
                if (a.material   != b.material)   return std::less<const Material*>{}(a.material, b.material);
                return a.depth > b.depth;   // back-to-front (큰 depth 가 먼저)
            });
    }

    void RenderQueue::Flush(RenderContext& rc,
                            const vmath::mat4& viewMat,
                            const vmath::mat4& projMat)
    {
        const Program*  lastProg = nullptr;
        const Material* lastMat  = nullptr;

        for (const auto& cmd : mItems)
        {
            if (!cmd.program || !cmd.mesh || !cmd.material) continue;

            if (cmd.program != lastProg) {
                rc.UseProgram(*cmd.program);
                Uniforms::SetMat4(*cmd.program, "uView", viewMat);
                Uniforms::SetMat4(*cmd.program, "uProj", projMat);
                lastProg = cmd.program;
                lastMat  = nullptr;   // program 바뀌면 material 재바인딩 강제
            }
            if (cmd.material != lastMat) {
                MaterialApplier::WriteUniforms(*cmd.program, *cmd.material);
                MaterialApplier::BindTextures(rc, *cmd.material);
                lastMat = cmd.material;
            }
            Uniforms::SetMat4(*cmd.program, "uModel", cmd.modelMatrix);
            rc.BindVAO(cmd.mesh->GetVAO());
            rc.DrawIndexed(cmd.mesh->GetIndexCount());
        }
    }
}
```

- [ ] **Step 3: CMake 갱신 — `src/render/CMakeLists.txt`** (T5 follow-up 통합형)

T4 의 material_applier 이전과 합쳐서 sources 4개 (render_target/render_context/render_queue/material_applier) + PRIVATE 4개 (diagnostics/material/object/resource_registry):

```cmake
add_library(sjhopengl_render STATIC
    render_target.cpp
    render_context.cpp
    render_queue.cpp
    material_applier.cpp
)

# material_applier 는 GL 호출이 본업이므로 render 모듈 소속 (SP3 T5 follow-up).
# 공개 헤더(render_queue.h / material_applier.h) 는 Material/Texture forward decl 만 →
# SJH::material + SJH::resource_registry 는 PRIVATE.
target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common SJH::program project_deps
    PRIVATE SJH::diagnostics SJH::material SJH::object SJH::resource_registry
)
```

(material_applier 이전 + `SJH::material` / `SJH::object` / `SJH::resource_registry` PRIVATE 추가. `material → render` 순환 의존 해소.)

- [ ] **Step 4: 빌드 통과 확인**

```bash
cmake --build --preset ninja --target sjhopengl_render 2>&1 | tail -10
```
Expected: PASS.

- [ ] **Step 5: 커밋**

```bash
git add <src>/render/render_queue.h <src>/render/render_queue.cpp src/render/CMakeLists.txt
git commit -m "[feat] : RenderQueue + DrawCommand 신설 — Cocos Layer A (SP3 T5)

DrawCommand 7 필드 (Q2-3): program/mesh/material/modelMatrix/queueLayer/actor/depth.
Multi-stage sort (Q3-4): queueLayer → program → material → depth (back-to-front).
Flush: program/material 전환 감지 → UseProgram/WriteUniforms/BindTextures 최소화.
per-draw 는 model matrix 만. view/proj 는 program 전환 시 1 회.
"
```

---

## Task 6: `src/render/render_system.{h,cpp}` 신설

**Files:**
- Create: `<src>/render/render_system.h`
- Create: `<src>/render/render_system.cpp`
- Modify: `src/render/CMakeLists.txt`

- [ ] **Step 1: 헤더 — `<src>/render/render_system.h`**

```cpp
#ifndef __SJH_RENDER_SYSTEM_H__
#define __SJH_RENDER_SYSTEM_H__

#include "<render>/render_queue.h"
#include <vmath.h>

namespace SJH::Scene { class Actor; }
namespace SJH
{
    /// @brief Actor 트리 traverse → MeshRenderer 수집 → DrawCommand → Queue Flush.
    /// @details per-frame 호출:
    ///   1. Scene::Root() 부터 DFS — MeshRenderer 컴포넌트 수집
    ///   2. Actor::GetWorldMatrix() = model, depth = (view × model)[3].z
    ///   3. Queue 정렬 (Multi-stage)
    ///   4. Queue Flush → RenderContext
    ///
    ///   SP3.5 의 CameraComponent 도입 시 view/proj 인자 없는 overload 추가.
    class RenderSystem
    {
    public:
        void Render(const vmath::mat4& viewMat, const vmath::mat4& projMat);

    private:
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat);

        RenderQueue mQueue;
    };
}

#endif // __SJH_RENDER_SYSTEM_H__
```

- [ ] **Step 2: 구현 — `<src>/render/render_system.cpp`**

```cpp
#include "<render>/render_system.h"
#include "<render>/render_context.h"
#include "scene/scene.h"
#include "<scene>/components.h"
#include "material/material.h"

namespace SJH
{
    void RenderSystem::Render(const vmath::mat4& viewMat, const vmath::mat4& projMat)
    {
        auto& rc = RenderContext::Get();
        mQueue.Clear();
        CollectFromActor(Scene::Director::Get().Root(), viewMat);
        mQueue.SortMultiStage();
        mQueue.Flush(rc, viewMat, projMat);
    }

    void RenderSystem::CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat)
    {
        if (!actor.IsActive()) return;

        if (auto* mr = actor.GetComponent<Scene::MeshRenderer>())
        {
            if (mr->IsEnabled() && mr->Visible && mr->Mesh && mr->Material)
            {
                const vmath::mat4 model   = actor.GetWorldMatrix();
                const vmath::vec4 viewPos = (viewMat * model) * vmath::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                mQueue.Submit({
                    mr->Material->GetProgram(),
                    mr->Mesh,
                    mr->Material,
                    model,
                    mr->QueueLayer,
                    &actor,
                    viewPos[2]
                });
            }
        }

        for (const auto& child : actor.GetChildren())
            CollectFromActor(*child, viewMat);
    }
}
```

- [ ] **Step 3: CMake 갱신 — `src/render/CMakeLists.txt`**

소스 목록에 `render_system.cpp` 추가. PRIVATE 의존에 `SJH::scene` 추가:

```cmake
add_library(sjhopengl_render STATIC
    render_target.cpp
    render_context.cpp
    render_queue.cpp
    render_system.cpp
)

target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common SJH::program project_deps
    PRIVATE SJH::diagnostics SJH::material SJH::object SJH::scene
)
```

- [ ] **Step 4: 빌드 통과 확인**

```bash
cmake --build --preset ninja --target sjhopengl_render 2>&1 | tail -10
```
Expected: PASS.

- [ ] **Step 5: 커밋**

```bash
git add <src>/render/render_system.h <src>/render/render_system.cpp src/render/CMakeLists.txt
git commit -m "[feat] : RenderSystem — Actor 트리 traverse → Queue Flush (SP3 T6)

Scene::Root() 부터 DFS, MeshRenderer 컴포넌트 수집해 DrawCommand 빌드.
Actor::GetWorldMatrix() 로 부모-자식 합성 자동 적용. depth = view-space z.

SP3.5 의 CameraComponent 도입 시 view/proj 인자 없는 overload 추가 예정 (G1 seam).
"
```

---

## Task 7: `src/scene/model_spawner.{h,cpp}` 신설

**Files:**
- Create: `apps/_MyApp_/src/Bootstrap/model_spawner.h`
- Create: `apps/_MyApp_/src/Bootstrap/model_spawner.cpp`
- Modify: `src/scene/CMakeLists.txt`

- [ ] **Step 1: 헤더 — `apps/_MyApp_/src/Bootstrap/model_spawner.h`**

```cpp
#ifndef __SJH_SCENE_MODEL_SPAWNER_H__
#define __SJH_SCENE_MODEL_SPAWNER_H__

#include <vector>

namespace SJH { class Model; }
namespace SJH::Scene
{
    class Actor;

    /// @brief Assimp Model 의 N RenderUnit → N 자식 Actor (각자 MeshRenderer 보유).
    /// @details Q5-1 결정 — 1 RenderUnit = 1 Actor. 부모-자식 Transform 계층 자연 흡수.
    namespace ModelSpawner
    {
        /// @brief @c parent 의 자식들로 Model 의 모든 RenderUnit 을 Actor 화.
        /// @return 생성된 자식 Actor 목록 (호출자가 추가 셋업 시).
        std::vector<Actor*> SpawnEntities(Actor& parent, const Model& model);
    }
}

#endif // __SJH_SCENE_MODEL_SPAWNER_H__
```

- [ ] **Step 2: 구현 — `apps/_MyApp_/src/Bootstrap/model_spawner.cpp`**

```cpp
#include "apps/_MyApp_/src/Bootstrap/model_spawner.h"
#include "scene/actor.h"
#include "<scene>/components.h"
#include "object/model.h"

namespace SJH::Scene::ModelSpawner
{
    std::vector<Actor*> SpawnEntities(Actor& parent, const Model& model)
    {
        std::vector<Actor*> spawned;
        const auto& units = model.GetRenderUnits();
        spawned.reserve(units.size());

        for (size_t i = 0; i < units.size(); ++i)
        {
            const auto& ru = units[i];
            auto child = std::make_unique<Actor>("RenderUnit_" + std::to_string(i));
            child->AddComponent<MeshRenderer>(ru.mesh.get(), ru.material);
            spawned.push_back(parent.AddChild(std::move(child)));
        }
        return spawned;
    }
}
```

- [ ] **Step 3: CMake 갱신 — `src/scene/CMakeLists.txt`**

소스 목록에 `model_spawner.cpp` 추가. `SJH::material` PRIVATE 의존 추가 (MeshRenderer 가 Material 참조):

```cmake
add_library(sjhopengl_scene STATIC
    actor.cpp
    scene.cpp
    model_spawner.cpp
)

target_link_libraries(sjhopengl_scene
    PUBLIC  SJH::common SJH::object project_deps
    PRIVATE SJH::material
)
```

- [ ] **Step 4: 빌드 통과 확인**

```bash
cmake --build --preset ninja --target sjhopengl_scene 2>&1 | tail -10
```
Expected: PASS.

- [ ] **Step 5: 커밋**

```bash
git add apps/_MyApp_/src/Bootstrap/model_spawner.h apps/_MyApp_/src/Bootstrap/model_spawner.cpp src/scene/CMakeLists.txt
git commit -m "[feat] : ModelSpawner — Model 의 N RU → N 자식 Actor (SP3 T7)

Q5-1 결정. 1 RenderUnit = 1 Actor. 부모-자식 Transform 계층이 Cocos Node
트리로 자연 흡수. 호출자: SpawnEntities(parent, model) → vector<Actor*>.
"
```

---

## Task 8: `src/engine/` 전체 삭제

**Files:**
- Delete: `src/engine/{scene_graph,transform,camera,model_base}.{h,cpp}`, `src/engine/CMakeLists.txt`

- [ ] **Step 1: 사전 확인 — 활성 사용처 0 검증**

```bash
grep -n "add_subdirectory(engine)" src/CMakeLists.txt && echo "❌ 엔진이 빌드 그래프에 있음 — 수동 점검 필요" || echo "✅ engine 미등록 — 안전"
git grep -nl 'engine/' -- 'apps/' 2>/dev/null | grep -v "_MyApp_\|_deptest_" || echo "✅ 활성 챕터에서 engine 참조 없음"
```
Expected: `✅ engine 미등록 — 안전` + `✅ 활성 챕터에서 engine 참조 없음`.

(`_MyApp_`, `_deptest_` 비활성 챕터의 engine 참조는 무시 — 빌드 그래프 밖.)

- [ ] **Step 2: `src/engine/` 전체 삭제**

```bash
git rm -r src/engine/
```
Expected: 5 ~ 7 파일 + 디렉토리 제거 메시지.

- [ ] **Step 3: 빌드 통과 검증**

```bash
cmake --build --preset ninja 2>&1 | tail -10
```
Expected: 활성 타깃 모두 PASS.

- [ ] **Step 4: 잔재 grep**

```bash
git ls-files src/engine/
echo "---"
echo "위 출력 비어야 함"
```

- [ ] **Step 5: 커밋**

```bash
git commit -m "[remove] : src/engine/ 전체 삭제 — 레거시 dead code 청소 (SP3 T8)

SP1 에서 src/engine/shader_program.* 제거 후 잔재였던:
- scene_graph.{h,cpp} — Scene 싱글톤 + Actor 트리가 대체
- transform.h — SJH::Transform (src/object/) 가 대체
- camera.h — SP3.5 의 CameraComponent 가 흡수 예정
- model_base.{h,cpp} — SJH::Model (src/object/) 가 대체
- CMakeLists.txt

모두 빌드 그래프 밖 dead code. 활성 사용처 0. 비활성 챕터(_MyApp_,
_deptest_) 의 잔존 참조는 챕터 활성화 시 별도 마이그레이션.
"
```

---

## Task 9: Actor 라이프사이클 단위 테스트

**Files:**
- Create: `<test>/test_actor_lifecycle.cpp`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: 단위 테스트 — `<test>/test_actor_lifecycle.cpp`**

```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "scene/actor.h"

namespace {
    struct CountingComponent : public SJH::Scene::Component
    {
        int onEnterCount  = 0;
        int onExitCount   = 0;
        int updateCount   = 0;

        void OnEnter() override { ++onEnterCount; }
        void OnExit()  override { ++onExitCount;  }
        void Update(float) override { ++updateCount; }
    };
}

TEST_CASE("Actor — AddComponent before OnEnter, lazy entry", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    auto* comp = root.AddComponent<CountingComponent>();

    REQUIRE(comp->onEnterCount == 0);   // 아직 OnEnter 안 호출

    root.OnEnter();
    REQUIRE(comp->onEnterCount == 1);   // cascade 로 호출
}

TEST_CASE("Actor — AddComponent after OnEnter, immediate entry (D-9)", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    root.OnEnter();
    REQUIRE(root.IsEntered());

    auto* comp = root.AddComponent<CountingComponent>();
    REQUIRE(comp->onEnterCount == 1);   // 즉시 호출 — D-9 contract
}

TEST_CASE("Actor — OnExit cascade, children reverse order", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    auto child = std::make_unique<SJH::Scene::Actor>("Child");
    auto* childComp = child->AddComponent<CountingComponent>();
    auto* childPtr = root.AddChild(std::move(child));

    root.OnEnter();
    root.OnExit();

    REQUIRE(childComp->onExitCount == 1);   // 자식 컴포넌트도 cleanup
    REQUIRE_FALSE(childPtr->IsEntered());
}

TEST_CASE("Actor — SetEnabled false skips Update, not OnEnter/OnExit", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    auto* comp = root.AddComponent<CountingComponent>();
    root.OnEnter();
    REQUIRE(comp->onEnterCount == 1);

    comp->SetEnabled(false);
    root.Update(0.016f);
    REQUIRE(comp->updateCount == 0);   // 호출 안 됨

    comp->SetEnabled(true);
    root.Update(0.016f);
    REQUIRE(comp->updateCount == 1);
}

TEST_CASE("Actor — SetActive false blocks own + children Update", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    auto* rootComp = root.AddComponent<CountingComponent>();
    auto child = std::make_unique<SJH::Scene::Actor>("Child");
    auto* childComp = child->AddComponent<CountingComponent>();
    root.AddChild(std::move(child));
    root.OnEnter();

    root.SetActive(false);
    root.Update(0.016f);
    REQUIRE(rootComp->updateCount == 0);
    REQUIRE(childComp->updateCount == 0);   // 자식도 차단
}

TEST_CASE("Actor — AddChild(nullptr) safe early return", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    auto* result = root.AddChild(nullptr);
    REQUIRE(result == nullptr);
    REQUIRE(root.GetChildren().empty());
}

TEST_CASE("Actor — GetWorldMatrix parent chain composition", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    root.GetTransform().Translate = vmath::vec3(10.0f, 0.0f, 0.0f);

    auto child = std::make_unique<SJH::Scene::Actor>("Child");
    child->GetTransform().Translate = vmath::vec3(5.0f, 0.0f, 0.0f);
    auto* childPtr = root.AddChild(std::move(child));

    const vmath::mat4 world = childPtr->GetWorldMatrix();
    // (10 + 5) = 15 — translation column 의 X 성분
    REQUIRE(world[3][0] == 15.0f);
}
```

- [ ] **Step 2: CMake 등록 — `test/CMakeLists.txt`**

기존 CMakeLists.txt 의 패턴을 따라 `test_actor_lifecycle` 타깃 추가:

```cmake
add_executable(test_actor_lifecycle test_actor_lifecycle.cpp)
target_link_libraries(test_actor_lifecycle PRIVATE
    Catch2::Catch2WithMain
    SJH::scene
)
catch_discover_tests(test_actor_lifecycle)
```

(기존 다른 `test_*` 타깃 옆에 추가. 정확한 위치는 alphabetical 또는 추가 순.)

- [ ] **Step 3: 빌드 + 테스트 실행**

```bash
cmake --preset ninja -DENABLE_TESTING=ON 2>&1 | tail -3
cmake --build --preset ninja --target test_actor_lifecycle 2>&1 | tail -5
ctest --test-dir build_ninja -R "test_actor_lifecycle" -V 2>&1 | tail -20
```
Expected: 7 TEST_CASE 모두 PASS.

- [ ] **Step 4: 커밋**

```bash
git add <test>/test_actor_lifecycle.cpp test/CMakeLists.txt
git commit -m "[test] : Actor 라이프사이클 단위 테스트 (SP3 T9)

7 TEST_CASE — Catch2 v3:
- AddComponent before OnEnter — lazy entry
- AddComponent after OnEnter — immediate entry (D-9 contract)
- OnExit cascade — 자식 컴포넌트 cleanup
- SetEnabled — Update 만 토글, OnEnter/OnExit 무관
- SetActive — 자기 + 자식 Update 차단
- AddChild(nullptr) — safe early return (ddd Early Return)
- GetWorldMatrix — 부모 체인 합성 (10 + 5 = 15)
"
```

---

## Task 10: `apps/ecs_demo/` 챕터 + 셰이더 + 통합 검증

**Files:**
- Create: `<apps>/ecs_demo/main.cpp`
- Create: `apps/ecs_demo/CMakeLists.txt`
- Create: `apps/ecs_demo/resources/shaders/simple.vs`
- Create: `apps/ecs_demo/resources/shaders/simple.fs`
- Modify: `apps/CMakeLists.txt` (`add_subdirectory(ecs_demo)` 추가)

- [ ] **Step 1: 정점 셰이더 — `apps/ecs_demo/resources/shaders/simple.vs`**

```glsl
#version 410 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main() {
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
```

- [ ] **Step 2: 프래그먼트 셰이더 — `apps/ecs_demo/resources/shaders/simple.fs`**

```glsl
#version 410 core

out vec4 FragColor;

void main() {
    FragColor = vec4(0.8, 0.4, 0.2, 1.0);   // 오렌지색 단일색
}
```

- [ ] **Step 3: 챕터 main — `<apps>/ecs_demo/main.cpp`**

```cpp
#include <sb7.h>
#include "<render>/render_context.h"
#include "<render>/render_system.h"
#include "scene/scene.h"
#include "<scene>/components.h"
#include "program/program.h"
#include "object/mesh.h"
#include "material/material.h"

class ecs_demo_app : public sb7::application
{
public:
    void startup() override
    {
        // 1) Program / Mesh / Material 리소스 생성
        mProgram = SJH::Program::CreateWithVSFS(
            "resources/shaders/simple.vs",
            "resources/shaders/simple.fs");
        if (!mProgram) { std::cerr << "셰이더 로드 실패\n"; std::exit(1); }

        mBoxMesh = SJH::Mesh::CreateBox();
        mBoxMat  = SJH::Material::Create();
        mBoxMat->SetProgram(mProgram.get());

        // 2) Scene 에 Box Actor 추가
        auto& director = SJH::Scene::Director::Get();
        auto box = std::make_unique<SJH::Scene::Actor>("Box");
        box->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, -3.0f);
        box->AddComponent<SJH::Scene::MeshRenderer>(mBoxMesh.get(), mBoxMat.get());
        director.Root().AddChild(std::move(box));

        // 3) Scene 진입
        director.Enter();
        mLastTime = 0.0;
    }

    void render(double currentTime) override
    {
        const float dt = static_cast<float>(currentTime - mLastTime);
        mLastTime = currentTime;

        // Update 호출 (현재는 컴포넌트가 Update 안 쓰지만 패턴 검증)
        SJH::Scene::Director::Get().Update(dt);

        auto& rc = SJH::RenderContext::Get();
        rc.SetDefaultTargetSize(info.windowWidth, info.windowHeight);
        rc.BeginFrame(rc.GetDefaultTarget());

        const float aspect = static_cast<float>(info.windowWidth) / info.windowHeight;
        const auto view = vmath::lookat(vmath::vec3(0,0,5), vmath::vec3(0), vmath::vec3(0,1,0));
        const auto proj = vmath::perspective(45.0f, aspect, 0.1f, 100.0f);

        mRenderSys.Render(view, proj);
    }

    void shutdown() override
    {
        SJH::Scene::Director::Get().Exit();
    }

private:
    SJH::ProgramUPtr  mProgram;
    SJH::MeshUPtr     mBoxMesh;
    SJH::MaterialUPtr mBoxMat;
    SJH::RenderSystem mRenderSys;
    double            mLastTime = 0.0;
};

DECLARE_MAIN(ecs_demo_app);
```

- [ ] **Step 4: 챕터 CMakeLists.txt — `apps/ecs_demo/CMakeLists.txt`**

```cmake
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(${CHAPTER_NAME} main.cpp)

target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    SJH::program
    SJH::object
    SJH::material
    SJH::render
    SJH::scene
)

# 리소스 (셰이더) 복사
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)
```

- [ ] **Step 5: 활성화 — `apps/CMakeLists.txt`**

기존 `add_subdirectory(imguitest)` 줄 *직후* (또는 alphabetical 위치) 에 한 줄 추가:

```cmake
add_subdirectory(ecs_demo)
```

- [ ] **Step 6: 빌드 통과 확인**

```bash
cmake --build --preset ninja --target ecs_demo 2>&1 | tail -10
```
Expected: PASS — `build_ninja/apps/ecs_demo/ecs_demo` 실행 파일 생성, resources/shaders/ 복사 완료.

- [ ] **Step 7: 시각 회귀 smoke test**

```bash
cd build_ninja/apps/ecs_demo && timeout 5 ./ecs_demo 2>&1 | head -20 ; cd -
```
Expected: 정상 startup, *오렌지 박스* 화면 가운데 표시. 5 초 후 timeout 으로 종료. crash/assert 없음.

> GUI 가 자동 종료 안 되면 startup 로그만 검증. *시각 검증* 은 인간 책임.

- [ ] **Step 8: 통합 검증 — spec §6 매핑**

| Spec §6 | 검증 |
|---|---|
| 1. 빌드 통과 | Step 6 |
| 2. static_assert 비복사·비이동 | Task 1 / 2 의 static_assert 종합 |
| 3. ecs_demo 시각 동작 | Step 7 |
| 4. 단위 테스트 PASS | Task 9 |
| 5. Pattern Y 정합 (`->Draw/->Apply` 0 건) | Task 3 Step 8 |
| 6. `src/engine/` 부재 | Task 8 |

각 step 의 결과를 종합해 *SP3 spec §6 의 6 단계 모두 충족* 을 보고.

- [ ] **Step 9: 커밋**

```bash
git add apps/ecs_demo apps/CMakeLists.txt
git commit -m "[feat] : apps/ecs_demo 챕터 + 시각 검증 (SP3 T10)

F1 결정. SP3 의 Actor+Component + RenderSystem + RenderQueue 시각 동작 검증.
- 오렌지 박스 1 개를 화면 가운데에 렌더링
- Scene.Enter() / Update() / Render(view, proj) / Exit() lifecycle
- SP3 spec §6 검증 6 단계 모두 충족 확인

SP3 완수.
"
```

---

## Out of Scope (재확인)

| 항목 | 위치 |
|------|------|
| CameraComponent + 활성 카메라 자동 추출 | **SP3.5** |
| LightComponent + 광원 list | **SP3.5** |
| GetWorldMatrix dirty cache | **SP3.5** |
| OnEnable/OnDisable transition hook | **영구 미지원** |
| FrameBufferTarget + post-processing | **SP4** |
| 비활성 챕터 (_MyApp_, _deptest_) 의 entt 사용 코드 마이그레이션 | 챕터 활성화 시 별도 |
