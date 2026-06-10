/**
 * @file Constants.h
 * @brief 연출(presentation) 레이어 상수 모음 - 엔티티 스프라이트 텍스처 테이블 + 방향 판정 임계 각도 + 연출 튜닝 값.
 *
 * @details
 *  ### 책임
 *  - 플레이어/적 스프라이트 PNG 경로 + 그리드(행/열) + 뒤집기(flip) + draw order 를 정적 테이블로 보유.
 *  - 방향(facing) 판정용 각도 임계 (@c FacingThresholdConfig) 와 걷기 애니 FPS 를 단일 소스로 제공.
 *  - hit-flash / dissolve / 비네팅 같은 연출 지속시간/강도 튜닝 상수.
 *
 *  ### 비-책임
 *  - 도메인 수치(이동속도/HP/데미지)는 다루지 않음 - presentation 전용 (도메인은 별개 Config 레이어).
 *
 * @note 도메인 @c PlayerActorConfig::SpriteCfg::fps 와 본 헤더의 @c PLAYER_ANIM_FPS 는 서로
 *       교차 참조하지 않는다 (레이어 분리 - presentation 단일 소스 원칙).
 */
#ifndef _TOPDOWNSHOOTER_PLAYABLE_CONSTANTS__
#define _TOPDOWNSHOOTER_PLAYABLE_CONSTANTS__

#include "vector"
#include <vmath.h>

namespace TopdownShooter::Playable
{
	/**
	 * @brief 스프라이트 레이어 1장의 텍스처 설정 (경로 + 그리드 + 뒤집기 + draw order).
	 * @details @c AttachSpriteLayer 가 이 PoD 를 받아 atlas find-or-create + SpriteRenderer 부착에 사용.
	 *          @c ColCount > 1 이면 가로 N프레임 스트립으로 간주해 SpriteSequencePlayable(걷기 애니) 부착.
	 */
	struct EntityTextureConfig
	{
		const int DrawOrder = -1;          ///< painter 합성 층 (낮을수록 먼저 = 뒤). SpriteRenderer.QueueOffset 으로 전달.
		const char *TexturePath = nullptr; ///< PNG 경로 - atlas 캐시 키로도 사용 (같은 경로는 atlas 공유).
		const int RowCount = -1;           ///< 아틀라스 그리드 행 수.
		const int ColCount = -1;           ///< 아틀라스 그리드 열 수. 1보다 크면 가로 스트립 애니 파트로 간주.
		const bool Flip = false;           ///< 좌우 뒤집기 (Left 텍스처를 재사용해 Right 를 만들 때 true).
	};

	/// @brief 플레이어 방향 스프라이트 걷기 애니(ColCount>1) 초당 프레임 - presentation 단일 소스.
	/// @note 도메인 @c PlayerActorConfig::SpriteCfg::fps 는 별개 레이어의 기본값 - 레이어 분리상 교차 참조하지 않는다.
	constexpr float PLAYER_ANIM_FPS = 8.0f;

	/**
	 * @brief 방향(facing) 판정용 각도 구간 테이블.
	 * @details
	 *  단위 = degree, 범위 [0,360), 0 도 = 오른쪽(+X), 반시계 방향이 +.
	 *  각 @c vmath::vec2 = {start, end} 의 반열린 구간 [start, end).
	 *  start > end 면 0 도를 가로질러 wrap (Right 가 그 경우).
	 *  네 구간이 360 도를 빈틈없이 덮어야 판정이 결정적.
	 *  화면 매핑: Back = 화면 위(Up, 약 90 도), Front = 화면 아래(Down, 약 270 도).
	 */
	struct FacingThresholdConfig
	{
		vmath::vec2 Back;  ///< Up   (약 90 도)
		vmath::vec2 Front; ///< Down (약 270 도)
		vmath::vec2 Left;  ///< 약 180 도
		vmath::vec2 Right; ///< 약 0 도 (0 도를 가로질러 wrap)
	};
	/// @brief 플레이어 방향 판정 구간 인스턴스 (Up/Down/Left/Right 가 360 도를 분할).
	const FacingThresholdConfig PLAYER_FACING_THRESHOLD = {
	    {45.0f, 135.0f},   // Back  (Up)
	    {225.0f, 315.0f},  // Front (Down)
	    {135.0f, 225.0f},  // Left
	    {315.0f, 45.0f},   // Right (0 도 wrap)
	};

