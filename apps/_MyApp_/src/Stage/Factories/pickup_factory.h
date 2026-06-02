#ifndef __TOPDOWNSHOOTER_STAGE_FACTORIES_PICKUP_FACTORY_H__
#define __TOPDOWNSHOOTER_STAGE_FACTORIES_PICKUP_FACTORY_H__

#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/PhysicsLayer.h"
#include "Stage/Components/PickupTriggerLogger.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <memory>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Stage::Factories
{
    /// @brief 정적 Sensor 박스 — Unity isTrigger=true 영역과 동일.
    /// @details PickupTriggerLogger Component 는 분리됨 (Stage/Components/PickupTriggerLogger.{h,cpp} 참조).
    /// @param name   Actor 이름
    /// @param world  b2World
    /// @param center 박스 중심 (XY 평면)
    /// @param half   half-extents
    inline std::unique_ptr<SJH::Scene::Actor> CreatePickupActor(
        std::string name, b2World& world, vmath::vec2 center, vmath::vec2 half)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>(std::move(name));

        TopdownShooter::Physics::Components::BodyConfig bc;
        bc.world         = &world;
        bc.bodyType      = b2_staticBody;
        bc.startPosition = center;
        bc.isSensor      = true;   // Unity isTrigger ON
        bc.categoryBits  = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Pickup);
        bc.maskBits      = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Player);
        actor->AddComponent<TopdownShooter::Physics::Components::BoxBody>(bc, vmath::vec2(half[0] * 2.0f, half[1] * 2.0f));

        actor->AddComponent<Components::PickupTriggerLogger>();

        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_STAGE_FACTORIES_PICKUP_FACTORY_H__
