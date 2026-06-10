/**
 * @file bullet_factory.h
 * @brief 플레이어 발사 총알 Actor 를 생성하는 인라인 팩토리 함수 모음.
 *
 * @details
 *  ### 책임
 *  - @c BulletConfig 설정값을 받아 총알 @c SJH::Scene::Actor 를 완성해 반환(@c CreateBulletActor).
 *  - Box2D @c CircleBody(Sensor), @c Carrier::Projectile(IDamageable 배달 + self-despawn),
 *    @c BulletLifetime(수명 만료 비활성화) 세 컴포넌트를 조립.
 *  - 공유 디버그 시각화 자원(Program/Mesh/Material)을 @c ResourceRegistry 에 1회만 등록해
 *    per-Actor 레지스트리 무한 증식을 방지.
 *
 *  ### 비-책임
 *  - [X] Scene 에 직접 추가 - 호출자(@c Weapon::UseWeapon)가 @c Director::Root().AddChild 로 관리.
 *  - [X] 물리 레이어 정책 결정 - @c PhysicsLayer 상수로 고정(변경 시 @c PhysicsLayer.h 수정).
 *  - [X] 비주얼 스프라이트/FX 교체 - 현재는 임시 Magenta 구(정식은 추후 sprite/Effekseer 로 대체).
 *
 * @note 헤더 전용(인라인 팩토리)이므로 @c bullet_factory.h 를 include 하면
 *       box2d + SJH::Scene + SJH::Geometry 등 무거운 헤더가 따라온다.
 *       @c WeaponComponents.cpp 처럼 .cpp 쪽에서만 include 해 헤더 전파를 최소화할 것.
 */
#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__

#include "Entity/Bullet/BulletLifetime.h"
#include "Spawns/Carrier.h"
#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/PhysicsLayer.h"
#include "Entity/Constants.h"
#include "scene/actor.h"
// 임시 비행 가시화 — 구 Mesh + Magenta Material + MeshRenderer (HealthBarFactory 패턴).
#include "object/geometry.h"
#include "object/mesh.h"
#include "material/material.h"
#include "material/pass.h"
#include "program/program.h"
#include "render/mesh_renderer.h"
#include "resource_registry/resource_registry.h"
#include <GL/glcorearb.h> // GL_TRIANGLES
#include <box2d/box2d.h>
#include <memory>
#include <vmath.h>

namespace TopdownShooter::Entity::Bullet
{
    /**
     * @brief 총알 Actor 생성에 필요한 설정 집합(PoD Config).
     * @details PlayerBuilder(또는 Weapon::UseWeapon) 에서 채워 @c CreateBulletActor 에 전달.
     *          기본값은 @c Entity::Constants 의 전역 상수(@c BULLET_SPEED 등)로 설정된다.
     */
    struct BulletConfig
    {
        b2World*    world;                   ///< 총알 body 를 등록할 Box2D 월드 (비소유).
        vmath::vec2 pos;                     ///< 스폰 위치(box2d 좌표계).
        vmath::vec2 dir;                     ///< 발사 방향(정규화된 단위 벡터, box2d 좌표계).
        float       speed    = BULLET_SPEED;   ///< 총알 초기 속력(units/sec).
        int         damage   = BULLET_DAMAGE;  ///< 적에게 가할 데미지.
        float       lifetime = BULLET_LIFETIME; ///< 총알 최대 수명(초). 초과 시 비활성.
    };

    /**
     * @brief @p cfg 설정값으로 총알 @c Actor 를 조립해 반환하는 인라인 팩토리.
     * @details 생성 순서:
     *  1. @c Physics::Components::CircleBody(Sensor) - BulletPlayer 레이어, Enemy|Wall 마스크.
     *  2. @c Spawn::Carrier::Projectile - IDamageable/IImpulsable 배달 + self-despawn.
     *  3. @c BulletLifetime - 수명 만료 시 @c Actor::SetActive(false).
     *  4. 디버그 시각화용 공유 Magenta 구 자원 등록(MeshRenderer 는 현재 주석 처리 - 임시).
     *
     * @param cfg 총알 설정 구조체.
     * @return 씬에 추가 가능한 총알 Actor (unique_ptr 소유권 반환).
     */
    inline std::unique_ptr<SJH::Scene::Actor> CreateBulletActor(const BulletConfig& cfg)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>("Bullet");

