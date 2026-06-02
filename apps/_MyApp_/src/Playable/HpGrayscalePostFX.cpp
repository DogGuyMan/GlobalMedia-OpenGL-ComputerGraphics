#include "Playable/HpGrayscalePostFX.h"

#include "Playable/PostFXRegistry.h"
#include "material/material.h" // SJH::Material::Properties.Floats

#include <utility>

namespace TopdownShooter::Playable
{
	HpGrayscalePostFX::HpGrayscalePostFX(std::string passName, std::string uniformName)
	    : mPassName(std::move(passName)), mUniformName(std::move(uniformName))
	{
	}

	void HpGrayscalePostFX::OnEnter()
	{
		// 핫패스 sink 1회 해소 (ctor 금지 — GetOwner null + dynamic type=base). 없으면 매 프레임 no-op.
		if (GetOwner())
			mLife = GetOwner()->GetComponent<Entity::ILivable>();
	}

	void HpGrayscalePostFX::Update(float /*dt*/)
	{
		if (!mLife) return;
		const int maxHp = mLife->GetMaxHp();
		if (maxHp <= 0) return;

		// 체력 비율 [0,1] — 1.0=원색, 0.0=완전 무채색 (셰이더 uGrayscaleAmount 규약).
		float ratio = static_cast<float>(mLife->GetHp()) / static_cast<float>(maxHp);
		if (ratio < 0.0f) ratio = 0.0f;
		else if (ratio > 1.0f) ratio = 1.0f;

		if (auto *mat = PostFXRegistry::Get().Material(mPassName))
			mat->Properties.Floats[mUniformName] = ratio;
	}
}
