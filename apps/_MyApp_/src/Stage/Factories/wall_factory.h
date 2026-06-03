#ifndef __TOPDOWNSHOOTER_STAGE_FACTORIES_WALL_FACTORY_H__
#define __TOPDOWNSHOOTER_STAGE_FACTORIES_WALL_FACTORY_H__

#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/PhysicsLayer.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <memory>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Stage::Factories
{
    /// @brief 정적 벽 Actor — b2_staticBody + box shape + Components::BoxBody.
    /// @param name   Actor 이름
    /// @param world  b2World (lifetime = PhysicsSystem)
    /// @param center 벽 중심 (XY 평면)
    /// @param half   half-extents (box 절반 크기)
    inline std::unique_ptr<SJH::Scene::Actor> CreateWallActor(
        std::string name, b2World& world, vmath::vec2 center, vmath::vec2 half)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>(std::move(name));

        TopdownShooter::Physics::Components::BodyConfig bc;
        bc.world         = &world;
        bc.bodyType      = b2_staticBody;          // 정적 벽 — 추락/이동 없음
        bc.startPosition = center;
        bc.isSensor      = false;                  // solid — Unity isTrigger OFF
        bc.categoryBits  = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Wall);
        bc.maskBits      = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::WallMask);
        // half = half-extents -> BoxBody 는 size(full)*0.5 로 SetAsBox 하므로 half*2 전달(절반크기 보존).
        actor->AddComponent<TopdownShooter::Physics::Components::BoxBody>(bc, vmath::vec2(half[0] * 2.0f, half[1] * 2.0f));

        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_STAGE_FACTORIES_WALL_FACTORY_H__
