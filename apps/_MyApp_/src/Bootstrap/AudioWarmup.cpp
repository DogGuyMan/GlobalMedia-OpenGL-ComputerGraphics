/**
 * @file AudioWarmup.cpp
 * @brief @c WarmupAudio 구현 - FMOD bank 로드 + BGM 시퀀스 부착.
 * @details bank 2종(strings + master)을 로드한 뒤 @c SequenceContext 에 audio/sceneRoot 를
 *          채워 @c Spawns::BuildBGM 에 위임 (BGM 재생 시작). reg/dir 은 싱글톤 @c ::Get() 조회.
 */
#include <GL/gl3w.h> // 반드시 최상단 — resource_registry.h 가 끄는 gl3w 보호 (다른 GL 헤더 선행 대비).

#include "Bootstrap/AudioWarmup.h"

#include "Audio/AudioSystem.h"
#include "Spawns/AmbientSequences.h"
#include "Spawns/SequenceContext.h"
#include "resource_registry/resource_registry.h"
#include "scene/scene.h"

namespace TopdownShooter::Bootstrap
{
	void WarmupAudio(TopdownShooter::Audio::AudioSystem &audio)
	{
		auto &reg = SJH::ResourceRegistry::Get();
		auto &dir = SJH::Scene::Director::Get();

		audio.LoadBank("resources/banks/Master.strings.bank");
		audio.LoadBank("resources/banks/Master.bank");

		// BGM — 인라인 -> Spawns::BuildBGM 이관 (M6 Task 5).
		TopdownShooter::Spawns::SequenceContext bgmCtx;
		bgmCtx.audio     = &audio;
		bgmCtx.sceneRoot = &dir.Root();
		TopdownShooter::Spawns::BuildBGM(bgmCtx);
	}
}
