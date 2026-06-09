/**
 * @file sprite_component.cpp
 * @brief SpriteRenderer 구현 - ResourceRegistry 경유 공유 자원 lazy 해결 + per-instance Material 생성 + uniform 동기화.
 *
 * @details
 *  ### 책임
 *  - 익명 namespace 헬퍼 3종:
 *    - @c EnsureSharedPlane()      - @c _sprite_plane (Mesh) 최초 1회 생성/등록.
 *    - @c EnsureTemplateMaterial() - @c _sprite_billboard_program (Program) + @c _sprite_billboard (SharedMaterial) 최초 1회.
 *    - @c CreateInstanceMaterial() - @c _sprite_inst_N (MaterialInstance) per-SpriteRenderer 생성.
 *  - @c SpriteRenderer::Update : atlas UV / tint / flipX / roll / 피격 / 디졸브 uniform 매 프레임 동기화.
 *
 *  ### ResourceRegistry 키 컨벤션 ('_' 접두 - 사용자 namespace 격리)
 *  - @c "_sprite_plane"             (Mesh)
 *  - @c "_sprite_billboard_program" (Program)
 *  - @c "_sprite_billboard"         (template SharedMaterial)
 *  - @c "_sprite_inst_N"            (per-instance MaterialInstance, N = 단조 증가 counter)
 */
#include "sprite/sprite_component.h"

#include "material/material.h"
#include "material/material_uniforms.h"
#include "material/pass.h"
#include "object/mesh.h"
#include "resource_registry/resource_registry.h"
#include "sprite/uniform_atlas.h"

#include <spdlog/spdlog.h>
#include <string>

namespace SJH::Sprite
{
	namespace
	{
		constexpr const char *kPlaneKey = "_sprite_plane";
		constexpr const char *kProgramKey = "_sprite_billboard_program";
		constexpr const char *kTemplateKey = "_sprite_billboard";

		SJH::Mesh *EnsureSharedPlane()
		{
			auto &reg = ResourceRegistry::Get();
			if (auto *m = reg.FindMesh(kPlaneKey))
				return m;
			return reg.RegisterMesh(kPlaneKey, SJH::Mesh::CreatePlane());
		}

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
					spdlog::error("SpriteRenderer: billboard_atlas shader 로드 실패");
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

		SJH::Material *CreateInstanceMaterial(UniformAtlas *atlas)
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
	} // namespace

	SpriteRenderer::SpriteRenderer(UniformAtlas *atlasPtr)
	    : MeshRenderer(EnsureSharedPlane(), CreateInstanceMaterial(atlasPtr)),
	      atlas(atlasPtr)
	{
	}

	void SpriteRenderer::Update(float dt)
	{
		if (!atlas || !Material)
			return;
		mEffectClock += dt;   // uTime free-running clock

		Uniforms::SetVec4(*Material, "uUvRect", atlas->GetUVRect(frameIdx));
		Uniforms::SetFloat(*Material, "uFlipX", flipX ? -1.0f : 1.0f);
		Uniforms::SetVec4(*Material, "uTint", tint);

		// === billboard roll - owner Transform.EulerRot.z(degree) 만 송신 (pitch/yaw 무시) ===
		// 빌보드 셰이더는 uModel 회전을 버리므로, z축 roll 은 별도 uniform 으로 전달해야 반영된다.
		float rollDeg = 0.0f;
		if (auto *owner = GetOwner())
			rollDeg = owner->GetTransform().EulerRot[2];
		Uniforms::SetFloat(*Material, "uRoll", vmath::radians(rollDeg));

		// === 피격 (uEnableHit / uTime) ===
		Uniforms::SetInt(*Material, "uEnableHit", enableHit ? 1 : 0);
		Uniforms::SetFloat(*Material, "uTime", mEffectClock);

		// === 디졸브 (uEnableDissolve / uDissolve*) ===
		Uniforms::SetInt(*Material, "uEnableDissolve", enableDissolve ? 1 : 0);
		Uniforms::SetFloat(*Material, "uDissolveThreshold", dissolveThreshold);
		Uniforms::SetFloat(*Material, "uDissolveOutlineThickness", dissolveOutlineThickness);
		Uniforms::SetVec3(*Material, "uDissolveOutlineColor", dissolveOutlineColor);
		if (dissolveTex)
			Uniforms::SetTexture(*Material, "uDissolveTex", dissolveTex, /*unit=*/1); // uAtlas=unit0
	}
} // namespace SJH::Sprite
