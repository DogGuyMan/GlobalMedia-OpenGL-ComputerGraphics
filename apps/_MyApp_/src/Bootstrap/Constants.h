#ifndef _TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__
#define _TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__

namespace TopdownShooter::Bootstrap
{
	// EntityPresentation 사망 연출 길이(초) — Player 기본 / Enemy override.
	constexpr float PLAYER_DISSOLVE_SECONDS = 1.5f;
	constexpr float PLAYER_DEATH_DELAY      = 1.5f;
	constexpr float ENEMY_DISSOLVE_SECONDS  = 0.6f;
	constexpr float ENEMY_DEATH_DELAY       = 0.6f;
	// 적 상시 트윈 길이(ms 편도).
	constexpr int   ENEMY_SCALE_PULSE_MS    = 400;
	constexpr int   ENEMY_ROT_WOBBLE_MS     = 2000;
	// 발밑 데칼 Y offset (바닥 z-fight 회피 + 엔티티별 높이 튜닝). 직접 조작용 하드코딩.
	constexpr float PLAYER_DECAL_Y       = 0.02f;
	constexpr float ENEMY_DECAL_Y        = 0.02f;
	constexpr float DECAL_CIRCLE_Y_DELTA = 0.01f; // 피격원이 그림자보다 약간 위
} // namespace TopdownShooter::Bootstrap

#endif //_TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__
