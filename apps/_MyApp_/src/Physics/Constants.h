/**
 * @file Constants.h
 * @brief Impulse(Player Dash / Enemy Knockback) 속도 버스트 튜닝 상수 모음.
 *
 * @details
 *  Impulse(PhysicsImpulse.h) 가 참조하는 튜닝 값 집합. 힘은 b2Body::SetLinearVelocity 의
 *  목표 속도(m/s)로 직접 들어가고, 쿨다운/지속시간은 SJH::Timer 의 baseTime 으로 사용된다.
 *
 * @note 힘 값은 force 라는 이름이지만 실제로는 SetLinearVelocity 의 속도 크기로 쓰인다 (impulse 가 아닌 즉발 속도 세팅).
 */
#ifndef _TOPDOWNSHOOTER_PHYSICS_CONSTANTS__
#define _TOPDOWNSHOOTER_PHYSICS_CONSTANTS__

namespace TopdownShooter::Physics
{
	// Impulse(Player Dash / Enemy Knockback) 속도 버스트 튜닝.
	constexpr float IMPULSE_PLAYER_FORCE    = 15.0f; ///< Player Dash 버스트 속도 크기 (Stat DashForce base; 7.5->2.5 1/3 튜닝).
	constexpr float IMPULSE_ENEMY_FORCE    = 5.0f; ///< Enemy Knockback 버스트 속도 크기 (Stat DashForce base; 7.5->2.5 1/3 튜닝).
	constexpr float IMPULSE_COOLDOWN = 3.0f; ///< 재발동 쿨다운 (초). cooldownTimer baseTime 으로 사용.
	constexpr float IMPULSE_DURATION = 0.3f; ///< active 버스트 창 길이 (초). 이 동안 이동자가 자유이동을 skip.
	constexpr float IMPULSE_LENGTH_EPS = 0.001f; ///< 방향 벡터 길이 하한 - 이보다 짧으면 무방향으로 보고 발동 취소.
} // namespace TopdownShooter::Physics

#endif //_TOPDOWNSHOOTER_PHYSICS_CONSTANTS__
