/**
 * @file Constants.h
 * @brief Bootstrap 빌더(Player/Enemy) 가 공유하는 연출/데칼 튜닝 상수.
 * @details EntityPresentation 사망 연출 타이밍 + 적 상시 트윈 길이 + 발밑 데칼 Y offset.
 *          빌더 코드에 흩어진 매직 넘버를 한곳에 모은 단일 소스 (직접 튜닝용 하드코딩).
 */
#ifndef _TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__
#define _TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__

namespace TopdownShooter::Bootstrap
{
	// EntityPresentation 사망 연출 길이(초) - Player 기본 / Enemy override.
	constexpr float PLAYER_DISSOLVE_SECONDS = 1.5f;  ///< Player "death" SpriteDissolve 디졸브 지속 시간(초).
	constexpr float PLAYER_DEATH_DELAY      = 1.5f;  ///< Player 사망 후 비활성 지연(초) = 디졸브 가시화 창.
	constexpr float ENEMY_DISSOLVE_SECONDS  = 0.6f;  ///< Enemy "death" SpriteDissolve 디졸브 지속 시간(초).
	constexpr float ENEMY_DEATH_DELAY       = 0.6f;  ///< Enemy 사망 후 비활성 지연(초) = 디졸브 가시화 창.
	// 적 상시 트윈 길이(ms 편도).
	constexpr int   ENEMY_SCALE_PULSE_MS    = 400;   ///< 적 Y 스케일 펄스 트윈 편도(ms). 왕복은 2배.
	constexpr int   ENEMY_ROT_WOBBLE_MS     = 2000;  ///< 적 z축 회전 워블 트윈 편도(ms). 왕복은 2배.
	// 발밑 데칼 Y offset (바닥 z-fight 회피 + 엔티티별 높이 튜닝). 직접 조작용 하드코딩.
	constexpr float PLAYER_DECAL_Y       = -0.5f;    ///< Player 발밑 그림자 데칼 Y 높이 (바닥 z-fight 회피).
	constexpr float ENEMY_DECAL_Y        = -0.5f;    ///< Enemy 발밑 그림자 데칼 Y 높이 (바닥 z-fight 회피).
	constexpr float DECAL_CIRCLE_Y_DELTA = 0.01f;    ///< 피격원이 그림자보다 약간 위에 오도록 더하는 Y 델타.
} // namespace TopdownShooter::Bootstrap

#endif //_TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__
