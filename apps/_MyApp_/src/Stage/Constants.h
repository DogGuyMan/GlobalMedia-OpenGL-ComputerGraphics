#ifndef _TOPDOWNSHOOTER_STAGE_CONSTANTS__
#define _TOPDOWNSHOOTER_STAGE_CONSTANTS__

namespace TopdownShooter::Stage
{
	// 웨이브 스폰/난이도 곡선.
	constexpr float WAVE_SPAWN_INTERVAL = 3.0f; // 적 스폰 간격(초)
	constexpr int   WAVE_MAX_ENEMIES    = 5;    // 동시 생존 최대
	constexpr int   WAVE_HP_BASE        = 20;   // 적 HP = BASE + wave*PER_WAVE
	constexpr int   WAVE_HP_PER_WAVE    = 5;
	constexpr float WAVE_SPEED_BASE     = 1.5f; // 적 속도 = BASE + wave*PER_WAVE
	constexpr float WAVE_SPEED_PER_WAVE = 0.3f;
	constexpr int   WAVE_CONTACT_DAMAGE = 10;   // 접촉 데미지(전 웨이브 일정)
} // namespace TopdownShooter::Stage

#endif //_TOPDOWNSHOOTER_STAGE_CONSTANTS__
