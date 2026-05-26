#include "Stage/StageBuilder.h"
#include "Stage/Components/StageStateComponent.h"
#include "Stage/Factories/pickup_factory.h"
#include "Stage/Factories/wall_factory.h"

#include "material/material.h"
#include "material/material_uniforms.h"
#include "object/mesh.h"
#include "program/program.h"
#include "render/mesh_renderer.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"

#include <box2d/box2d.h>
#include <cassert>
#include <string>

namespace TopdownShooter::Stage
{
    namespace
    {
        // wall/pickup 시각화 자원의 registry key — "stage_" prefix 로 영역 명시.
        constexpr const char* kPlaneKey     = "stage_plane";
        constexpr const char* kProgKey      = "stage_solid_plane";
        constexpr const char* kWallMatKey   = "stage_wall";
        constexpr const char* kPickupMatKey = "stage_pickup";
        constexpr const char* kVS           = "resources/shaders/simple.vs";
        constexpr const char* kFS           = "resources/shaders/simple.fs";

        SJH::Mesh* EnsurePlane(SJH::ResourceRegistry& reg)
        {
            if (auto* existing = reg.FindMesh(kPlaneKey))
                return existing;
            auto planeUPtr = SJH::Mesh::CreatePlane();
            return reg.RegisterMesh(kPlaneKey, std::move(planeUPtr));
        }

        SJH::Program* EnsureProgram(SJH::ResourceRegistry& reg)
        {
            if (auto* existing = reg.FindProgram(kProgKey))
                return existing;
            return reg.CreateProgram(kProgKey, kVS, kFS);
        }

        SJH::Material* EnsureMaterial(SJH::ResourceRegistry& reg, const std::string& key,
                                       SJH::Program* prog, vmath::vec4 baseColor)
        {
            if (auto* existing = reg.FindSharedMaterial(key))
                return existing;
            auto* mat = reg.CreateSharedMaterial(key);
            mat->SetProgram(prog);
            mat->SetPass(SJH::Pass::Kind::Opaque);
            SJH::Uniforms::SetVec4(*mat, "baseColor", baseColor);
            return mat;
        }
    } // namespace

    std::unique_ptr<SJH::Scene::Actor> CreateStageActor(const StageConfig& cfg)
    {
        assert(cfg.world    != nullptr && "StageConfig::world 가 nullptr — PhysicsSystem 미초기화 상태에서 호출됨");
        assert(cfg.registry != nullptr && "StageConfig::registry 가 nullptr");

        auto& reg = *cfg.registry;

        // 1) 공유 자원 등록 — idempotent (이미 있으면 Find 로 재사용)
        SJH::Mesh*     plane     = EnsurePlane(reg);
        SJH::Program*  solidProg = EnsureProgram(reg);
        SJH::Material* wallMat   = EnsureMaterial(reg, kWallMatKey, solidProg,
                                                   vmath::vec4(0.55f, 0.55f, 0.60f, 1.0f));
        SJH::Material* pickupMat = EnsureMaterial(reg, kPickupMatKey, solidProg,
                                                   vmath::vec4(1.0f, 0.85f, 0.2f, 1.0f));

        // 2) Stage Actor + StageState Component
        auto stage = std::make_unique<SJH::Scene::Actor>("MainStage");
        auto* stageState = stage->AddComponent<Components::StageState>();
        stageState->SetCurrent(cfg.startStatus);

        // 3) 벽 4개 — arena 안쪽 둘레
        const float arena = cfg.arenaHalfExtent;
        const float wallH = cfg.wallThickness;
        auto spawnWall = [&](const char* name, vmath::vec2 center, vmath::vec2 half) {
            auto a = Factories::CreateWallActor(name, *cfg.world, center, half);
            a->GetTransform().Scale = vmath::vec3(half[0] * 2.0f, 1.0f, half[1] * 2.0f);
            a->AddComponent<SJH::Scene::MeshRenderer>(plane, wallMat);
            stage->AddChild(std::move(a));
        };
        spawnWall("WallTop",    vmath::vec2(0.0f, +arena), vmath::vec2(arena, wallH));
        spawnWall("WallBottom", vmath::vec2(0.0f, -arena), vmath::vec2(arena, wallH));
        spawnWall("WallLeft",   vmath::vec2(-arena, 0.0f), vmath::vec2(wallH, arena));
        spawnWall("WallRight",  vmath::vec2(+arena, 0.0f), vmath::vec2(wallH, arena));

        // 4) Pickup Sensor — cfg.pickupPositions 각 좌표마다 하나씩
        for (std::size_t i = 0; i < cfg.pickupPositions.size(); ++i)
        {
            const auto& pos = cfg.pickupPositions[i];
            auto name = std::string("PickupTest") + std::to_string(i);
            auto p = Factories::CreatePickupActor(std::move(name), *cfg.world, pos, vmath::vec2(0.8f, 0.8f));
            p->GetTransform().Scale = vmath::vec3(1.6f, 1.0f, 1.6f);
            p->AddComponent<SJH::Scene::MeshRenderer>(plane, pickupMat);
            stage->AddChild(std::move(p));
        }

        return stage;
    }
} // namespace TopdownShooter::Stage
