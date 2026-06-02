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
		if (!mLife || !mMat) return;
		const int maxHp = mLife->GetMaxHp();
		if (maxHp <= 0) return;

		float ratio = static_cast<float>(mLife->GetHp()) / static_cast<float>(maxHp);
		if (ratio < 0.0f) ratio = 0.0f;
		else if (ratio > 1.0f) ratio = 1.0f;

		mMat->Properties.Floats["uFill"] = ratio;
	}
} // namespace TopdownShooter::HUD
