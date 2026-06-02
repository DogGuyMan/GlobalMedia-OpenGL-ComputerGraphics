#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__

#include "Entity/Bullet/BulletLifetime.h"
#include "Spawns/Carrier.h"
#include "Physics/PhysicsComponent.Imp.h"
#include "Physics/PhysicsLayer.h"
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

        Physics::Components::BodyConfig bc;
        bc.world          = cfg.world;
        bc.bodyType       = b2_kinematicBody;      // 총알 — 등속 비행(velocity), 충돌에 무반응
        bc.startPosition  = cfg.pos;
        bc.linearVelocity = vmath::vec2(cfg.dir[0] * cfg.speed, cfg.dir[1] * cfg.speed);
        bc.density        = 1.0f;
        bc.isSensor       = false;
        bc.categoryBits   = Physics::ToBits(Physics::PhysicsLayer::BulletPlayer);
        bc.maskBits       = Physics::ToBits(Physics::PhysicsLayer::Enemy |
                                            Physics::PhysicsLayer::Wall);
        actor->AddComponent<Physics::Components::CircleBody>(bc, 0.15f);

        // 데미지 배달 = Carrier::Projectile (IDamageable/IImpulsable 인터페이스 배달 + self-despawn).
        auto* proj = actor->AddComponent<Spawn::Carrier::Projectile>(cfg.damage);
        proj->SetOwnerEntity(nullptr); // 발사자 자가피해 방지 site (현재 물리 필터로 충분 — 후속 owner 주입 가능)
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
                SJH::MeshData data = SJH::Geometry::Sphere(0.0, kTwoPi, 16, 0.0, 1.0, 8, 0.15f);
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

            if (prog != nullptr && mesh != nullptr && mat != nullptr)
                actor->AddComponent<SJH::Scene::MeshRenderer>(mesh, mat);
        }

        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__
