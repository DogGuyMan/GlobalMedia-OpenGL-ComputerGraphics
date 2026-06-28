// SJH::Scene::Actor / Component characterization - 트리 편집/조회/lifecycle/월드행렬 계약 잠금.
// 핵심: AddChild=소유권이전, DetachChild=소유권반환, RemoveChild=파괴, OnEnter 는 public 진입점.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "scene/actor.h"

#include <memory>
#include <string>

using Catch::Matchers::WithinAbs;
using SJH::Scene::Actor;
using SJH::Scene::Component;

namespace
{
    // 1) 스파이 Component - OnEnter/OnExit/OnUpdate 호출 횟수 카운트.
    //    GetComponent<T> 정확 매칭(fast-path) 검증을 위해 두 개의 distinct 타입을 둔다.
    struct SpyComponentA : public Component
    {
        int enterCount = 0;
        int exitCount = 0;
        int updateCount = 0;

        void OnEnter() override { enterCount++; }
        void OnExit() override { exitCount++; }
        void Update(float) override { updateCount++; }
    };

    struct SpyComponentB : public Component
    {
        int updateCount = 0;
        void OnEnter() override {}
        void OnExit() override {}
        void Update(float) override { updateCount++; }
    };

    // 2) 공통 인터페이스 - GetComponent<인터페이스> slow-path(dynamic_cast) 검증용.
    //    Component 비파생 순수 인터페이스(is_polymorphic).
    struct IPingable
    {
        virtual ~IPingable() = default;
        virtual int Ping() const = 0;
    };

    // SpyComponentA 와 동일 타입 충돌을 피하려 별도 Component + IPingable 다중 상속.
    struct PingComponent : public Component, public IPingable
    {
        void OnEnter() override {}
        void OnExit() override {}
        void Update(float) override {}
        int Ping() const override { return 42; }
    };
}

TEST_CASE("Scene: AddChild 는 소유권 이전 + 부모/자식 링크 설정", "[scene]")
{
    Actor parent("parent");
    auto childOwned = std::make_unique<Actor>("child");
    Actor *childRaw = childOwned.get();

    Actor *returned = parent.AddChild(std::move(childOwned));
    REQUIRE(returned == childRaw);
    REQUIRE(parent.GetChildren().size() == 1);
    REQUIRE(childRaw->GetParent() == &parent);
}

TEST_CASE("Scene: AddChild(nullptr) 는 nullptr 반환 + 무변동", "[scene]")
{
    Actor parent("parent");
    Actor *returned = parent.AddChild(nullptr);
    REQUIRE(returned == nullptr);
    REQUIRE(parent.GetChildren().empty());
}

TEST_CASE("Scene: DetachChild 는 소유권 반환 + 부모 링크 해제 + OnExit", "[scene]")
{
    Actor parent("parent");
    Actor *child = parent.AddChild(std::make_unique<Actor>("child"));
    auto *spy = child->AddComponent<SpyComponentA>();
    parent.OnEnter(); // 트리 전체 entered 진입 (public 경로)
    REQUIRE(spy->enterCount == 1);

    std::unique_ptr<Actor> detached = parent.DetachChild(child);
    REQUIRE(detached != nullptr);            // 소유권 반환
    REQUIRE(detached.get() == child);
    REQUIRE(detached->GetParent() == nullptr); // 부모 링크 해제
    REQUIRE(parent.GetChildren().empty());
    REQUIRE(spy->exitCount == 1);            // entered 였으므로 OnExit 호출
    // detached 가 살아있으므로 spy 도 유효
    REQUIRE(spy->enterCount == 1);
}

TEST_CASE("Scene: DetachChild - 비자식이면 nullptr", "[scene]")
{
    Actor parent("parent");
    Actor stranger("stranger");
    REQUIRE(parent.DetachChild(&stranger) == nullptr);
}

TEST_CASE("Scene: RemoveChild 는 OnExit 후 파괴", "[scene]")
{
    Actor parent("parent");
    Actor *child = parent.AddChild(std::make_unique<Actor>("child"));
    auto *spy = child->AddComponent<SpyComponentA>();
    parent.OnEnter();
    REQUIRE(spy->enterCount == 1);

    parent.RemoveChild(child); // entered 면 OnExit 후 erase(파괴)
    REQUIRE(parent.GetChildren().empty());
    // child 는 파괴됨 - spy 포인터는 dangling 이므로 역참조 금지. children 비었음으로만 확인.
}

TEST_CASE("Scene: RemoveChild - 비자식이면 no-op", "[scene]")
{
    Actor parent("parent");
    Actor *child = parent.AddChild(std::make_unique<Actor>("child"));
    Actor stranger("stranger");
    parent.RemoveChild(&stranger); // 없으면 no-op
    REQUIRE(parent.GetChildren().size() == 1);
    (void)child;
}

TEST_CASE("Scene: FindChild - 직계/재귀/미존재", "[scene]")
{
    Actor root("root");
    Actor *a = root.AddChild(std::make_unique<Actor>("a"));
    Actor *grand = a->AddChild(std::make_unique<Actor>("grand"));

    REQUIRE(root.FindChild("a") == a);          // 직계 적중
    REQUIRE(root.FindChild("grand") == nullptr); // 비재귀 - 손자 미탐색
    REQUIRE(root.FindChild("grand", true) == grand); // 재귀 적중
    REQUIRE(root.FindChild("nope", true) == nullptr); // 미존재
}

