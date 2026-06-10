/**
 * @file Constants.h
 * @brief FMOD Studio 이벤트/bus 경로 문자열 단일 상수 모음 - 오타/표류 차단.
 *
 * @details
 *  @c getEvent / @c getBus 는 경로 오타나 @c .strings.bank 미로드 시 조용히
 *  nullptr(@c ERR_EVENT_NOTFOUND) 를 돌려주므로 (FMODAPI.md §5/§11), 분산된 문자열 리터럴 대신
 *  컴파일 타임 상수 한 곳으로 통일해 컴파일러가 오타를 막도록 한다.
 * @note 값은 @c .strings.bank 의 실제 이벤트/bus 명과 정확히 일치해야 한다 (대소문자/공백 포함).
 */
#ifndef _TOPDOWNSHOOTER_AUDIO_CONSTANTS__
#define _TOPDOWNSHOOTER_AUDIO_CONSTANTS__

namespace TopdownShooter::Audio
{
	// ── FMOD Studio 이벤트 경로 (event:/<name>) ──
	// getEvent 는 오타 시 조용히 nullptr(ERR_EVENT_NOTFOUND) 를 돌려주므로(FMODAPI.md §5/§11),
	// 분산된 문자열 리터럴 대신 단일 상수로 통일해 오타/표류를 차단한다.
	// 값은 .strings.bank 의 실제 이벤트명과 정확히 일치해야 한다(대소문자/공백 포함).
	constexpr const char *EVENT_BGM         = "event:/BGM";          ///< 배경음 (loop, Stage FSM BGM_STATE 전환 대상).
	constexpr const char *EVENT_SLASH       = "event:/Slash";        ///< 근접/발사 베기 효과음.
	constexpr const char *EVENT_DAMAGED     = "event:/Damaged";      ///< 플레이어 피격 효과음.
	constexpr const char *EVENT_ENEMY_DEATH = "event:/EnemyDeath";   ///< 적 사망 효과음.
	constexpr const char *EVENT_PICKUP      = "event:/Pickup";       ///< 아이템 획득 효과음.
	constexpr const char *EVENT_DASH      = "event:/Dash";           ///< 대시 효과음.
	constexpr const char *EVENT_SHOOT      = "event:/Shoot";         ///< 발사 효과음.

	// ── FMOD Studio Bus 경로 (bus:/<name>) — 그룹별 볼륨 제어 (Pause 볼륨 슬라이더) ──
	// 공백 포함 이름 그대로(FMODAPI.md §9). .strings.bank 의 실제 bus 명과 일치해야 함.
	constexpr const char *BUS_BGM = "bus:/BGM Bus";   ///< 배경음 믹서 bus (Pause 볼륨 슬라이더 대상).
	constexpr const char *BUS_SFX = "bus:/SFX Bus";   ///< 효과음 믹서 bus (Pause 볼륨 슬라이더 대상).
} // namespace TopdownShooter::Audio

#endif //_TOPDOWNSHOOTER_AUDIO_CONSTANTS__
