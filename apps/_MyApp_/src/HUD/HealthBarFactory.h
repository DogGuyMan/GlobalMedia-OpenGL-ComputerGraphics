/**
 * @file HealthBarFactory.h
 * @brief 타깃 Actor 머리 위에 분절형 체력바 자식 Actor 를 조립/부착하는 팩토리.
 *
 * @details
 *  ### 책임
 *  - @c HealthBarConfig 외형/배치 설정 구조체 정의 (전부 @c Constants.h 기본값).
 *  - @c AttachHealthBar - 타깃 Actor 에 체력바 자식 Actor 1개를 만들어 붙임:
 *    공유 Program/Mesh 확보 + per-instance Material 설정 + MeshRenderer + @c HealthBarDriver 부착.
 *
 *  ### 비-책임
 *  - [X] 프레임별 HP 값 -> uFill 갱신 - @c HealthBarDriver 담당.
 *  - [X] 체력바 셰이더 작성 - resources/shaders/healthbar.vs/.fs.
 *
 *  ### 정통 매핑
 *  - Cocos2D Factory free 함수 - 조립된 노드 트리 일괄 부착.
 *
 * @note 외형(색/조각)은 부착 시 1회 Material uniform 으로 굳고, 동적인 채움 비율만
 *       @c HealthBarDriver 가 매 프레임 @c uFill 로 갱신한다.
 */
#ifndef __TOPDOWNSHOOTER_HUD_HEALTHBAR_FACTORY_H__
#define __TOPDOWNSHOOTER_HUD_HEALTHBAR_FACTORY_H__

#include <glm/glm.hpp>
#include "HUD/Constants.h"

namespace SJH::Scene
{
	class Actor;
} // namespace SJH::Scene

namespace TopdownShooter::HUD
{
	/**
	 * @brief 머리 위 분절형 체력바 외형/배치 설정 (전부 @c Constants.h 기본값 보유).
	 * @details 모든 멤버가 기본 초기자를 가지므로 @c {} 로 디폴트 구성 가능.
	 */
	struct HealthBarConfig
	{
		glm::vec4 fillColor      = HEALTHBAR_FILL_COLOR;      ///< 채워진 조각 색 (레퍼런스 녹색).
		glm::vec4 bgColor        = HEALTHBAR_BG_COLOR;        ///< 빈 조각 트랙 색.
		float       segmentCount   = HEALTHBAR_SEGMENT_COUNT;   ///< 바를 나누는 분절 조각 수.
		float       segmentSpacing = HEALTHBAR_SEGMENT_SPACING; ///< 조각 사이 간격.
		float       headOffset     = HEALTHBAR_HEAD_OFFSET;     ///< cameraUp 방향 머리 위 거리.
		glm::vec2 size           = HEALTHBAR_SIZE;            ///< 바 크기 (가로 x 세로).
	};

	/// @brief target 의 Life(ILivable) 에 묶인 체력바 자식 Actor 를 생성/부착.
	/// @details Program/Mesh 는 @c ResourceRegistry 공유 캐시에서 확보(없으면 생성), Material 은
	///          per-instance 로 고유 키 생성. 부착 후 매 프레임 갱신은 @c HealthBarDriver 가 담당.
	///          target 에 @c ILivable 이 없거나 셰이더/메시 확보 실패 시 아무 것도 하지 않음(silent no-op).
	/// @param target 체력바를 머리 위에 달 대상 Actor.
	/// @param cfg    외형/배치 설정 (생략 시 전부 기본값).
	void AttachHealthBar(SJH::Scene::Actor &target, const HealthBarConfig &cfg = {});
} // namespace TopdownShooter::HUD

#endif // __TOPDOWNSHOOTER_HUD_HEALTHBAR_FACTORY_H__
