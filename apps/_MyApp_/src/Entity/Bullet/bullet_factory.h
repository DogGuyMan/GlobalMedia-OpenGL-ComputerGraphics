#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__

#include "Entity/Bullet/BulletContactHandler.h"
#include "Entity/Bullet/BulletLifetime.h"
#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/filter.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <memory>
#include <vmath.h>

namespace TopdownShooter::Entity::Bullet
{
    struct BulletConfig
    {
        b2World*    world;
        vmath::vec2 pos;
        vmath::vec2 dir;        // normalized
        float       speed    = 15.0f;
        int         damage   = 10;
        float       lifetime = 3.0f;
    };

    inline std::unique_ptr<SJH::Scene::Actor> CreateBulletActor(const BulletConfig& cfg)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>("Bullet");

        b2BodyDef bd;
        bd.type = b2_kinematicBody;
        bd.position.Set(cfg.pos[0], cfg.pos[1]);
        bd.linearVelocity.Set(cfg.dir[0] * cfg.speed, cfg.dir[1] * cfg.speed);
        b2Body* body = cfg.world->CreateBody(&bd);

        b2CircleShape circle;
        circle.m_radius = 0.15f;

        b2FixtureDef fd;
        fd.shape               = &circle;
        fd.density             = 1.0f;
        fd.isSensor            = false;
        fd.filter.categoryBits = Physics::ToBits(Physics::PhysicsLayer::BulletPlayer);
        fd.filter.maskBits     = Physics::ToBits(Physics::PhysicsLayer::Enemy |
                                                   Physics::PhysicsLayer::Wall);
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<Physics::Components::CircleBody>();
        pb->SetBody(body);

        actor->AddComponent<BulletContactHandler>(cfg.damage);
        actor->AddComponent<BulletLifetime>(cfg.lifetime);

        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__
