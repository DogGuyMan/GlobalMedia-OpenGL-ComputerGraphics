#ifndef _TOPDOWNSHOOTER_PHYSICS_CONSTANTS__
#define _TOPDOWNSHOOTER_PHYSICS_CONSTANTS__

namespace TopdownShooter::Physics
{
	// Impulse(Player Dash / Enemy Knockback) 속도 버스트 튜닝.
	constexpr float IMPULSE_PLAYER_FORCE    = 15.0f; // 버스트 힘 (Stat DashForce base; 7.5->2.5 1/3 튜닝)
	constexpr float IMPULSE_ENEMY_FORCE    = 5.0f; // 버스트 힘 (Stat DashForce base; 7.5->2.5 1/3 튜닝)
	constexpr float IMPULSE_COOLDOWN = 3.0f; // 재발동 쿨다운 (초)
	constexpr float IMPULSE_DURATION = 0.3f; // active 버스트 창 (초)
	constexpr float IMPULSE_LENGTH_EPS = 0.001f;
} // namespace TopdownShooter::Physics

#endif //_TOPDOWNSHOOTER_PHYSICS_CONSTANTS__
