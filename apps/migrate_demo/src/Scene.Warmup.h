#pragma once

#include "material/material_uniforms.h"
#include "object/mesh.h"
#include "program/program.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/components.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"
#include <memory>
#include <sys/syslimits.h>


namespace MigrateDemo::Scene
{
	inline void WarmupActors(const SJH::Program *const prog, SJH::Scene::Director &dir)
	{
		auto &reg = SJH::ResourceRegistry::Get();

		{
			const char *ACTOR_KEY = "Box0";
			auto box = std::make_unique<SJH::Scene::Actor>(ACTOR_KEY);
			box->AddComponent<SJH::Scene::MeshRenderer>(
			    reg.RegisterMesh(ACTOR_KEY, SJH::Mesh::CreateBox()),
			    &reg.CreateMaterial(ACTOR_KEY)
			         ->SetProgram(prog));
			SJH::Uniforms::SetVec3(*box->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.diffuse", vmath::vec3(0.8f, 0.3f, 0.3f));
			SJH::Uniforms::SetVec3(*box->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.specular", vmath::vec3(0.5f, 0.5f, 0.5f));
			SJH::Uniforms::SetFloat(*box->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.shininess", 32.0f);

			dir.Root().AddChild(std::move(box));
		}

		{
			const char *ACTOR_KEY = "Box1";
			auto box = std::make_unique<SJH::Scene::Actor>(ACTOR_KEY);
			box->AddComponent<SJH::Scene::MeshRenderer>(
			    reg.RegisterMesh(ACTOR_KEY, SJH::Mesh::CreateBox()),
			    &reg.CreateMaterial(ACTOR_KEY)
			         ->SetProgram(prog));
			SJH::Uniforms::SetVec3(*box->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.diffuse", vmath::vec3(0.8f, 0.3f, 0.3f));
			SJH::Uniforms::SetVec3(*box->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.specular", vmath::vec3(0.5f, 0.5f, 0.5f));
			SJH::Uniforms::SetFloat(*box->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.shininess", 32.0f);

			dir.Root().AddChild(std::move(box));
		}

		{
			const char *ACTOR_KEY = "Box2";
			auto box = std::make_unique<SJH::Scene::Actor>(ACTOR_KEY);
			{
				box->GetTransform().Translate = vmath::vec3(2.0f, 0.5f, 0.0f);
			}
			box->AddComponent<SJH::Scene::MeshRenderer>(
			    reg.RegisterMesh(ACTOR_KEY, SJH::Mesh::CreateBox()),
			    &reg.CreateMaterial(ACTOR_KEY)
			         ->SetProgram(prog));
			SJH::Uniforms::SetVec3(*box->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.diffuse", vmath::vec3(0.3f, 0.5f, 0.8f));
			SJH::Uniforms::SetVec3(*box->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.specular", vmath::vec3(0.5f, 0.5f, 0.5f));
			SJH::Uniforms::SetFloat(*box->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.shininess", 32.0f);

			dir.Root().AddChild(std::move(box));
		}

		{
			const char *ACTOR_KEY = "Plane";
			auto plane = std::make_unique<SJH::Scene::Actor>("Plane");
			{
				plane->GetTransform().Translate = vmath::vec3(0.0f, -1.0f, 0.0f);
				plane->GetTransform().Scale = vmath::vec3(10.0f, 1.0f, 10.0f);
			}

			plane->AddComponent<SJH::Scene::MeshRenderer>(
			    reg.RegisterMesh(ACTOR_KEY, SJH::Mesh::CreatePlane()),
			    &reg.CreateMaterial(ACTOR_KEY)
			         ->SetProgram(prog));
			SJH::Uniforms::SetVec3(*plane->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.diffuse", vmath::vec3(0.8f, 0.3f, 0.3f));
			SJH::Uniforms::SetVec3(*plane->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.specular", vmath::vec3(0.5f, 0.5f, 0.5f));
			SJH::Uniforms::SetFloat(*plane->GetComponent<SJH::Scene::MeshRenderer>()->Material, "material.shininess", 32.0f);
			dir.Root().AddChild(std::move(plane));
		}
	}

	inline void WarmupLights(const SJH::Program *const prog, SJH::Scene::Director &dir)
	{

		// 5) DirLight Actor — Compound (정통 LearnOpenGL 태양).
		dir.Root().AddChild(SJH::Scene::CreateDirLightActor(
		    "Sun", vmath::vec3(-0.2f, -1.0f, -0.3f)));

		// 6) PointLight Actor — Compound.
		dir.Root().AddChild(SJH::Scene::CreatePointLightActor(
		    "Lamp", vmath::vec3(3.0f, 2.0f, 3.0f), 32.0f));
	}
} // namespace Manager