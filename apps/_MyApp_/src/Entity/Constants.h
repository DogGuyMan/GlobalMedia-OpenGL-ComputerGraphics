#ifndef _TOPDOWNSHOOTER_ENTITY_CONSTANTS__
#define _TOPDOWNSHOOTER_ENTITY_CONSTANTS__

namespace TopdownShooter::Entity
{
	// ── Player (PlayerActorConfig 기본값) ──
	constexpr int   PLAYER_HP             = 100;
	constexpr float PLAYER_MOVE_SPEED     = 3.0f; // C1: struct 기본(5.0)↔Builder(3.0) 불일치 -> 실값 3.0
	constexpr float PLAYER_LINEAR_DAMPING = 5.0f;
	constexpr int   PLAYER_WEAPON_DAMAGE  = 5;
	constexpr float PLAYER_SPRITE_FPS     = 8.0f; // C2: SpriteCfg::fps + PlayerBuilder kFps 통합

	// ── Hand (PlayerHands 배치) ──
	constexpr float HAND_SPREAD_DEG       = 25.0f; // 초기 벌림각(첫 프레임). 이후 거리 보간이 매 프레임 덮어씀
	constexpr float HAND_RADIUS           = 0.3f;  // forward 거리(player local) — 팔 길이 (절반으로 축소)
	constexpr float HAND_Y_OFFSET         = -0.1f;
	constexpr float HAND_SCALE            = 0.25f; // 손 스프라이트 균등 Scale
	constexpr int   HAND_QUEUE_OFFSET     = 10;    // 몸통 레이어 위
	constexpr int   HAND_FIRE_PINCH_MS    = 500;   // 발사 핀치 복귀 시간(ms) — 클릭 시 15°로 좁혔다 원래로

	// 손 spread 화면 거리 보간 — v_c(중심→커서)↔각 손 half-angle 을 *화면(NDC) 거리* t∈[0,1] 로 보간.
	//   t=0 (커서가 플레이어 화면위치 위) → MAX(90°): 양팔이 서로 180° (활짝)
	//   t=1 (커서 화면 가장자리)        → MIN(7.5°): 양팔이 서로 15° (좁게)
	//   t = PlayerController::GetAimScreenT() (화면 가장자리에서 포화).
	constexpr float HAND_HALF_ANGLE_MIN   = 7.5f;  // 화면 가장자리 half-angle (deg)
	constexpr float HAND_HALF_ANGLE_MAX   = 90.0f; // 화면 중심 half-angle (deg)

	// ── Enemy (EnemyConfig/EnemyDeps 기본 + factory 물리) ──
	constexpr int   ENEMY_HP              = 30;
	constexpr float ENEMY_SPEED           = 2.0f;
	constexpr int   ENEMY_DAMAGE          = 10;
	constexpr float ENEMY_LINEAR_DAMPING  = 0.5f;
	constexpr float ENEMY_FRICTION        = 0.3f;
	constexpr float ENEMY_RADIUS          = 0.4f;
	constexpr float ENEMY_SPRITE_FPS      = 6.0f; // 2프레임 walk 애니 속도

	// ── Bullet (BulletConfig 기본 + 물리/시각 반지름) ──
	constexpr float BULLET_SPEED          = 30.0f;
	constexpr int   BULLET_DAMAGE         = 10;
	constexpr float BULLET_LIFETIME       = 3.0f;
	constexpr float BULLET_RADIUS         = 0.15f; // C5: 물리 CircleBody + 시각 Sphere mesh 동기

	// ── Ultimate (R키 — 플레이어 중심 회전 히트스캔 레이저) ──
	constexpr int   ULTIMATE_DAMAGE       = 50;    // 적별 틱당 데미지 (Enemy HP=30 → 1틱 살살)
	constexpr float ULTIMATE_DURATION     = 3.0f;  // 유지 시간(초) — 이후 자동 파괴
	constexpr float ULTIMATE_ROT_PER_SEC  = 6.283185307179586f; // 2π rad/s = 1초당 1회전 (3초간 3회전)
	constexpr float ULTIMATE_RANGE        = 14.15f; // 아레나 끝 = √2 × Stage::ARENA_HALF_EXTENT(10) ≈ 14.14
	constexpr float ULTIMATE_TICK         = 0.2f;  // 적별 데미지 재적용 간격(초) — 빔에 머무는 동안 DoT
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_CONSTANTS__
