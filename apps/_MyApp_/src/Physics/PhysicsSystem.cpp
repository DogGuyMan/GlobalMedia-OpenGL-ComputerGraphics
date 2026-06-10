/**
 * @file PhysicsSystem.cpp
 * @brief PhysicsSystem 구현 -- b2World 초기화 / Step / SyncToTransform.
 *
 * @details
 *  ### Init
 *  - @c b2World(b2Vec2(0,0)): top-down 슈터는 중력 불필요 -> gravity=0.
 *  - @c PhysicsContactListener 설치 -> BeginContact/EndContact 가 @c IContactable 에 디스패치.
 *
 *  ### Step
 *  - @c b2World::Step(dt, 8, 3): velocityIterations=8, positionIterations=3 (Box2D 공식 권장값).
 *  - Step 실행 중 @c b2World::IsLocked()==true.
 *    ContactListener 콜백 내부에서 SetEnabled/DestroyBody/CreateBody 는 abort ->
 *    콜백에서는 enqueue 만, Step 후 sweep 에서 실행.
 *
 *  ### SyncToTransform
 *  - root 부터 재귀 lambda DFS.
 *  - @c Components::FindPhysics(actor) 로 @c Physics 파생 컴포넌트 검색.
 *  - 물리 XY -> 렌더 XZ: tr.Translate = vec3(p.x, heightOffset, -p.y).
 */
#include "Physics/PhysicsSystem.h"
#include "Physics/PhysicsComponent.h"
#include "Physics/ContactListener.h"

#include "scene/actor.h"
#include "object/transform.h"

#include <spdlog/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::Physics
{
    PhysicsSystem::PhysicsSystem()  = default;
    PhysicsSystem::~PhysicsSystem() { Shutdown(); }

    void PhysicsSystem::Init()
    {
        if (mWorld) {
            spdlog::warn("[Physics] Init called twice - 무시");
            return;
        }
        mWorld = std::make_unique<b2World>(b2Vec2(0.0f, 0.0f));   // top-down: gravity 0
        mContactListener = std::make_unique<PhysicsContactListener>();
        mWorld->SetContactListener(mContactListener.get());
        spdlog::info("[Physics] b2World 생성 (gravity=0) + ContactListener 설치");
    }

    void PhysicsSystem::Shutdown()
    {
        mWorld.reset();
    }

    void PhysicsSystem::Step(float dt)
    {
        if (!mWorld) return;
        mWorld->Step(dt, /*velocityIterations=*/8, /*positionIterations=*/3);
    }

    void PhysicsSystem::SyncToTransform(SJH::Scene::Actor& root)
    {
        // root 부터 재귀 순회 - Components::Physics (BoxBody/CircleBody) 부착 Actor 의 Transform 갱신.
        auto traverse = [](SJH::Scene::Actor* actor, auto& self) -> void {
            if (!actor) return;
            auto* pb = Components::FindPhysics(actor);
            if (pb && pb->GetBody()) {
                const b2Vec2 p = pb->GetBody()->GetPosition();
                auto& tr = actor->GetTransform();
                // 물리 XY  렌더 XZ (Y 는 height offset).
                tr.Translate = vmath::vec3(p.x, pb->GetHeightOffset(), -p.y);
            }
            for (const auto& child : actor->GetChildren())
                self(child.get(), self);
        };
        traverse(&root, traverse);
    }
}
