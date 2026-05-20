#include <catch2/catch_test_macros.hpp>
#include "scene/actor.h"

#include <memory>   // std::make_unique

namespace {
    struct CountingComponent : public SJH::Scene::Component
    {
        int onEnterCount = 0;
        int onExitCount  = 0;
        int updateCount  = 0;

        void OnEnter() override { ++onEnterCount; }
        void OnExit()  override { ++onExitCount;  }
        void Update(float) override { ++updateCount; }
    };
}

// ======================================================================
//  TEST_CASE 1 — OnEnter 이전 AddComponent: lazy entry
// ======================================================================
TEST_CASE("Actor — AddComponent before OnEnter, lazy entry", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    auto* comp = root.AddComponent<CountingComponent>();

    REQUIRE(comp->onEnterCount == 0);   // 아직 OnEnter 안 호출

    root.OnEnter();
    REQUIRE(comp->onEnterCount == 1);   // cascade 로 호출
}

// ======================================================================
//  TEST_CASE 2 — OnEnter 이후 AddComponent: immediate entry (D-9 contract)
// ======================================================================
TEST_CASE("Actor — AddComponent after OnEnter, immediate entry (D-9)", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    root.OnEnter();
    REQUIRE(root.IsEntered());

    auto* comp = root.AddComponent<CountingComponent>();
    REQUIRE(comp->onEnterCount == 1);   // 즉시 호출 — D-9 contract
}

// ======================================================================
//  TEST_CASE 3 — OnExit cascade: 자식 컴포넌트 cleanup
// ======================================================================
TEST_CASE("Actor — OnExit cascade, children reverse order", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    auto child = std::make_unique<SJH::Scene::Actor>("Child");
    auto* childComp = child->AddComponent<CountingComponent>();
    auto* childPtr  = root.AddChild(std::move(child));

    root.OnEnter();
    root.OnExit();

    REQUIRE(childComp->onExitCount == 1);   // 자식 컴포넌트도 cleanup
    REQUIRE_FALSE(childPtr->IsEntered());
}

// ======================================================================
//  TEST_CASE 4 — SetEnabled false: Update 만 토글, OnEnter/OnExit 무관
// ======================================================================
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

// ======================================================================
//  TEST_CASE 5 — SetActive false: 자기 + 자식 Update 차단
// ======================================================================
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
    REQUIRE(rootComp->updateCount  == 0);
    REQUIRE(childComp->updateCount == 0);   // 자식도 차단
}

// ======================================================================
//  TEST_CASE 6 — AddChild(nullptr): safe early return
// ======================================================================
TEST_CASE("Actor — AddChild(nullptr) safe early return", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    auto* result = root.AddChild(nullptr);
    REQUIRE(result == nullptr);
    REQUIRE(root.GetChildren().empty());
}

// ======================================================================
//  TEST_CASE 7 — GetWorldMatrix: 부모 체인 합성 (10 + 5 = 15)
// ======================================================================
TEST_CASE("Actor — GetWorldMatrix parent chain composition", "[scene][actor]")
{
    SJH::Scene::Actor root("Root");
    root.GetTransform().Translate = vmath::vec3(10.0f, 0.0f, 0.0f);

    auto child = std::make_unique<SJH::Scene::Actor>("Child");
    child->GetTransform().Translate = vmath::vec3(5.0f, 0.0f, 0.0f);
    auto* childPtr = root.AddChild(std::move(child));

    const vmath::mat4 world = childPtr->GetWorldMatrix();
    // vmath mat4 는 컬럼-메이저: data[col][row] — data[3][0] 이 X translation
    REQUIRE(world[3][0] == 15.0f);
}
