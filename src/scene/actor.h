/**
 * @file actor.h
 * @brief Scene 그래프의 기본 단위 - @c Actor (노드) + @c Component (동작 단위) 정의.
 *
 * @details
 *  ### 책임
 *  - @c Component - 순수 추상 생명주기 베이스 (OnEnter / OnExit / Update).
 *    Cocos @c cc::Component + Unreal @c UActorComponent 영감.
 *  - @c Actor - Scene 트리 노드. 내장 @c Transform + @c Component map
 *    + 부모-자식 트리 구조. Cocos @c cc::Node 정통.
 *
 *  ### Actor 비상속 컨벤션
 *  `class FooActor : public Actor` 형태는 *금지*. 특수 속성은 Component 부착으로만 부여.
 *  자주 쓰이는 조합은 @c compound_actor.h 의 free factory(@c CreateCameraActor 등) 사용.
 *
 *  ### 비-책임
 *  - [X] 렌더링 - @c MeshRenderer Component 가 담당.
 *  - [X] 물리 - @c Components::Physics 계열이 담당.
 *  - [X] Camera / Light 전역 등록 - 해당 Component 의 OnEnter hook 이 @c SceneContext 에 자동 등록.
 *
 * @note @c addChild 순서가 자동 렌더 순서 (Cocos2D 정통). @c Camera::Depth 는 폐기됨.
 */

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

    /**
     * @brief Cocos @c cc::Component + Unreal @c UActorComponent 영감 - Actor 부착 동작 단위 추상 베이스.
     * @details
     *  ### Lifecycle (Cocos 정통)
     *  - @c OnEnter  - Owner Actor 가 running 상태 진입 (Cocos @c onEnter / Unreal @c BeginPlay).
     *                  Camera/Light 등은 이 시점에 @c SceneContext 에 자동 등록.
     *  - @c OnExit   - running 에서 벗어남 (Cocos @c onExit / Unreal @c EndPlay).
     *  - @c Update(dt) - 매 프레임, enabled 일 때만 Actor 가 호출 (Cocos @c update / Unreal @c TickComponent).
     *
     *  ### Enabled (Unreal 영감)
     *  @c SetEnabled(false) 는 @c Update 만 정지. OnEnter / OnExit lifecycle 에는 영향 없음.
     *
     *  ### 소유권
     *  Component 의 소유권은 @c Actor 내 @c mComponents(@c unique_ptr). 외부 raw 포인터는 관찰용.
     */
    class Component
    {
    public:
        virtual ~Component() = default;

        /// @brief Owner Actor 가 running 상태에 진입할 때 호출 (BeginPlay 정통).
        virtual void OnEnter() = 0;
        /// @brief Owner Actor 가 running 상태에서 벗어날 때 호출 (EndPlay 정통).
        virtual void OnExit() = 0;
        /// @brief 매 프레임 호출 - @c IsEnabled() 가 true 일 때만 Actor 가 디스패치.
        /// @param dt 프레임 델타 타임 (초).
        virtual void Update(float dt) = 0;

        /// @brief Update 활성 여부.
        bool IsEnabled() const { return mEnabled; }
        /// @brief Update 활성/비활성 전환. OnEnter/OnExit lifecycle 에는 영향 없음.
        void SetEnabled(bool e) { mEnabled = e; }
        /// @brief 이 Component 를 보유한 Actor (비소유 raw 포인터).
        Actor* GetOwner() const { return mOwner; }

    protected:
        // 추상 베이스 - 파생 클래스 통해서만 생성 가능
        Component() = default;

    private:
        friend class Actor;
        Actor* mOwner   = nullptr;
        bool   mEnabled = true;
    };

    /**
     * @brief Cocos @c cc::Node 정통 - Scene 트리 노드. 부모-자식 계층 + Component 맵 + 내장 Transform.
     * @details
     *  ### 단일 호출 contract (D-9)
     *  @c AddComponent\<T\> / @c AddChild 는 entered 부모에 대해 *construct + insert + OnEnter* 를
     *  한 호출에서 실행. 호출자가 이 contract 를 인지해야 함.
     *
     *  ### Actor 비상속 컨벤션
     *  Actor 는 상속 금지 - 특수 속성은 Component 부착으로만 부여.
     *  자주 쓰이는 Actor + Component 조합은 @c compound_actor.h 의 free factory 사용.
     *
     *  ### 렌더 순서
     *  @c addChild(자식) 호출 순서가 렌더 순서. @c Camera::Depth 는 폐기됨 (2026-05-26).
     *
     *  ### 복사/이동 금지
     *  @c unique_ptr 로 트리를 소유하므로 복사/이동 불가 (컴파일 타임 검증, @c actor.cpp).
     */
    class Actor
    {
    public:
        /// @brief 이름으로 Actor 를 생성. 이름은 디버그/조회 용도.
        explicit Actor(std::string name = "");
        ~Actor();

        Actor(const Actor&)            = delete;
        Actor& operator=(const Actor&) = delete;
        Actor(Actor&&)                 = delete;
        Actor& operator=(Actor&&)      = delete;

        // === Tree ===
        /// @brief 자식 Actor 를 추가하고 raw 포인터 반환 (소유권 이전).
        /// @details mEntered 상태이면 자식의 OnEnter 를 즉시 호출 (단일 호출 contract D-9).
        ///          @p child 가 nullptr 이면 nullptr 반환.
        /// @return 부착된 자식의 비소유 raw 포인터. 수명은 이 Actor 가 보장.
        Actor* AddChild(std::unique_ptr<Actor> child);

        /// @brief 파괴 없이 자식을 분리, 소유권 반환 (Godot @c remove_child 정통).
        /// @details mEntered 상태면 OnExit 호출 후 mParent 를 nullptr 로 초기화.
        ///          반환된 @c unique_ptr 을 다른 Actor::AddChild 에 넘겨 재부착 가능.
        /// @return 분리된 Actor 소유권. @p child 가 자식이 아니면 nullptr.
        std::unique_ptr<Actor> DetachChild(Actor* child);

        /// @brief 자식 Actor 를 제거하고 즉시 소멸 (소유권 포기).
        /// @details mEntered 상태이면 OnExit 먼저 호출. 자식이 없으면 no-op.
        void   RemoveChild(Actor* child);

        /// @brief 부모 Actor (비소유 raw 포인터). 루트이면 nullptr.
        Actor* GetParent() const { return mParent; }
        /// @brief 직계 자식 벡터 (읽기 전용).
        const std::vector<std::unique_ptr<Actor>>& GetChildren() const { return mChildren; }

        /// @brief 이름으로 직계(또는 재귀) 자식을 조회 (Cocos @c getChildByName 정통).
        /// @param name 검색할 이름.
        /// @param recursive true 이면 전체 하위 트리 DFS 탐색.
        /// @return 첫 번째 일치 자식. 없으면 nullptr.
        Actor* FindChild(const std::string& name, bool recursive = false) const;

        /// @brief 술어 @p fn 을 만족하는 첫 번째 자식 반환.
        /// @param fn @c bool(Actor*) 서명의 callable.
        /// @param recursive true 이면 전체 하위 트리 DFS 탐색.
        template<typename Fn>
        Actor* FindChildIf(Fn&& fn, bool recursive = false) const;

        // === Components ===
        /// @brief 새 Component @c T 를 생성/부착하고 raw 포인터 반환.
        /// @details @c T::ctor 에 @p args 를 완전 전달. mEntered 상태이면 OnEnter 즉시 호출.
        ///          같은 타입을 두 번 부착하면 assert (duplicate component guard).
        /// @tparam T @c Component 파생 타입.
        /// @return 부착된 컴포넌트의 비소유 raw 포인터.
        template<typename T, typename... Args>
        T* AddComponent(Args&&... args);

        /// @brief @c T 타입(인터페이스 포함) 컴포넌트 조회. 자식 Actor 는 순회하지 않음.
        /// @details
        ///   게이트 = @c is_polymorphic_v\<T\> - Component 파생뿐 아니라 순수 인터페이스
        ///   (@c IDamageable / @c IActorPresentation 등)도 조회 가능 (Unity @c GetComponent\<IInterface\> 정통).
        ///   - 1단계 - @c typeid(T) 정확 매칭 O(1). 구체 클래스 호출의 빠른 경로.
        ///   - 2단계 - 1단계 miss 시 @c mComponents 순회 + @c dynamic_cast\<T*\> O(N).
        ///   인터페이스/base 타입 호출은 항상 2단계. 동일 인터페이스를 만족하는 컴포넌트가 여럿이면
        ///   @c unordered_map 순회 순서 기준 *첫 번째* 반환 (비결정적).
        ///   여러 매칭이 필요하면 @c ForEachComponent + dynamic_cast 사용.
        /// @return 일치 컴포넌트 raw 포인터. 없으면 nullptr.
        template<typename T,
                 typename = std::enable_if_t<std::is_polymorphic_v<T>>>
        T*   GetComponent() const;

        /// @brief @c T 타입 컴포넌트를 제거/소멸.
        /// @details mEntered 상태이면 OnExit 먼저 호출. 없으면 no-op.
        template<typename T> void RemoveComponent();

        /// @brief 모든 Component 를 제거/소멸 (OnExit 후 map clear).
        void RemoveAllComponents();

        /// @brief 모든 Component 에 콜백 @p fn 적용 - contact listener 의 dynamic_cast dispatch 용.
        /// @param fn @c void(Component*) 서명의 callable.
        template<typename Fn>
        void ForEachComponent(Fn&& fn) const
        {
            for (auto& [ti, comp] : mComponents)
                fn(comp.get());
        }

        // === Identity ===
        /// @brief Actor 이름 조회 (디버그/FindChild 검색 키).
        const std::string& GetName() const { return mName; }
        /// @brief Actor 이름 설정.
        void SetName(std::string n) { mName = std::move(n); }

        // === Transform (Unity 내장) ===
        /// @brief 내장 Transform (위치/회전/스케일) 참조.
        Transform&       GetTransform()       { return mTransform; }
        /// @brief 내장 Transform (읽기 전용).
        const Transform& GetTransform() const { return mTransform; }
        /// @brief 루트까지의 부모 chain 을 곱한 월드 변환 행렬 (column-major).
        /// @details 루트이면 @c GetTransform().GetLocalMatrix() 와 동등.
        vmath::mat4      GetWorldMatrix() const;

        // === Active ===
        /// @brief 활성 여부 - false 이면 @c Update 가 이 Actor 와 모든 자식을 건너뜀.
        bool IsActive()  const { return mActive; }
        /// @brief 활성/비활성 전환.
        void SetActive(bool a) { mActive = a; }
        /// @brief running 상태 여부 - OnEnter 가 호출된 이후 OnExit 전까지 true.
        bool IsEntered() const { return mEntered; }

        // === Layer (SP4 D-15) ===
        /// @brief Actor 의 가시성 layer (Unity 정통 비트마스크).
        /// @details 기본값 = @c Layer::Default (비트 0).
        ///          @c Camera::CullingMask 와 AND 검사로 SceneRenderer 가 필터.
        void     SetLayer(uint64_t layer) { mLayer = layer; }
        /// @brief type-safe @c Layer overload.
        void     SetLayer(SJH::Scene::Layer l) { mLayer = SJH::Scene::ToBits(l); }
        /// @brief 현재 layer 비트마스크.
        uint64_t GetLayer() const         { return mLayer; }

        // === Lifecycle ===
        /// @brief running 상태 진입 - 모든 Component 와 자식 Actor 의 OnEnter 를 재귀 호출.
        /// @details 중복 호출(이미 entered) 이면 no-op.
        void OnEnter();
        /// @brief running 상태 이탈 - 자식(역순) 및 Component 의 OnExit 를 재귀 호출.
        /// @details 이미 exited 이면 no-op.
        void OnExit();
        /// @brief 매 프레임 갱신 - 자기 Component(enabled 만) -> 자식 Actor 순서로 재귀 호출.
        /// @note 호출 순서: 자기 components (enabled 만) -> 자식 Actor (재귀).
        ///       부모 Transform 갱신 후 자식이 Read 하는 케이스에서 *동일 프레임 일관성* 보장.
        /// @param dt 프레임 델타 타임 (초).
        void Update(float dt);

    private:
        std::string mName;
        Actor*      mParent = nullptr;
        std::vector<std::unique_ptr<Actor>> mChildren;
        std::unordered_map<std::type_index, std::unique_ptr<Component>> mComponents;
        Transform   mTransform;
        bool        mActive  = true;
        bool        mEntered = false;
        uint64_t    mLayer   = SJH::Scene::ToBits(SJH::Scene::Layer::Default); // SP5 - Layer::Default = 비트 0
    };

    // === Template 정의 (ddd 2 차 patch 적용) ===

    template<typename Fn>
    Actor* Actor::FindChildIf(Fn&& fn, bool recursive) const
    {
        for (auto& child : mChildren)
        {
            if (fn(child.get())) return child.get();
            if (recursive)
                if (auto* found = child->FindChildIf(fn, true)) return found;
        }
        return nullptr;
    }

    template<typename T, typename... Args>
    T* Actor::AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        // 중복 추가 가드 (ddd Explicit Side Effects - silent overwrite 금지)
        // 찾기 먼저: 실패하면 맵 미변동 (exception-safe)
        assert(mComponents.find(typeid(T)) == mComponents.end()
               && "Duplicate component type - Actor::AddComponent<T> called twice");

        // construct -> mOwner 설정 -> insert -> dispatch (exception-safe)
        // T 의 ctor 가 throw 해도 map 무변동 - 다음 AddComponent<T> 가 깨끗하게 진행
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
        // Fast path - 구체 컴포넌트(Component 파생) 정확 매칭 (O(1), RTTI 없음 - static_cast).
        // if constexpr 가드: T 가 순수 인터페이스(비-Component)면 이 블록은 폐기되어
        // static_cast<T*>(Component*) (무관한 타입 간 변환 = 컴파일 에러)가 인스턴스화되지 않는다.
        // 인터페이스/base 조회는 항상 아래 slow-path dynamic_cast 로 떨어진다.
        if constexpr (std::is_base_of_v<Component, T>)
        {
            auto it = mComponents.find(typeid(T));
            if (it != mComponents.end())
                return static_cast<T*>(it->second.get());
        }

        // Slow path - 인터페이스/base 매칭 (O(N), 현 Actor 의 컴포넌트만, 자식 미순회)
        for (auto& [ti, comp] : mComponents)
            if (auto* p = dynamic_cast<T*>(comp.get()))
                return p;

        return nullptr;
    }

    template<typename T>
    void Actor::RemoveComponent()
    {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
        auto it = mComponents.find(typeid(T));
        if (it == mComponents.end()) return;
        if (mEntered) it->second->OnExit();   // symmetric - OnEnter 호출된 경우만 OnExit
        mComponents.erase(it);
    }
}

#endif // __SJH_SCENE_ACTOR_H__
