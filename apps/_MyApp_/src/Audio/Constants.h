#ifndef _TOPDOWNSHOOTER_AUDIO_CONSTANTS__
#define _TOPDOWNSHOOTER_AUDIO_CONSTANTS__

namespace TopdownShooter::Audio
{
	// ── FMOD Studio 이벤트 경로 (event:/<name>) ──
	// getEvent 는 오타 시 조용히 nullptr(ERR_EVENT_NOTFOUND) 를 돌려주므로(FMODAPI.md §5/§11),
	// 분산된 문자열 리터럴 대신 단일 상수로 통일해 오타/표류를 차단한다.
	// 값은 .strings.bank 의 실제 이벤트명과 정확히 일치해야 한다(대소문자/공백 포함).
	constexpr const char *EVENT_BGM         = "event:/BGM";
	constexpr const char *EVENT_SLASH       = "event:/Slash";
	constexpr const char *EVENT_DAMAGED     = "event:/Damaged";
	constexpr const char *EVENT_ENEMY_DEATH = "event:/EnemyDeath";
	constexpr const char *EVENT_PICKUP      = "event:/Pickup";
	constexpr const char *EVENT_DASH      = "event:/Dash";
	constexpr const char *EVENT_SHOOT      = "event:/Shoot";

	// ── FMOD Studio Bus 경로 (bus:/<name>) — 그룹별 볼륨 제어 (Pause 볼륨 슬라이더) ──
	// 공백 포함 이름 그대로(FMODAPI.md §9). .strings.bank 의 실제 bus 명과 일치해야 함.
	constexpr const char *BUS_BGM = "bus:/BGM Bus";
	constexpr const char *BUS_SFX = "bus:/SFX Bus";
} // namespace TopdownShooter::Audio

#endif //_TOPDOWNSHOOTER_AUDIO_CONSTANTS__
