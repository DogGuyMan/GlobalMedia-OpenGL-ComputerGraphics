#ifndef __MYAPP_PHYSICS_WALL_FACTORY_H__
#define __MYAPP_PHYSICS_WALL_FACTORY_H__

#include "Physics/filter.h"
#include "Physics/physics_body.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <memory>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Physics
{
    /// @brief 정적 벽 Actor — b2_staticBody + box shape + PhysicsBodyComponent.
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
        fd.filter.categoryBits = ToBits(PhysicsLayer::Wall);
        fd.filter.maskBits     = ToBits(WallMask);
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<PhysicsBodyComponent>();
        pb->SetBody(body);
        pb->SetHeightOffset(0.0f);

        return actor;
    }
}

#endif // __MYAPP_PHYSICS_WALL_FACTORY_H__
