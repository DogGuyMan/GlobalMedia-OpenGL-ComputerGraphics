/**
 * @file sprite_resources.cpp
 * @brief Sprite 공유 자원 해결 구현 - plane Mesh + template/instance Material lazy 생성.
 *
 * @details
 *  2026-06-11 E6(D8) - 과거 @c sprite_component.cpp 의 익명 ns 헬퍼 3종을 본 TU 로 이주.
 *  - @c EnsureSharedPlane()      - @c _sprite_plane (Mesh) 최초 1회.
 *  - @c EnsureTemplateMaterial() - @c _sprite_billboard_program (Program) + @c _sprite_billboard (SharedMaterial) 최초 1회.
 *  - @c CreateInstanceMaterial() - @c _sprite_inst_N (MaterialInstance) per 호출.
 *  로직/키 컨벤션/생성 순서는 이주 전과 동일 (동작 보존).
 */
#include "resource_registry/sprite_resources.h"

#include "resource_registry/resource_registry.h"
#include "material/material.h"
#include "material/material_uniforms.h"
#include "material/pass.h"
#include "object/mesh.h"
#include "sprite/uniform_atlas.h"

#include <spdlog/spdlog.h>
#include <string>

namespace SJH::SpriteResources
{
	namespace
	{
		constexpr const char *kPlaneKey = "_sprite_plane";
		constexpr const char *kProgramKey = "_sprite_billboard_program";
		constexpr const char *kTemplateKey = "_sprite_billboard";

		SJH::Material *EnsureTemplateMaterial()
		{
			auto &reg = ResourceRegistry::Get();
			if (auto *tpl = reg.FindSharedMaterial(kTemplateKey))
				return tpl;

			SJH::Program *prog = reg.FindProgram(kProgramKey);
			if (!prog)
			{
				prog = reg.CreateProgram(
				    kProgramKey,
				    "resources/shaders/billboard_atlas.vs",
				    "resources/shaders/billboard_atlas.fs");
				if (!prog)
				{
					spdlog::error("SpriteResources: billboard_atlas shader 로드 실패");
					return nullptr;
				}
			}

			auto *tpl = reg.CreateSharedMaterial(kTemplateKey);
			if (tpl)
			{
				tpl->SetProgram(prog);
				tpl->SetPass(SJH::Pass::Kind::AlphaTest);
			}
			return tpl;
		}
	} // namespace

	SJH::Mesh *EnsureSharedPlane()
	{
		auto &reg = ResourceRegistry::Get();
		if (auto *m = reg.FindMesh(kPlaneKey))
			return m;
		return reg.RegisterMesh(kPlaneKey, SJH::Mesh::CreatePlane());
	}

	SJH::Material *CreateInstanceMaterial(SJH::Sprite::UniformAtlas *atlas)
	{
		if (!atlas)
			return nullptr;
		auto &reg = ResourceRegistry::Get();
		auto *tpl = EnsureTemplateMaterial();
		if (!tpl)
			return nullptr;

		// 단조 증가 - 동일 프로세스 안 unique. ResourceRegistry::Clear 이후에도 충돌 없음.
		static int counter = 0;
		const std::string key = std::string("_sprite_inst_") + std::to_string(++counter);
		auto *inst = reg.CreateMaterialInstanceFrom(key, tpl);
		if (!inst)
			return nullptr;

		inst->Properties.Textures["uAtlas"] = {atlas->GetTexture(), /*unit=*/0};
		Uniforms::SetVec4(*inst, "uUvRect", atlas->GetUVRect(/*frameIdx=*/0));
		Uniforms::SetFloat(*inst, "uFlipX", 1.0f);
		Uniforms::SetVec4(*inst, "uTint", vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		return inst;
	}
} // namespace SJH::SpriteResources
