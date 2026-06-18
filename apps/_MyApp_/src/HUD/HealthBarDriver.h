/**
 * @file HealthBarDriver.h
 * @brief 타깃 액터의 HP 값을 읽어 체력바 Material 의 채움 비율 uniform 으로 구동하는 Component.
 *
 * @details
 *  ### 책임
 *  - 매 프레임 타깃 Life(@c ILivable) 의 HP 비율(CurHp/MaxHp)을 계산해
 *    체력바 Material 의 @c uFill 셰이더 uniform 에 기록.
 *  - 즉 *HP 값 변화 -> 시각 갱신* 의 데이터 흐름을 담당하는 단방향 어댑터.
 *
 *  ### 비-책임
 *  - [X] 체력바 Actor/Material 생성 - @c HealthBarFactory 담당.
 *  - [X] HP 값 변경 - 본 Component 는 읽기 전용 관찰자.
 *  - [X] 색/조각 외형 결정 - Material uniform (@c HealthBarFactory 가 1회 설정).
 *
 *  ### 정통 매핑
 *  - Unity HealthBar UI 의 fillAmount 바인딩 - 모델 값을 뷰 파라미터로 매 프레임 동기화.
 *
 * @note 참조(Life/Material)는 모두 비소유. 생성자 주입으로 들어오며 유효 시점은 팩토리가 보장.
 */
#ifndef __TOPDOWNSHOOTER_HUD_HEALTHBAR_DRIVER_H__
#define __TOPDOWNSHOOTER_HUD_HEALTHBAR_DRIVER_H__

#include "Contracts/EntityContracts.h" // TopdownShooter::Entity::ILivable
#include "scene/actor.h"                             // SJH::Scene::Component

namespace SJH
{
	class Material;
} // namespace SJH

namespace TopdownShooter::HUD
{
	/**
	 * @brief 타깃 액터 Life 의 HP 비율을 체력바 Material 의 @c uFill 에 매 프레임 기록하는 Component.
	 * @details
	 *  HpGrayscalePostFX 와 같은 *모델 값 -> uniform* 패턴. 참조(Life/Material)는 생성자 주입으로 받으며
	 *  팩토리(@c AttachHealthBar)가 두 참조의 유효 시점을 보장한다.
	 *  계산은 @c GetHp() / @c GetMaxHp() 를 [0,1] 로 클램프한 비율.
	 */
	class HealthBarDriver : public SJH::Scene::Component
	{
	  public:
		/// @brief 관찰할 Life 와 갱신할 체력바 Material 을 주입.
		/// @param life     HP 값을 읽어올 대상 (비소유).
		/// @param material @c uFill uniform 을 기록할 per-instance Material (비소유).
		HealthBarDriver(Entity::ILivable *life, SJH::Material *material);

		void OnEnter() override {}
		void OnExit() override {}

		/// @brief HP 비율(CurHp/MaxHp 를 [0,1] 클램프)을 계산해 Material 의 @c uFill 에 기록.
		/// @param dt 프레임 델타(미사용 - 즉시 동기화).
		void Update(float dt) override;

	  private:
		Entity::ILivable *mLife = nullptr; ///< HP 값을 읽어올 대상 Life (비소유).
		SJH::Material    *mMat  = nullptr; ///< uFill 을 기록할 체력바 Material (비소유, per-instance).
	};
} // namespace TopdownShooter::HUD

#endif // __TOPDOWNSHOOTER_HUD_HEALTHBAR_DRIVER_H__
