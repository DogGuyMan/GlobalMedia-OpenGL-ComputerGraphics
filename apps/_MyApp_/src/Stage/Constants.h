/**
 * @file Constants.h
 * @brief Stage 도메인 전역 상수 - 웨이브 난이도 곡선 + 아레나 크기 파라미터.
 *
 * @details
 *  ### 책임
 *  - 웨이브 스폰 간격 / 최대 생존 수 / HP/속도 선형 증가 계수 / 접촉 데미지 상수화.
 *  - 아레나 반-크기(@c ARENA_HALF_EXTENT) + 벽 두께(@c WALL_THICKNESS) 기본값 제공.
 *    @c StageConfig 의 기본 인자가 이 값들을 참조해 단일화.
 *  ### 비-책임
 *  - [X] 런타임 조정 - 모두 @c constexpr, 런타임 변경 불가. 게임플레이 튜닝은 수치만 수정.
 *
 * @note 웨이브 HP/속도 공식:
 *  hp    = WAVE_HP_BASE    + wave * WAVE_HP_PER_WAVE
 *  speed = WAVE_SPEED_BASE + wave * WAVE_SPEED_PER_WAVE
 *  @c WaveController::SpawnEnemy 가 이 공식으로 파라미터를 도출.
 */
#ifndef _TOPDOWNSHOOTER_STAGE_CONSTANTS__
#define _TOPDOWNSHOOTER_STAGE_CONSTANTS__

namespace TopdownShooter::Stage
{
	// 웨이브 스폰/난이도 곡선.
	constexpr float WAVE_SPAWN_INTERVAL = 3.0f; ///< 적 스폰 간격(초) - @c SJH::Timer::Timer 주기로 사용.
	constexpr int   WAVE_MAX_ENEMIES    = 5;    ///< 동시 생존 최대 Enemy 수. 초과 시 스폰 대기.
	constexpr int   WAVE_HP_BASE        = 20;   ///< 웨이브 1 적 기본 HP. hp = BASE + wave * PER_WAVE.
	constexpr int   WAVE_HP_PER_WAVE    = 5;    ///< 웨이브당 HP 증가량.
	constexpr float WAVE_SPEED_BASE     = 1.5f; ///< 웨이브 1 적 기본 이동속도. speed = BASE + wave * PER_WAVE.
	constexpr float WAVE_SPEED_PER_WAVE = 0.3f; ///< 웨이브당 이동속도 증가량.
	constexpr int   WAVE_CONTACT_DAMAGE = 10;   ///< 접촉 데미지 - 전 웨이브 동일(일정).

	// 아레나/벽 - StageConfig 기본값 (main.cpp WaveController arenaHalfExtent 와 단일화).
	constexpr float ARENA_HALF_EXTENT = 10.0f; ///< 아레나 벽 안쪽 절반 크기(월드 단위). @c StageConfig 기본값.
	constexpr float WALL_THICKNESS    = 0.5f;  ///< 물리 벽 두께(반-크기). @c StageConfig 기본값.
} // namespace TopdownShooter::Stage

#endif //_TOPDOWNSHOOTER_STAGE_CONSTANTS__
