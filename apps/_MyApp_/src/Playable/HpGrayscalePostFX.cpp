/**
 * @file HpGrayscalePostFX.cpp
 * @brief HpGrayscalePostFX Component 구현 -- HP 비율 -> PostFX float uniform 연속 바인딩.
 *
 * @details
 *  - @c OnEnter : 소유 Actor 에서 @c ILivable 인터페이스를 1회 캐시. GetOwner() null 가드 포함.
 *  - @c Update  : HP 비율 [0,1] 을 계산하고 @c ResourceRegistry 의 mat_pass_<name> 공유본을 조회해
 *                 @c Properties.Floats[mUniformName] 에 직접 기록한다.
 *                 maxHp<=0 이면 ZeroDivision 방지를 위해 조기 return.
 * @note Material uniform 기록은 GL draw 호출 전에 이루어진다 -- Properties.Floats 는
 *       draw 시점에 mesh_pass 가 UBO MaterialBlock 멤버(@c UpdateUniformMember)로 업로드한다.
 */
#include "Playable/HpGrayscalePostFX.h"

#include "resource_registry/resource_registry.h"
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
		// 핫패스 sink 1회 해소 (ctor 금지 - GetOwner null + dynamic type=base). 없으면 매 프레임 no-op.
		if (GetOwner())
			mLife = GetOwner()->GetComponent<Entity::ILivable>();
	}

	void HpGrayscalePostFX::Update(float /*dt*/)
	{
		if (!mLife) return;
		const int maxHp = mLife->GetMaxHp();
		if (maxHp <= 0) return;

		// 체력 비율 [0,1] - 1.0=원색, 0.0=완전 무채색 (셰이더 uGrayscaleAmount 규약).
		float ratio = static_cast<float>(mLife->GetHp()) / static_cast<float>(maxHp);
		if (ratio < 0.0f) ratio = 0.0f;
		else if (ratio > 1.0f) ratio = 1.0f;

		// rr 의 mat_pass_<name> 공유본 직접 조회 (D-7 PostFXRegistry 흡수).
		if (auto *mat = SJH::ResourceRegistry::Get().FindSharedMaterial("mat_pass_" + mPassName))
			mat->Properties.Floats[mUniformName] = ratio;
	}
}
