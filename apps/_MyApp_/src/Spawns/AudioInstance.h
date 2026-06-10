/**
 * @file AudioInstance.h
 * @brief 단발(one-shot) SFX Actor 를 조립·스폰하는 팩토리 자유 함수.
 *
 * @details
 *  ### 책임
 *  - @c fxParent 밑에 "AudioInstance" Actor 를 생성하고 @c FmodStudioPlayable +
 *    @c AutoDespawnOnFinish 를 부착 후 즉시 @c Play() 호출.
 *  - 이벤트 종료 시 @c AutoDespawnOnFinish.mDone 이 true 로 바뀌어 다음 sweep 에서
 *    Actor 가 자동 파괴 -- 호출자가 수명을 직접 관리하지 않아도 된다.
 *  ### 비-책임
 *  - [X] FMOD EventInstance 생성/해제 직접 처리 -- @c FmodStudioPlayable 위임.
 *  - [X] Actor 트리 sweep(RemoveChild) -- 부모의 @c SweepFinishedChildren 담당.
 *  ### 정통 매핑
 *  - Cocos2D @c runAction(CallFunc) + @c removeFromParentAndCleanup -- 단발 연출 후 자동 정리.
 *  - Unity @c AudioSource.PlayOneShot / @c Object.Destroy(go, clip.length) 패턴.
 *
 * @note @p desc 가 nullptr 이면 조용히 no-op (LoadEvent 실패 안전).
 *       Studio 이벤트 전용 -- FMOD Core API @c createSound 는 사용하지 않는다 (결정 2026-05-31).
 */
#ifndef __TOPDOWNSHOOTER_SPAWNS_AUDIO_INSTANCE_H__
#define __TOPDOWNSHOOTER_SPAWNS_AUDIO_INSTANCE_H__

#include <optional>
#include <vmath.h>

// fwd
namespace SJH::Scene { class Actor; }
namespace FMOD::Studio { class EventDescription; }

namespace TopdownShooter::Spawns
{
    /// @brief 단발(one-shot) SFX 프리미티브 -- fxParent 밑에 "AudioInstance" Actor 를 스폰하고
    ///        @c FmodStudioPlayable (이벤트 재생) + @c AutoDespawnOnFinish (종료 감지) 를 부착 후 Play.
    /// @details
    ///  조립(composition) 패턴 -- 상속 아님. 기존 부품(@c FmodStudioPlayable + @c AutoDespawnOnFinish)을
    ///  한 Actor 에 묶는 팩토리. Actor 비상속 컨벤션 + 부품 재사용.
    ///
    ///  자동 파괴 흐름:
    ///    FmodStudioPlayable.finished_ -> AutoDespawnOnFinish.mDone = true
    ///    -> 다음 프레임 SweepFinishedChildren 가 Actor 파괴 (메모리 자동 관리).
    ///
    ///  Studio 전용 (결정 2026-05-31): 단발 SFX 는 FMOD Studio 이벤트로 통일.
    ///
    ///  공간(3D): @p pos 를 주면 @c set3DAttributes 로 panning/감쇠 적용 (listener = 카메라).
    ///            없으면 2D 재생.
    /// @param fxParent 스폰 Actor 를 붙일 부모 Actor (FxRoot 등).
    /// @param desc     @c AudioSystem::LoadEvent 로 얻은 EventDescription.
    ///                 nullptr 이면 no-op (LoadEvent 실패 시 안전).
    /// @param pos      3D 공간 재생 위치. @c std::nullopt 면 2D 재생.
    void SpawnAudioInstance(SJH::Scene::Actor& fxParent,
                            ::FMOD::Studio::EventDescription* desc,
                            std::optional<vmath::vec3> pos = std::nullopt);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_AUDIO_INSTANCE_H__
