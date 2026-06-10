/**
 * @file HealthBarDriver.cpp
 * @brief HealthBarDriver 구현 - HP 비율을 체력바 Material 의 uFill uniform 으로 매 프레임 동기화.
 */
#include "HUD/HealthBarDriver.h"

#include "material/material.h" // SJH::Material::Properties.Floats

namespace TopdownShooter::HUD
{
	HealthBarDriver::HealthBarDriver(Entity::ILivable *life, SJH::Material *material)
	    : mLife(life), mMat(material)
	{
	}

	void HealthBarDriver::Update(float /*dt*/)
	{
		// 참조 둘 중 하나라도 없으면 갱신 불가 - 조용히 skip.
		if (!mLife || !mMat) return;
		const int maxHp = mLife->GetMaxHp();
		if (maxHp <= 0) return; // 0 나눗셈 가드.

		// HP 비율을 [0,1] 로 클램프해 채움 비율로 사용.
		float ratio = static_cast<float>(mLife->GetHp()) / static_cast<float>(maxHp);
		if (ratio < 0.0f) ratio = 0.0f;
		else if (ratio > 1.0f) ratio = 1.0f;

		// 셰이더의 uFill uniform 으로 송신 - 다음 프레임 렌더에서 바 채움에 반영.
		mMat->Properties.Floats["uFill"] = ratio;
	}
} // namespace TopdownShooter::HUD
