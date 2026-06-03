#ifndef __TOPDOWNSHOOTER_SPAWNS_AUDIO_INSTANCE_H__
#define __TOPDOWNSHOOTER_SPAWNS_AUDIO_INSTANCE_H__

#include <optional>
#include <vmath.h>

// fwd
namespace SJH::Scene { class Actor; }
namespace FMOD::Studio { class EventDescription; }

namespace TopdownShooter::Spawns
{
    /// @brief 단발(one-shot) SFX 프리미티브 — fxParent 밑에 "AudioInstance" Actor 를 스폰하고
    ///        FmodStudioPlayable(이벤트 재생) + AutoDespawnOnFinish(종료 감지) 를 부착 후 Play.
    /// @details
    ///   - **상속 아님 — 조립(composition)**: 기존 부품(FmodStudioPlayable + AutoDespawnOnFinish)을
    ///     한 Actor 에 묶는 팩토리. Actor 비상속 컨벤션 + 부품 재사용.
    ///   - **자동 파괴**: 이벤트가 끝나면 FmodStudioPlayable.finished_ -> AutoDespawnOnFinish.mDone ->
    ///     render 의 SweepFinishedChildren 가 Actor 파괴 (메모리 자동 관리).
    ///   - **Studio 전용** (결정 2026-05-31): 단발 SFX 는 FMOD Studio 이벤트로 통일.
    ///   - **공간(3D)**: @p pos 주면 set3DAttributes 로 panning/감쇠 (listener=카메라). 없으면 2D.
    /// @param desc  AudioSystem::LoadEvent 로 얻은 EventDescription. nullptr 이면 no-op (이벤트 미존재 안전).
    void SpawnAudioInstance(SJH::Scene::Actor& fxParent,
                            ::FMOD::Studio::EventDescription* desc,
                            std::optional<vmath::vec3> pos = std::nullopt);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_AUDIO_INSTANCE_H__
