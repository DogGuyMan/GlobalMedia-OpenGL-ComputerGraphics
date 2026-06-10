/**
 * @file wall_factory.h
 * @brief 정적 solid 벽 Actor 를 생성하는 팩토리 함수.
 *
 * @details
 *  ### 책임
 *  - Box2D b2_staticBody + isSensor=false (Unity isTrigger=false) solid 벽 Actor 를 생성한다.
 *  - @c BoxBody Component 를 자동으로 부착하고 PhysicsLayer::Wall 카테고리 / WallMask 를 설정한다.
 *
 *  ### 비-책임
 *  - [X] 렌더 메시 부착 — 호출자가 MeshRenderer 등을 추가해야 한다.
 *  - [X] 씬 등록 — 반환된 unique_ptr 를 호출자가 addChild 로 직접 등록한다.
 *
 * @note half 파라미터는 half-extents (박스 절반 크기).
 *       BoxBody 는 SetAsBox 에 full size(half*2) 를 전달하므로 절반 크기가 보존된다.
 */
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
    /**
     * @brief 정적 solid 벽 Actor 를 생성한다 — b2_staticBody + box shape + @c BoxBody Component.
     * @details
     *  생성된 Actor 에는 @c BoxBody Component 가 자동 부착된다.
     *  - b2_staticBody: 중력/이동 없는 고정 벽.
     *  - isSensor=false: solid — 다른 바디가 관통하지 못한다.
     *  - categoryBits = PhysicsLayer::Wall, maskBits = WallMask.
     *
     * @param name   Actor 이름.
     * @param world  b2World 레퍼런스 (lifetime = PhysicsSystem).
     * @param center 벽 중심 좌표 (XY 평면).
     * @param half   half-extents (박스 절반 크기). BoxBody 내부에서 half*2 를 SetAsBox 에 전달.
     * @return 생성된 벽 Actor 의 @c unique_ptr. 씬 등록 및 MeshRenderer 부착은 호출자 책임.
     */
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