TEST_CASE("Scene: FindChildIf - 술어 적중/미존재", "[scene]")
{
    Actor root("root");
    Actor *a = root.AddChild(std::make_unique<Actor>("a"));
    root.AddChild(std::make_unique<Actor>("b"));

    Actor *found = root.FindChildIf([](Actor *c) { return c->GetName() == "a"; });
    REQUIRE(found == a);
    Actor *none = root.FindChildIf([](Actor *) { return false; });
    REQUIRE(none == nullptr);
}

TEST_CASE("Scene: AddComponent 서로 다른 타입은 공존", "[scene]")
{
    Actor actor("a");
    auto *ca = actor.AddComponent<SpyComponentA>();
    auto *cb = actor.AddComponent<SpyComponentB>();
    REQUIRE(ca != nullptr);
    REQUIRE(cb != nullptr);
    REQUIRE(ca->GetOwner() == &actor); // mOwner 설정 확인
}

TEST_CASE("Scene: GetComponent - 구체 타입 적중 + 미존재 nullptr", "[scene]")
{
    Actor actor("a");
    auto *ca = actor.AddComponent<SpyComponentA>();
    REQUIRE(actor.GetComponent<SpyComponentA>() == ca); // fast-path 정확 매칭
    REQUIRE(actor.GetComponent<SpyComponentB>() == nullptr); // 미부착
}

TEST_CASE("Scene: GetComponent - 인터페이스 기반 조회(slow-path)", "[scene]")
{
    Actor actor("a");
    actor.AddComponent<PingComponent>();
    IPingable *ip = actor.GetComponent<IPingable>(); // dynamic_cast slow-path
    REQUIRE(ip != nullptr);
    REQUIRE(ip->Ping() == 42);
}

TEST_CASE("Scene: GetWorldMatrix - 부모/자식 translate 합성(곱 순서)", "[scene]")
{
    Actor parent("parent");
    parent.GetTransform().Translate = glm::vec3(10.0f, 0.0f, 0.0f);
    Actor *child = parent.AddChild(std::make_unique<Actor>("child"));
    child->GetTransform().Translate = glm::vec3(0.0f, 5.0f, 0.0f);

    // world = parentLocal * childLocal. 둘 다 순수 translate 이므로 합산된 위치.
    glm::mat4 world = child->GetWorldMatrix();
    // 행렬 마지막 컬럼(=평행이동) = (10, 5, 0).
    REQUIRE_THAT(world[3][0], WithinAbs(10.0f, 1e-6f));
    REQUIRE_THAT(world[3][1], WithinAbs(5.0f, 1e-6f));
    REQUIRE_THAT(world[3][2], WithinAbs(0.0f, 1e-6f));

    // 루트(parent)의 world == 자기 local.
    glm::mat4 rootWorld = parent.GetWorldMatrix();
    REQUIRE_THAT(rootWorld[3][0], WithinAbs(10.0f, 1e-6f));
}

TEST_CASE("Scene: Update - enabled 컴포넌트만 + 자식 재귀", "[scene]")
{
    Actor root("root");
    auto *rootComp = root.AddComponent<SpyComponentA>();
    Actor *child = root.AddChild(std::make_unique<Actor>("child"));
    auto *childComp = child->AddComponent<SpyComponentB>();

    root.Update(0.016f);
    REQUIRE(rootComp->updateCount == 1);  // enabled - 호출
    REQUIRE(childComp->updateCount == 1); // 자식 재귀

    rootComp->SetEnabled(false);
    root.Update(0.016f);
    REQUIRE(rootComp->updateCount == 1);  // disabled - 스킵
    REQUIRE(childComp->updateCount == 2); // 자식은 계속
}

TEST_CASE("Scene: Update - 비활성 Actor 는 자신+자식 전체 스킵", "[scene]")
{
    Actor root("root");
    auto *rootComp = root.AddComponent<SpyComponentA>();
    Actor *child = root.AddChild(std::make_unique<Actor>("child"));
    auto *childComp = child->AddComponent<SpyComponentB>();

    root.SetActive(false);
    root.Update(0.016f);
    REQUIRE(rootComp->updateCount == 0);  // 자신 스킵
    REQUIRE(childComp->updateCount == 0); // 자식 전체 스킵
}

TEST_CASE("Scene: OnEnter/OnExit - 중복 호출 가드 + IsEntered", "[scene]")
{
    Actor root("root");
    auto *spy = root.AddComponent<SpyComponentA>();
    REQUIRE_FALSE(root.IsEntered());

    root.OnEnter();
    REQUIRE(root.IsEntered());
    REQUIRE(spy->enterCount == 1);
    root.OnEnter(); // 이미 entered - no-op
    REQUIRE(spy->enterCount == 1);

    root.OnExit();
    REQUIRE_FALSE(root.IsEntered());
    REQUIRE(spy->exitCount == 1);
    root.OnExit(); // 이미 exited - no-op
    REQUIRE(spy->exitCount == 1);
}

TEST_CASE("Scene: entered 후 AddComponent 는 즉시 OnEnter (단일 호출 contract)", "[scene]")
{
    Actor root("root");
    root.OnEnter();
    auto *spy = root.AddComponent<SpyComponentA>(); // entered 상태 부착
    REQUIRE(spy->enterCount == 1); // 즉시 OnEnter
}