        Physics::Components::BodyConfig bc;
        bc.world          = cfg.world;
        bc.bodyType       = b2_dynamicBody;        // dynamic(월드중력 0 -> 안 떨어짐) — static 벽과도 접촉 생성(kinematic-static 은 접촉 0)
        bc.startPosition  = cfg.pos;
        bc.linearVelocity = vmath::vec2(cfg.dir[0] * cfg.speed, cfg.dir[1] * cfg.speed);
        bc.density        = 1.0f;
        bc.isSensor       = true;        // 트리거 — 벽/적 OnTriggerEnter 로 despawn (물리 밀어내기/바운스 없음)
        bc.categoryBits   = Physics::ToBits(Physics::PhysicsLayer::BulletPlayer);
        bc.maskBits       = Physics::ToBits(Physics::PhysicsLayer::Enemy |
                                            Physics::PhysicsLayer::Wall);
        actor->AddComponent<Physics::Components::CircleBody>(bc, BULLET_RADIUS);

        // 데미지 배달 = Carrier::Projectile (IDamageable/IImpulsable 인터페이스 배달 + self-despawn).
        auto* proj = actor->AddComponent<Spawn::Carrier::Projectile>(cfg.damage);
        proj->SetOwnerEntity(nullptr);   // 발사자 자가피해 방지 site (현재 물리 필터로 충분 — 후속 owner 주입 가능)
        proj->SetLaunchDir(cfg.dir);     // 넉백 방향 = 비행방향(box2d XY) — 위치차분 불안정 대체
        actor->AddComponent<BulletLifetime>(cfg.lifetime);

        // ── 임시: 비행 가시화용 Magenta 단색 구 (정식 비주얼은 추후 sprite/FX 로 대체) ──
        // 모든 총알이 동일한 형상 — 공유 키로 1회만 생성/등록 (per-actor 키 = registry 무한 증식 방지).
        {
            auto& reg = SJH::ResourceRegistry::Get();

            // 공유 Program (simple.vs/.fs — uModel/uView/uProj + baseColor 단색, 광원 무관)
            constexpr char kBulletProgKey[] = "bullet_debug";
            SJH::Program* prog = reg.FindProgram(kBulletProgKey);
            if (prog == nullptr)
                prog = reg.CreateProgram(kBulletProgKey,
                                         "./resources/shaders/simple.vs",
                                         "./resources/shaders/simple.fs");

            // 공유 Mesh (반지름 0.15 구 — 물리 CircleBody 와 동일 크기)
            constexpr char kBulletMeshKey[] = "bullet_debug_sphere";
            SJH::Mesh* mesh = reg.FindMesh(kBulletMeshKey);
            if (mesh == nullptr)
            {
                constexpr double kTwoPi = 6.283185307179586; // 2π — 경도 한 바퀴 (M_PI 의존 회피)
                SJH::MeshData data = SJH::Geometry::Sphere(0.0, kTwoPi, 16, 0.0, 1.0, 8, BULLET_RADIUS);
                mesh = reg.RegisterMesh(kBulletMeshKey,
                                        SJH::Mesh::Create(data.vertices, data.indices, GL_TRIANGLES));
            }

            // 공유 Material (Magenta 1,0,1 단색, Opaque pass)
            constexpr char kBulletMatKey[] = "bullet_debug_mat";
            SJH::Material* mat = reg.FindSharedMaterial(kBulletMatKey);
            if (mat == nullptr)
            {
                mat = reg.CreateSharedMaterial(kBulletMatKey);
                if (mat != nullptr)
                {
                    mat->SetProgram(prog);
                    mat->SetPass(SJH::Pass::Kind::Opaque);
                    mat->Properties.Vec4s["baseColor"] = vmath::vec4(1.0f, 0.0f, 1.0f, 1.0f);
                }
            }

        //     if (prog != nullptr && mesh != nullptr && mat != nullptr)
        //         actor->AddComponent<SJH::Scene::MeshRenderer>(mesh, mat);
        }

        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__
