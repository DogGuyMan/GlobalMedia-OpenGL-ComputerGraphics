#ifndef __TOPDOWNSHOOTER_PLAYABLE_HP_GRAYSCALE_POSTFX_H__
#define __TOPDOWNSHOOTER_PLAYABLE_HP_GRAYSCALE_POSTFX_H__

#include "Entity/Components/Components.Interfaces.h" // ILivable (HP 조회)
#include "scene/actor.h"                             // SJH::Scene::Component

#include <string>

namespace TopdownShooter::Playable
{
	/// @brief 소유 액터 Life(ILivable) 의 HP 비율(CurHp/MaxHp, [0,1])을 매 프레임 PostFX float uniform 에 기록.
	///        예: ("grayscale_vignetting","uGrayscaleAmount") — 1.0=원색, 0.0=무채색 -> 체력이 닳을수록
	///        화면이 *점진적으로* grayscale (사망=HP0 시 자동으로 완전 무채색).
	///        [C] 트리거가 아니라 연속 상태 바인딩이라 director Playable 이 아니라 일반 Component (AddComponent + scene tick).
	class HpGrayscalePostFX : public SJH::Scene::Component
	{
	  public:
		HpGrayscalePostFX(std::string passName, std::string uniformName);

		void OnEnter() override;          // ILivable sink 1회 캐시
		void OnExit() override {}
		void Update(float dt) override;   // uniform = clamp(CurHp/MaxHp, 0, 1)

	  private:
		Entity::ILivable *mLife = nullptr;
		std::string       mPassName;
		std::string       mUniformName;
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_HP_GRAYSCALE_POSTFX_H__
