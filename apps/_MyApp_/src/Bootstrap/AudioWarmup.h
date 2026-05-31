#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_AUDIO_WARMUP_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_AUDIO_WARMUP_H__

namespace TopdownShooter::Audio
{
	class AudioSystem;
}

namespace TopdownShooter::Bootstrap
{
	/// @brief FMOD bank 로드 + BGM(Spawns::BuildBGM) + "shot" 사운드 등록.
	///        산출 포인터 없음 (main 이 보유할 핸들 0) → void. 기존 main.cpp WramupFMOD 와 동일.
	/// @param audio Manager::Get().Audio() — bank/event/sound 의 owner. reg/dir 은 ::Get().
	void WarmupAudio(TopdownShooter::Audio::AudioSystem &audio);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_AUDIO_WARMUP_H__
