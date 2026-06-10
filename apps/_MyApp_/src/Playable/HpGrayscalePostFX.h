/**
 * @file HpGrayscalePostFX.h
 * @brief HP 비율을 PostFX grayscale uniform 에 실시간 바인딩하는 Component.
 *
 * @details
 *  ### 책임
 *  - 소유 Actor 의 @c ILivable (Life Component) 에서 HP 비율(CurHp/MaxHp, [0,1])을 매 프레임 읽어
 *    지정한 PostFX 패스 Material 의 float uniform 에 기록한다.
 *  - HP 가 낮아질수록 @c uGrayscaleAmount 가 0 에 가까워져 화면이 점진적으로 무채색으로 변한다.
 *    HP=0 이면 uniform=0.0 -> 완전 무채색.
 *  ### 비-책임
 *  - [X] 트위닝/애니메이션 시작 결정 -- 연속 상태 바인딩이므로 Director Playable 이 아니라 일반 Component.
 *  - [X] Material 소유 -- @c PostFXRegistry 를 통한 비소유 관찰자.
 *  - [X] PostFX 셰이더 컴파일 / 패스 생성 -- @c PostFXRegistry / PassComponent 담당.
 *  ### 정통 매핑
 *  - Cocos2D @c CustomCommand per-frame uniform blit.
 *  - Unity @c PostProcessing.Volume + Weight 실시간 갱신.
 * @note @c OnEnter 에서 @c ILivable sink 를 1회 캐시한다. ctor 시점 GetOwner() 가 null 이므로
 *       생성자에서 GetComponent 호출 금지.
 */
#ifndef __TOPDOWNSHOOTER_PLAYABLE_HP_GRAYSCALE_POSTFX_H__
#define __TOPDOWNSHOOTER_PLAYABLE_HP_GRAYSCALE_POSTFX_H__

#include "Entity/Components/Components.Interfaces.h" // ILivable (HP 조회)
#include "scene/actor.h"                             // SJH::Scene::Component

#include <string>

namespace TopdownShooter::Playable
{
	/**
	 * @brief HP 비율을 PostFX float uniform 에 연속 기록하는 Component.
	 * @details
	 *  매 프레임 @c ILivable::GetHp() / @c ILivable::GetMaxHp() 로 비율을 계산하고
	 *  @c PostFXRegistry::Get().Material(passName) 으로 Material 을 조회해
	 *  @c Properties.Floats[uniformName] 에 기록한다.
	 *
	 *  - 트리거 방식이 아닌 연속 바인딩 -> Director Playable 이 아닌 일반 Component (AddComponent + scene tick).
	 *  - 예시: ("grayscale_vignetting", "uGrayscaleAmount") -> 1.0=원색, 0.0=완전 무채색.
	 */
	class HpGrayscalePostFX : public SJH::Scene::Component
	{
	  public:
		/// @brief 감시할 PostFX 패스 이름과 uniform 명을 지정해 생성.
		/// @param passName    @c PostFXRegistry 키 (예: "grayscale_vignetting").
		/// @param uniformName float uniform 명 (예: "uGrayscaleAmount").
		HpGrayscalePostFX(std::string passName, std::string uniformName);

		/// @brief 소유 Actor 에서 @c ILivable sink 를 1회 캐시.
		/// @note ctor 시점 GetOwner() 가 null 이므로 OnEnter 에서 수행한다.
		void OnEnter() override;          // ILivable sink 1회 캐시
		void OnExit() override {}
		/// @brief HP 비율 [0,1] 을 계산해 PostFX Material uniform 에 기록.
		/// @param dt 프레임 델타 타임 (초). 비율 계산에는 미사용, 인터페이스 준수용.
		void Update(float dt) override;   // uniform = clamp(CurHp/MaxHp, 0, 1)

	  private:
		Entity::ILivable *mLife = nullptr;  ///< 소유 Actor 의 ILivable 캐시 (OnEnter 에서 1회 취득).
		std::string       mPassName;         ///< PostFXRegistry 검색 키 (예: "grayscale_vignetting").
		std::string       mUniformName;      ///< 기록 대상 float uniform 명 (예: "uGrayscaleAmount").
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_HP_GRAYSCALE_POSTFX_H__
