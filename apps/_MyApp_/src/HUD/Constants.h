/**
 * @file Constants.h
 * @brief 머리 위 분절형 체력바(HUD)의 외형/배치 기본값 모음.
 *
 * @details
 *  ### 책임
 *  - 체력바 색상/조각 수/간격/머리 위 거리/크기의 *프로젝트 단일 기본값* 제공.
 *  - @c HealthBarConfig (HealthBarFactory.h) 의 멤버 디폴트 초기자로 소비됨.
 *  - Bootstrap 단계 EntityPresentation 의 healthBarColor 기본도 @c HEALTHBAR_FILL_COLOR 를 참조 (결정 C3).
 *
 *  ### 비-책임
 *  - [X] 런타임 상태 보유 - 전부 컴파일 타임 상수.
 *  - [X] HP 값 -> 시각 변환 로직 - @c HealthBarDriver 담당.
 *
 * @note 색상은 알파를 포함한 vec4 (RGBA), 배경은 반투명 검정.
 */
#ifndef _TOPDOWNSHOOTER_HUD_CONSTANTS__
#define _TOPDOWNSHOOTER_HUD_CONSTANTS__

#include <glm/glm.hpp>

namespace TopdownShooter::HUD
{
	// 머리 위 분절형 체력바 외형/배치. (Bootstrap EntityPresentation 의 healthBarColor 기본도 FILL 참조 - C3.)
	const     glm::vec4 HEALTHBAR_FILL_COLOR      = glm::vec4(0.13f, 1.0f, 0.0f, 1.0f); ///< 채워진 조각 색 (레퍼런스 녹색, RGBA).
	const     glm::vec4 HEALTHBAR_BG_COLOR        = glm::vec4(0.0f, 0.0f, 0.0f, 0.55f); ///< 빈 트랙 색 (반투명 검정, RGBA).
	constexpr float       HEALTHBAR_SEGMENT_COUNT   = 5.0f;                                 ///< 바를 나누는 분절 조각 수.
	constexpr float       HEALTHBAR_SEGMENT_SPACING = 0.08f;                                ///< 조각 사이 간격 (UV 비율).
	constexpr float       HEALTHBAR_HEAD_OFFSET     = 0.5f;                                 ///< cameraUp 방향으로 머리 위에 띄우는 거리.
	const     glm::vec2 HEALTHBAR_SIZE            = glm::vec2(1.2f, 0.18f);             ///< 바 크기 (가로 x 세로).
} // namespace TopdownShooter::HUD

#endif //_TOPDOWNSHOOTER_HUD_CONSTANTS__
