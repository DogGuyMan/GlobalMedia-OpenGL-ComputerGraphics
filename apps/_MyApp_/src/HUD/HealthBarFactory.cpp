#include "HUD/HealthBarFactory.h"

#include "HUD/HealthBarDriver.h"

#include "Entity/Components/Components.Interfaces.h" // TopdownShooter::Entity::ILivable
#include "material/material.h"
#include "material/pass.h"
#include "object/geometry.h"
#include "object/mesh.h"
#include "program/program.h" // SJH::Program (FindProgram/CreateProgram 반환형)
#include "render/mesh_renderer.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"

#include <GL/glcorearb.h> // GL_TRIANGLES
#include <memory>
#include <string>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::HUD
{
	namespace
	{
		constexpr char kQuadKey[]    = "ui_quad";
		constexpr char kProgramKey[] = "healthbar";
		constexpr char kVsPath[]     = "./resources/shaders/healthbar.vs";
		constexpr char kFsPath[]     = "./resources/shaders/healthbar.fs";

		// per-instance Material 키 고유화 — 다중 액터/중복 이름 충돌 회피.
		int gInstanceCounter = 0;
	} // namespace

	void AttachHealthBar(SJH::Scene::Actor &target, const HealthBarConfig &cfg)
	{
		auto *life = target.GetComponent<Entity::ILivable>();
		if (!life) return; // ILivable 없으면 부착 의미 없음 — silent no-op

		auto &reg = SJH::ResourceRegistry::Get();

		// 1) 공유 Program (healthbar.vs/.fs) — 없으면 생성.
		SJH::Program *prog = reg.FindProgram(kProgramKey);
		if (!prog)
			prog = reg.CreateProgram(kProgramKey, kVsPath, kFsPath);
		if (!prog) return; // 셰이더 컴파일 실패 (콘솔 InfoLog) — 부착 포기

		// 2) 공유 QuadMesh (Geometry::Plane — XY quad, z=0) — 없으면 등록.
		SJH::Mesh *quad = reg.FindMesh(kQuadKey);
		if (!quad)
		{
			SJH::MeshData data = SJH::Geometry::Plane();
			quad = reg.RegisterMesh(kQuadKey, SJH::Mesh::Create(data.vertices, data.indices, GL_TRIANGLES));
		}
		if (!quad) return;

		// 3) per-instance Material — 고유 키, Transparent pass, 초기 uniform.
		const std::string matKey =
		    "healthbar_" + target.GetName() + "_" + std::to_string(gInstanceCounter++);
		SJH::Material *mat = reg.CreateSharedMaterial(matKey);
		if (!mat) return;
		mat->SetProgram(prog);
		mat->SetPass(SJH::Pass::Kind::Transparent);
		mat->Properties.Vec4s["uColor"]           = cfg.fillColor;
		mat->Properties.Vec4s["uBgColor"]         = cfg.bgColor;
		mat->Properties.Floats["uSegmentCount"]   = cfg.segmentCount;
		mat->Properties.Floats["uSegmentSpacing"] = cfg.segmentSpacing;
		mat->Properties.Floats["uHeadOffset"]     = cfg.headOffset;
		mat->Properties.Floats["uFill"]           = 1.0f;

		// 4) 자식 Actor — local Translate=0 (빌보드가 부모 center 에서 cameraUp 으로 띄움),
		//    Scale 이 바 가로×세로. 부모 world transform 이 center 를 결정.
		auto bar = std::make_unique<SJH::Scene::Actor>("HealthBar");
		bar->GetTransform().Scale = vmath::vec3(cfg.size[0], cfg.size[1], 1.0f);
		auto *barPtr = target.AddChild(std::move(bar));

		// 5) MeshRenderer(QuadMesh, Material, +10) + Driver(Life→uFill).
		//    QueueOffset +10 — 같은 Transparent 큐(3000) 안에서 스프라이트 등 위로 정렬.
		barPtr->AddComponent<SJH::Scene::MeshRenderer>(quad, mat, /*queueOffset*/ 10);
		barPtr->AddComponent<TopdownShooter::HUD::HealthBarDriver>(life, mat);
	}
} // namespace TopdownShooter::HUD
