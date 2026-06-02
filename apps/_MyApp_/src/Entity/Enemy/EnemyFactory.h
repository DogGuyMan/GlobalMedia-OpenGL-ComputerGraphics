#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__

#include "Entity/Components/LifeComponents.h"
#include "Entity/Enemy/EnemyDeathHandler.h"
#include "Entity/Enemy/SimplePursueAI.h"
#include "Spawns/Carrier.h"
#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/PhysicsLayer.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <memory>
#include <vmath.h>

namespace TopdownShooter::Entity::Enemy
{
    struct EnemyConfig
    {
        b2World*           world;
        vmath::vec2        pos;
        SJH::Scene::Actor* playerTarget;
        int   hp     = 30;
        float speed  = 2.0f;
        int   damage = 10;
        EnemyDeathHandler::DeathFx onDeathFx; // 사망 시 FX (외부 주입 — WaveController/main 에서 SpawnEnemyDeathFX 바인딩)
    };

    inline std::unique_ptr<SJH::Scene::Actor> CreateEnemyActor(const EnemyConfig& cfg)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>("Enemy");

        b2BodyDef bd;
        bd.type          = b2_dynamicBody;
        bd.position.Set(cfg.pos[0], cfg.pos[1]);
        bd.linearDamping = 0.5f;
        b2Body* body     = cfg.world->CreateBody(&bd);

        b2CircleShape circle;
        circle.m_radius = 0.4f;

        b2FixtureDef fd;
        fd.shape               = &circle;
        fd.density             = 1.0f;
        fd.friction            = 0.3f;
        fd.filter.categoryBits = Physics::ToBits(Physics::PhysicsLayer::Enemy);
        fd.filter.maskBits     = Physics::ToBits(Physics::EnemyMask);
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<Physics::Components::CircleBody>();
        pb->SetBody(body);

        actor->AddComponent<Components::Life>(cfg.hp);
        actor->AddComponent<SimplePursueAI>(cfg.playerTarget, body, cfg.speed);
        // 접촉 데미지 배달 = Carrier::ContactCarrier (적 body 재사용, 동작 보존 — IDamageable 배달).
        actor->AddComponent<Spawn::Carrier::ContactCarrier>(cfg.damage);
        if (cfg.onDeathFx)
            actor->AddComponent<EnemyDeathHandler>(cfg.onDeathFx);

        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__
