/**
 * @file sprite_component.cpp
 * @brief SpriteRenderer 구현 - 주입된 공유 자원 base 전달 + 매 프레임 uniform 동기화.
 *
 * @details
 *  ### 책임
 *  - 생성자: 주입된 plane Mesh / per-instance Material 을 base @c MeshRenderer 로 전달 (D8 DI).
 *  - @c SpriteRenderer::Update : atlas UV / tint / flipX / roll / 피격 / 디졸브 uniform 매 프레임 동기화.
 *
 *  ### 거주지 이동 (2026-06-11 E6 D8)
 *  과거 익명 ns 헬퍼 3종(plane/template/instance ResourceRegistry 해결)은
 *  @c resource_registry/sprite_resources.{h,cpp} 로 이주 - 본 TU 는 rr 의존 0.
 */
#include "sprite/sprite_component.h"

#include "material/material.h"
#include "material/material_uniforms.h"
#include "sprite/uniform_atlas.h"

namespace SJH::Sprite
{
	SpriteRenderer::SpriteRenderer(UniformAtlas *atlasPtr, SJH::Mesh *mesh, SJH::Material *materialInstance)
	    : MeshRenderer(mesh, materialInstance),
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
		Uniforms::SetFloat(*Material, "uRoll", glm::radians(rollDeg));

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
