#ifndef __TOPDOWNSHOOTER_HUD_HEALTHBAR_DRIVER_H__
#define __TOPDOWNSHOOTER_HUD_HEALTHBAR_DRIVER_H__

#include "Entity/Components/Components.Interfaces.h" // TopdownShooter::Entity::ILivable
#include "scene/actor.h"                             // SJH::Scene::Component

namespace SJH
{
	class Material;
} // namespace SJH

namespace TopdownShooter::HUD
{
	/// @brief 타깃 액터 Life(ILivable) 의 HP 비율(CurHp/MaxHp, [0,1])을 매 프레임 체력바 Material 의
	///        `uFill` 에 기록. HpGrayscalePostFX 패턴 — 참조는 ctor 주입(팩토리가 시점 보장).
	class HealthBarDriver : public SJH::Scene::Component
	{
	  public:
		HealthBarDriver(Entity::ILivable *life, SJH::Material *material);

		void OnEnter() override {}
		void OnExit() override {}
		void Update(float dt) override;

	  private:
		Entity::ILivable *mLife = nullptr; // 비소유
		SJH::Material    *mMat  = nullptr; // 비소유 (per-instance)
	};
} // namespace TopdownShooter::HUD

#endif // __TOPDOWNSHOOTER_HUD_HEALTHBAR_DRIVER_H__
