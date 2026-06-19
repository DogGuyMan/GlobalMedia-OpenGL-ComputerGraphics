/**
 * @file AudioWarmup.h
 * @brief 게임 시작 시 FMOD 오디오 자원을 선로드(warm-up)하는 부트스트랩 자유 함수.
 *
 * @details
 *  ### 책임
 *  - FMOD Studio bank 로드 (Master.strings + Master) 를 게임 본 루프 진입 전에 1회 수행.
 *  - BGM 시퀀스를 @c Spawns::BuildBGM 으로 씬 루트에 부착 (재생 시작).
 *
 *  ### 비-책임([X])
 *  - [X] 핸들 반환 - 산출 포인터 없음(@c void). 로드된 bank/event 는 @c AudioSystem 이 owner.
 *  - [X] event:/Shoot 등 개별 SFX 로드 - 발사음/피격음은 빌더가 @c LoadEvent 로 lazy 로드.
 *
 *  ### 정통 매핑
 *  - Unity 의 startup AudioManager.Init - 본 게임은 main 부트스트랩 시퀀스의 한 단계.
 *
 * @note 기존 main.cpp 의 @c WarmupFMOD 행위를 자유 함수로 분리한 것 (행위 동일).
 */
#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_AUDIO_WARMUP_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_AUDIO_WARMUP_H__

namespace TopdownShooter::Audio
{
	class AudioSystem;
}

namespace TopdownShooter::Bootstrap
{
	/// @brief FMOD bank 로드 + BGM(@c Spawns::BuildBGM) 부착.
	/// @details 산출 포인터 없음 (main 이 보유할 핸들 0) -> void. 기존 main.cpp WarmupFMOD 와 동일.
	///          Master.strings.bank + Master.bank 를 로드한 뒤 BGM 시퀀스를 씬 루트에 부착한다.
	/// @param audio GameSystems::Get().Audio() - bank/event/sound 의 owner. reg/dir 은 ::Get() 으로 내부 조회.
	void WarmupAudio(TopdownShooter::Audio::AudioSystem &audio);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_AUDIO_WARMUP_H__
