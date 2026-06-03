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
} // namespace TopdownShooter::Bootstrap

#endif //_TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__
