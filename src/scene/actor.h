#ifndef __SJH_SCENE_ACTOR_H__
#define __SJH_SCENE_ACTOR_H__

#include "object/transform.h"
#include "scene/layer.h"
#include <cassert>
#include <cstdint>
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
        virtual void OnEnter() = 0;
        virtual void OnExit() = 0;
        virtual void Update(float dt) = 0;

        bool IsEnabled() const { return mEnabled; }
        void SetEnabled(bool e) { mEnabled = e; }
        Actor* GetOwner() const { return mOwner; }

    protected:
        // 추상 베이스 — 파생 클래스 통해서만 생성 가능
        Component() = default;

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
        template<typename T,
                 typename = std::enable_if_t<std::is_base_of_v<Component, T>>>
        T*   GetComponent() const;
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

        // === Layer (SP4 D-15) ===
        /// @brief Actor 의 가시성 layer (Unity 정통 비트마스크).
        /// @details 기본값 = 1 (비트 0). Camera::cullingMask 와 AND 검사로 SceneRenderer 이 필터.
        void     SetLayer(uint64_t layer) { mLayer = layer; }
        void     SetLayer(SJH::Scene::Layer l) { mLayer = SJH::Scene::ToBits(l); }
        uint64_t GetLayer() const         { return mLayer; }

        // === Lifecycle ===
        void OnEnter();
        void OnExit();
        /// @note 호출 순서: 자기 components (enabled 만) -> 자식 Actor (재귀).
        ///       부모 Transform 갱신 후 자식이 Read 하는 케이스에서 *동일 프레임 일관성* 보장.
        void Update(float dt);

    private:
        std::string mName;
        Actor*      mParent = nullptr;
        std::vector<std::unique_ptr<Actor>> mChildren;
        std::unordered_map<std::type_index, std::unique_ptr<Component>> mComponents;
        Transform   mTransform;
        bool        mActive  = true;
        bool        mEntered = false;
        uint64_t    mLayer   = SJH::Scene::ToBits(SJH::Scene::Layer::Default); // SP5 — Layer::Default = 비트 0
    };

    // === Template 정의 (ddd 2 차 patch 적용) ===

    template<typename T, typename... Args>
    T* Actor::AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        // 중복 추가 가드 (ddd Explicit Side Effects — silent overwrite 금지)
        // 찾기 먼저: 실패하면 맵 미변동 (exception-safe)
        assert(mComponents.find(typeid(T)) == mComponents.end()
               && "Duplicate component type — Actor::AddComponent<T> called twice");

        // construct -> mOwner 설정 -> insert -> dispatch (exception-safe)
        // T 의 ctor 가 throw 해도 map 무변동 — 다음 AddComponent<T> 가 깨끗하게 진행
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        comp->mOwner = this;
        T* raw = comp.get();
        mComponents.emplace(typeid(T), std::move(comp));
        if (mEntered) raw->OnEnter();
        return raw;
    }

    template<typename T, typename>
    T* Actor::GetComponent() const
    {
        auto it = mComponents.find(typeid(T));
        return it != mComponents.end() ? static_cast<T*>(it->second.get()) : nullptr;
    }

    template<typename T>
    void Actor::RemoveComponent()
    {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
        auto it = mComponents.find(typeid(T));
        if (it == mComponents.end()) return;
        if (mEntered) it->second->OnExit();   // symmetric — OnEnter 호출된 경우만 OnExit
        mComponents.erase(it);
    }
}

#endif // __SJH_SCENE_ACTOR_H__
