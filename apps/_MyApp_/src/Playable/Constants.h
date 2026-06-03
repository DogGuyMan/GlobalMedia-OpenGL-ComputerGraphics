#ifndef _TOPDOWNSHOOTER_PLAYABLE_CONSTANTS__
#define _TOPDOWNSHOOTER_PLAYABLE_CONSTANTS__

#include "vector"
#include <vmath.h>

namespace TopdownShooter::Playable
{
	struct EntityTextureConfig
	{
		const int DrawOrder = -1;
		const char *TexturePath = nullptr;
		const int RowCount = -1;
		const int ColCount = -1;
		const bool Flip = false;
	};

	// 플레이어 방향 스프라이트 걷기 애니(ColCount>1) 초당 프레임 — presentation 단일 소스.
	// (도메인 PlayerActorConfig::SpriteCfg::fps 는 별개 레이어의 기본값 — 레이어 분리상 교차 참조하지 않는다.)
	constexpr float PLAYER_ANIM_FPS = 8.0f;

	// 방향 판정 임계 각도 (degree, [0,360), 0°=오른쪽(+X), 반시계 +). 각 vec2 = {start, end};
	// start>end 면 0°를 가로질러 wrap (Right). 360° 를 빈틈없이 덮어야 결정적.
	// 매핑: Back=화면 위(Up, ~90°), Front=화면 아래(Down, ~270°). 순서 = Up,Down,Left,Right.
	struct FacingThresholdConfig
	{
		vmath::vec2 Back;  // Up   (~90°)
		vmath::vec2 Front; // Down (~270°)
		vmath::vec2 Left;  // (~180°)
		vmath::vec2 Right; // (~0°, wrap)
	};
	const FacingThresholdConfig PLAYER_FACING_THRESHOLD = {
	    {45.0f, 135.0f},   // Back  (Up)
	    {225.0f, 315.0f},  // Front (Down)
	    {135.0f, 225.0f},  // Left
	    {315.0f, 45.0f},   // Right (0° wrap)
	};

	const EntityTextureConfig PLAYER_HAND_PART = {
	    0, "./resources/texture/player/HAND_PART.png", 1, 1, false};
	const std::vector<EntityTextureConfig> PLAYER_BACK_IDLE = {
	    {-0, "./resources/texture/player/BACK_IDLE_H_0.png", 1, 1, false},
	    {-1, "./resources/texture/player/BACK_IDLE_E_1.png", 1, 1, false},
	    {-2, "./resources/texture/player/BACK_IDLE_B_2.png", 1, 1, false},
	    {-3, "./resources/texture/player/BACK_IDLE_F_3.png", 1, 1, false},
	};

	const std::vector<EntityTextureConfig> PLAYER_FRONT_IDLE = {
	    {-0, "./resources/texture/player/FRONT_IDLE_E_0.png", 1, 1, false},
	    {-1, "./resources/texture/player/FRONT_IDLE_H_1.png", 1, 1, false},
	    {-2, "./resources/texture/player/FRONT_IDLE_B_2.png", 1, 1, false},
	    {-3, "./resources/texture/player/FRONT_IDLE_F_3.png", 1, 1, false},
	};

	const std::vector<EntityTextureConfig> PLAYER_LEFT_IDLE = {
	    {-0, "./resources/texture/player/LEFT_IDLE_E_0.png", 1, 1, false},
	    {-1, "./resources/texture/player/LEFT_IDLE_H_1.png", 1, 1, false},
	    {-2, "./resources/texture/player/LEFT_IDLE_B_2.png", 1, 1, false},
	    {-3, "./resources/texture/player/LEFT_IDLE_F_3.png", 1, 1, false},
	};

	const std::vector<EntityTextureConfig> PLAYER_RIGHT_IDLE = {
	    {-0, "./resources/texture/player/LEFT_IDLE_E_0.png", 1, 1, true},
	    {-1, "./resources/texture/player/LEFT_IDLE_H_1.png", 1, 1, true},
	    {-2, "./resources/texture/player/LEFT_IDLE_B_2.png", 1, 1, true},
	    {-3, "./resources/texture/player/LEFT_IDLE_F_3.png", 1, 1, true},
	};

	const std::vector<EntityTextureConfig> PLAYER_BACK_MOVE = {
	    {-0, "./resources/texture/player/BACK_MOVE_B_0.png", 1, 2},
	    {-1, "./resources/texture/player/BACK_MOVE_H_1.png", 1, 1},
	    {-2, "./resources/texture/player/BACK_MOVE_E_2.png", 1, 1},
	    {-3, "./resources/texture/player/BACK_MOVE_F_3.png", 1, 1},
	};

	const std::vector<EntityTextureConfig> PLAYER_FRONT_MOVE = {
	    {-0, "./resources/texture/player/FRONT_MOVE_E_0.png", 1, 1},
	    {-1, "./resources/texture/player/FRONT_MOVE_H_1.png", 1, 1},
	    {-2, "./resources/texture/player/FRONT_MOVE_B_2.png", 1, 2},
	    {-3, "./resources/texture/player/FRONT_MOVE_F_3.png", 1, 1},
	};

	const std::vector<EntityTextureConfig> PLAYER_LEFT_MOVE = {
	    {-0, "./resources/texture/player/LEFT_MOVE_E_0.png", 1, 1},
	    {-1, "./resources/texture/player/LEFT_MOVE_H_1.png", 1, 1},
	    {-2, "./resources/texture/player/LEFT_MOVE_B_2.png", 1, 2},
	    {-3, "./resources/texture/player/LEFT_MOVE_F_3.png", 1, 1},
	};

	const std::vector<EntityTextureConfig> PLAYER_RIGHT_MOVE = {
	    {-0, "./resources/texture/player/LEFT_MOVE_E_0.png", 1, 1, true},
	    {-1, "./resources/texture/player/LEFT_MOVE_H_1.png", 1, 1, true},
	    {-2, "./resources/texture/player/LEFT_MOVE_B_2.png", 1, 2, true},
	    {-3, "./resources/texture/player/LEFT_MOVE_F_3.png", 1, 1, true},
	};

	const EntityTextureConfig ENEMY_FRONT[3] = {
	    {0, "./resources/texture/enemy/ENEMY1_FRONT.png", 1, 2, false},
	    {0, "./resources/texture/enemy/ENEMY2_FRONT.png", 1, 2, false},
	    {0, "./resources/texture/enemy/ENEMY3_FRONT.png", 1, 2, false},
	};
}; // namespace TopdownShooter::Playable

#endif //_TOPDOWNSHOOTER_PLAYABLE_CONSTANTS__