	/// @brief 조준 손(에임 피벗에 부착) 단일 스프라이트 - 방향과 무관하게 공용.
	const EntityTextureConfig PLAYER_HAND_PART = {
	    0, "./resources/texture/player/HAND_PART.png", 1, 1, false};
	/// @brief 플레이어 Back(위) 방향 Idle 4-레이어 (몸/장비 합성 - DrawOrder 로 층 정렬).
	const std::vector<EntityTextureConfig> PLAYER_BACK_IDLE = {
	    {-0, "./resources/texture/player/BACK_IDLE_H_0.png", 1, 1, false},
	    {-1, "./resources/texture/player/BACK_IDLE_E_1.png", 1, 1, false},
	    {-2, "./resources/texture/player/BACK_IDLE_B_2.png", 1, 1, false},
	    {-3, "./resources/texture/player/BACK_IDLE_F_3.png", 1, 1, false},
	};

	/// @brief 플레이어 Front(아래) 방향 Idle 4-레이어.
	const std::vector<EntityTextureConfig> PLAYER_FRONT_IDLE = {
	    {-0, "./resources/texture/player/FRONT_IDLE_E_0.png", 1, 1, false},
	    {-1, "./resources/texture/player/FRONT_IDLE_H_1.png", 1, 1, false},
	    {-2, "./resources/texture/player/FRONT_IDLE_B_2.png", 1, 1, false},
	    {-3, "./resources/texture/player/FRONT_IDLE_F_3.png", 1, 1, false},
	};

	/// @brief 플레이어 Left(왼쪽) 방향 Idle 4-레이어.
	const std::vector<EntityTextureConfig> PLAYER_LEFT_IDLE = {
	    {-0, "./resources/texture/player/LEFT_IDLE_E_0.png", 1, 1, false},
	    {-1, "./resources/texture/player/LEFT_IDLE_H_1.png", 1, 1, false},
	    {-2, "./resources/texture/player/LEFT_IDLE_B_2.png", 1, 1, false},
	    {-3, "./resources/texture/player/LEFT_IDLE_F_3.png", 1, 1, false},
	};

	/// @brief 플레이어 Right(오른쪽) 방향 Idle 4-레이어 - Left 텍스처를 flip=true 로 재사용.
	const std::vector<EntityTextureConfig> PLAYER_RIGHT_IDLE = {
	    {-0, "./resources/texture/player/LEFT_IDLE_E_0.png", 1, 1, true},
	    {-1, "./resources/texture/player/LEFT_IDLE_H_1.png", 1, 1, true},
	    {-2, "./resources/texture/player/LEFT_IDLE_B_2.png", 1, 1, true},
	    {-3, "./resources/texture/player/LEFT_IDLE_F_3.png", 1, 1, true},
	};

	/// @brief 플레이어 Back(위) 방향 걷기 4-레이어 - ColCount=2 인 레이어가 가로 2프레임 애니.
	const std::vector<EntityTextureConfig> PLAYER_BACK_MOVE = {
	    {-0, "./resources/texture/player/BACK_MOVE_B_0.png", 1, 2},
	    {-1, "./resources/texture/player/BACK_MOVE_H_1.png", 1, 1},
	    {-2, "./resources/texture/player/BACK_MOVE_E_2.png", 1, 1},
	    {-3, "./resources/texture/player/BACK_MOVE_F_3.png", 1, 1},
	};

	/// @brief 플레이어 Front(아래) 방향 걷기 4-레이어 - ColCount=2 인 레이어가 가로 2프레임 애니.
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

	// ── 연출 지속시간 / PostFX 튜닝 (분해 Task6) ──
	constexpr float SPRITE_HIT_FLASH_DURATION = 0.18f; // 피격 hit-flash 표시(초)
	constexpr float SPRITE_DISSOLVE_DURATION  = 1.0f;  // 사망 dissolve 표시 기본(초; Player/Enemy override)
	constexpr float VIGNETTE_PEAK             = 0.45f; // 피격 비네팅 시작 강도(0.45->0)
	constexpr int   VIGNETTE_DURATION_MS      = 300;   // 피격 비네팅 tween 길이(ms)
}; // namespace TopdownShooter::Playable

#endif //_TOPDOWNSHOOTER_PLAYABLE_CONSTANTS__
