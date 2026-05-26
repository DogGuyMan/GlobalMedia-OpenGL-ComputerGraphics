#include "Physics/physics_system.h"
#include "Physics/PhysicsComponent.h"
#include "Physics/contact_listener.h"

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
            spdlog::warn("[Physics] Init called twice — 무시");
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
        // root 부터 재귀 순회 — Components::Physics (BoxBody/CircleBody) 부착 Actor 의 Transform 갱신.
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
