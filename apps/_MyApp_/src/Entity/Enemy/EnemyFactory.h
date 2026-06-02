#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__

#include "Entity/Components/LifeComponents.h"
#include "Entity/Enemy/EnemyDeathHandler.h"
#include "Entity/Enemy/EnemyEntity.h"
#include "Entity/Enemy/SimplePursueAI.h"
#include "Spawns/Carrier.h"
#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/PhysicsImpulse.h"
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

        Physics::Components::BodyConfig bc;
        bc.world         = cfg.world;
        bc.startPosition = cfg.pos;
        bc.linearDamping = 0.5f;
        bc.density       = 1.0f;
        bc.friction      = 0.3f;
        bc.categoryBits  = Physics::ToBits(Physics::PhysicsLayer::Enemy);
        bc.maskBits      = Physics::ToBits(Physics::EnemyMask);
        auto* pb = actor->AddComponent<Physics::Components::CircleBody>(bc, 0.4f);

        actor->AddComponent<Components::Life>(cfg.hp);
        actor->AddComponent<SimplePursueAI>(cfg.playerTarget, pb->GetBody(), cfg.speed);
        // 접촉 데미지 배달 = Carrier::ContactCarrier (적 body 재사용, 동작 보존 — IDamageable 배달).
        actor->AddComponent<Spawn::Carrier::ContactCarrier>(cfg.damage);
        if (cfg.onDeathFx)
            actor->AddComponent<EnemyDeathHandler>(cfg.onDeathFx);

        actor->AddComponent<Physics::Impulse>();   // 넉백 타겟 (Carrier::Projectile 이 DoImpulse 배달)
        actor->AddComponent<EnemyEntity>();   // 적 Accessor-facade (Life/Physics/Director/Impulse 캐시)
        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__
