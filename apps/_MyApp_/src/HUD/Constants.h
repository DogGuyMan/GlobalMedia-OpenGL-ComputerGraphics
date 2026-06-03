#ifndef _TOPDOWNSHOOTER_HUD_CONSTANTS__
#define _TOPDOWNSHOOTER_HUD_CONSTANTS__

#include <vmath.h>

namespace TopdownShooter::HUD
{
	// 머리 위 분절형 체력바 외형/배치. (Bootstrap EntityPresentation 의 healthBarColor 기본도 FILL 참조 — C3.)
	const     vmath::vec4 HEALTHBAR_FILL_COLOR      = vmath::vec4(0.13f, 1.0f, 0.0f, 1.0f); // 채움(레퍼런스 녹색)
	const     vmath::vec4 HEALTHBAR_BG_COLOR        = vmath::vec4(0.0f, 0.0f, 0.0f, 0.55f); // 빈 트랙
	constexpr float       HEALTHBAR_SEGMENT_COUNT   = 5.0f;
	constexpr float       HEALTHBAR_SEGMENT_SPACING = 0.08f;
	constexpr float       HEALTHBAR_HEAD_OFFSET     = 0.5f;                     // cameraUp 머리 위 거리
	const     vmath::vec2 HEALTHBAR_SIZE            = vmath::vec2(1.2f, 0.18f); // 가로×세로
} // namespace TopdownShooter::HUD

#endif //_TOPDOWNSHOOTER_HUD_CONSTANTS__
