#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__

#include "Entity/Components/LifeComponents.h"
#include "Entity/Enemy/EnemyEntity.h"
#include "Entity/Enemy/SimplePursueAI.h"
#include "Physics/Constants.h"
#include "Spawns/Carrier.h"
#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/PhysicsImpulse.h"
#include "Physics/PhysicsLayer.h"
#include "Entity/Constants.h"
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
        int   hp     = ENEMY_HP;
        float speed  = ENEMY_SPEED;
        int   damage = ENEMY_DAMAGE;
    };

    inline std::unique_ptr<SJH::Scene::Actor> CreateEnemyActor(const EnemyConfig& cfg)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>("Enemy");

        Physics::Components::BodyConfig bc;
        bc.world         = cfg.world;
        bc.startPosition = cfg.pos;
        bc.linearDamping = ENEMY_LINEAR_DAMPING;
        bc.density       = 1.0f;
        bc.friction      = ENEMY_FRICTION;
        bc.categoryBits  = Physics::ToBits(Physics::PhysicsLayer::Enemy);
        bc.maskBits      = Physics::ToBits(Physics::EnemyMask);
        auto* pb = actor->AddComponent<Physics::Components::CircleBody>(bc, ENEMY_RADIUS);

        actor->AddComponent<Components::Life>(cfg.hp);
        actor->AddComponent<SimplePursueAI>(cfg.playerTarget, pb->GetBody(), cfg.speed);
        // 접촉 데미지 배달 = Carrier::ContactCarrier (적 body 재사용, 동작 보존 — IDamageable 배달).
        actor->AddComponent<Spawn::Carrier::ContactCarrier>(cfg.damage)
		->SetOwnerEntity(actor.get());

        actor->AddComponent<Physics::Impulse>(TopdownShooter::Physics::IMPULSE_ENEMY_FORCE);   // 넉백 타겟 (Carrier::Projectile 이 DoImpulse 배달)
        actor->AddComponent<EnemyEntity>();   // 적 Accessor-facade (Life/Physics/Director/Impulse 캐시)
        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__
