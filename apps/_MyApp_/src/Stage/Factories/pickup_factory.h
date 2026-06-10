/**
 * @file pickup_factory.h
 * @brief 정적 Sensor 박스 픽업 Actor 를 생성하는 팩토리 함수.
 *
 * @details
 *  ### 책임
 *  - Box2D b2_staticBody + isSensor=true (Unity isTrigger=true) Sensor 영역 Actor 를 생성한다.
 *  - @c BoxBody Component + @c PickupTriggerLogger Component 를 자동으로 부착한다.
 *  - PhysicsLayer::Pickup 카테고리 / PhysicsLayer::Player 마스크로 충돌 필터링을 설정한다.
 *
 *  ### 비-책임
 *  - [X] 픽업 동작(HP 회복, ammo 등) — 별도 Component 를 addComponent 로 추가해야 한다.
 *  - [X] 씬 등록 — 반환된 unique_ptr 를 호출자가 addChild 로 직접 등록한다.
 *
 * @note half 파라미터는 half-extents (박스 절반 크기). BoxBody 는 full size(half*2) 로 SetAsBox.
 */
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
    /**
     * @brief 정적 Sensor 박스 픽업 Actor 를 생성한다 — Unity @c isTrigger=true 영역과 동일.
     * @details
     *  생성된 Actor 에는 다음 Component 가 자동 부착된다:
     *  - @c BoxBody : b2_staticBody + isSensor=true, PhysicsLayer::Pickup|Player 필터.
     *  - @c PickupTriggerLogger : 진입/이탈 spdlog 로깅.
     *
     *  실제 픽업 동작(HP 회복 등)은 호출자가 반환된 Actor 에 추가 Component 를 부착해야 한다.
     *
     * @param name   Actor 이름.
     * @param world  b2World 레퍼런스 (lifetime = PhysicsSystem).
     * @param center 박스 중심 좌표 (XY 평면).
     * @param half   half-extents (박스 절반 크기). BoxBody 내부에서 half*2 를 SetAsBox 에 전달.
     * @return 생성된 픽업 Actor 의 @c unique_ptr. 씬 등록은 호출자 책임.
     */
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
