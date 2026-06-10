/**
 * @file EnemyFactory.h
 * @brief 적 Actor 조립 팩토리 - Config -> Component 부착 -> 완성된 Actor 반환.
 *
 * @details
 *  ### 책임
 *  - @c EnemyConfig (PoD) 를 받아 적 Actor 한 개를 조립하고 소유권을 호출자에게 넘긴다.
 *  - 물리 body(CircleBody) + Life + 추적 AI + 접촉 데미지 배달 + 넉백 + facade 를 한 자리에서 wiring.
 *
 *  ### 비-책임
 *  - [X] 프레임 추적 로직 - @c SimplePursueAI 담당.
 *  - [X] 적 능력 노출 - @c EnemyEntity facade 담당.
 *  - [X] 튜닝 수치 보유 - @c Physics/Constants.h / @c Entity/Constants.h 단일 소스.
 *
 *  ### 정통 매핑
 *  - Unity Prefab / Cocos2D create() - Config 로 한 Actor 를 통째로 찍어내는 factory.
 *
 * @note header-only inline factory. Component 부착 순서가 의존 wiring 순서와 일치한다.
 */
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
    /**
     * @brief 적 Actor 생성 매개변수 - PoD config.
     * @details world/pos/playerTarget 은 필수(기본값 없음), 나머지 스탯은 @c Entity/Constants.h 기본값.
     */
    struct EnemyConfig
    {
        b2World*           world;        ///< 적 물리 body 를 생성할 Box2D 월드 (필수).
        vmath::vec2        pos;          ///< 스폰 위치 (Box2D XY 평면).
        SJH::Scene::Actor* playerTarget; ///< 추적 대상 Actor (플레이어) - SimplePursueAI 가 매 프레임 추종.
        int   hp     = ENEMY_HP;         ///< 초기 체력 (Life Component).
        float speed  = ENEMY_SPEED;      ///< 추적 이동 속도 (SimplePursueAI).
        int   damage = ENEMY_DAMAGE;     ///< 접촉 데미지 (ContactCarrier 가 IDamageable 에게 배달).
    };

    /// @brief 적 Actor 한 개를 조립해 소유권과 함께 반환.
    /// @details
    ///  부착 순서: CircleBody(eager 물리 body) -> Life -> SimplePursueAI(playerTarget/body/speed 주입)
    ///  -> ContactCarrier(접촉 데미지 배달) -> Impulse(넉백 타겟) -> EnemyEntity(facade, 형제 캐시).
    ///  facade(EnemyEntity) 를 마지막에 부착해 OnEnter 시 모든 형제 Component 가 이미 존재하도록 보장한다.
    /// @param cfg 적 스폰 설정 (world/pos/playerTarget 필수).
    /// @return 완성된 적 Actor (호출자가 씬에 AddChild).
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
        // 접촉 데미지 배달 = Carrier::ContactCarrier (적 body 재사용, 동작 보존 - IDamageable 배달).
        actor->AddComponent<Spawn::Carrier::ContactCarrier>(cfg.damage)
		->SetOwnerEntity(actor.get());

        actor->AddComponent<Physics::Impulse>(TopdownShooter::Physics::IMPULSE_ENEMY_FORCE);   // 넉백 타겟 (Carrier::Projectile 이 DoImpulse 배달)
        actor->AddComponent<EnemyEntity>();   // 적 Accessor-facade (Life/Physics/Director/Impulse 캐시)
        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__
