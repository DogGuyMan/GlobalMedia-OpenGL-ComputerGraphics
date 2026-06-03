#ifndef _TOPDOWNSHOOTER_ENTITY_CONSTANTS__
#define _TOPDOWNSHOOTER_ENTITY_CONSTANTS__

namespace TopdownShooter::Entity
{
	// ── Player (PlayerActorConfig 기본값) ──
	constexpr int   PLAYER_HP             = 100;
	constexpr float PLAYER_MOVE_SPEED     = 3.0f; // C1: struct 기본(5.0)↔Builder(3.0) 불일치 -> 실값 3.0
	constexpr float PLAYER_LINEAR_DAMPING = 5.0f;
	constexpr int   PLAYER_WEAPON_DAMAGE  = 10;
	constexpr float PLAYER_SPRITE_FPS     = 8.0f; // C2: SpriteCfg::fps + PlayerBuilder kFps 통합

	// ── Hand (PlayerHands 배치) ──
	constexpr float HAND_SPREAD_DEG       = 25.0f; // forward(-Z) 기준 좌(+)/우(-) 벌림각
	constexpr float HAND_RADIUS           = 0.6f;  // forward 거리(player local)
	constexpr float HAND_Y_OFFSET         = 0.0f;
	constexpr float HAND_SCALE            = 0.25f; // 손 스프라이트 균등 Scale
	constexpr int   HAND_QUEUE_OFFSET     = 10;    // 몸통 레이어 위

	// ── Enemy (EnemyConfig/EnemyDeps 기본 + factory 물리) ──
	constexpr int   ENEMY_HP              = 30;
	constexpr float ENEMY_SPEED           = 2.0f;
	constexpr int   ENEMY_DAMAGE          = 10;
	constexpr float ENEMY_LINEAR_DAMPING  = 0.5f;
	constexpr float ENEMY_FRICTION        = 0.3f;
	constexpr float ENEMY_RADIUS          = 0.4f;
	constexpr float ENEMY_SPRITE_FPS      = 6.0f; // 2프레임 walk 애니 속도

	// ── Bullet (BulletConfig 기본 + 물리/시각 반지름) ──
	constexpr float BULLET_SPEED          = 15.0f;
	constexpr int   BULLET_DAMAGE         = 10;
	constexpr float BULLET_LIFETIME       = 3.0f;
	constexpr float BULLET_RADIUS         = 0.15f; // C5: 물리 CircleBody + 시각 Sphere mesh 동기
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_CONSTANTS__
