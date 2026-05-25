#ifndef __MYAPP_PHYSICS_PICKUP_FACTORY_H__
#define __MYAPP_PHYSICS_PICKUP_FACTORY_H__

#include "Physics/contact_interface.h"
#include "Physics/filter.h"
#include "Physics/physics_body.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Physics
{
    /// @brief 픽업/감지 영역 — Trigger 이벤트를 로그로 출력 (M3 시각 검증용).
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

    /// @brief 정적 Sensor 박스 — Unity isTrigger=true 영역과 동일.
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
        fd.filter.categoryBits = Filter::PICKUP;
        fd.filter.maskBits     = Filter::PLAYER;
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<PhysicsBodyComponent>();
        pb->SetBody(body);
        pb->SetSensor(true);

        actor->AddComponent<PickupTriggerLogger>();

        return actor;
    }
}

#endif // __MYAPP_PHYSICS_PICKUP_FACTORY_H__
