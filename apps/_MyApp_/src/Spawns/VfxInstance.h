/**
 * @file VfxInstance.h
 * @brief 단발(one-shot) Effekseer VFX Actor 를 조립/스폰하는 팩토리 + 전역 파사드.
 *
 * @details
 *  ### 책임
 *  - @c SpawnVfxInstance: @c fxParent 밑에 "VfxInstance" Actor 생성 +
 *    @c EffekseerPlayable(Static) + @c AutoDespawnOnFinish 부착 + 즉시 @c Play().
 *  - @c VFX::SetSpawnContext / @c VFX::Spawn: main 이 1회 컨텍스트(fxRoot + VFXSystem)를
 *    등록해 두면 컴포넌트가 fxRoot 를 직접 들지 않고 key 문자열만으로 VFX 를 호출 가능.
 *  - @c VFX::GetSpawnFxRoot / @c VFX::GetSpawnVfx: 단발 Spawn 으로 처리 불가한 합성 VFX
 *    (회전/지속 이펙트 등) 를 위한 컨텍스트 접근자.
 *  ### 비-책임
 *  - [X] EffekseerPlayable 생성 상세 (Manager/Effect 설정) -- @c EffekseerPlayable 위임.
 *  - [X] Actor 트리 sweep(RemoveChild) -- 부모의 @c SweepFinishedChildren 담당.
 *  ### 정통 매핑
 *  - Cocos2D @c ParticleSystem::runAction(Sequence(animate, removeFromParent)) 패턴.
 *  - Unity @c Object.Instantiate(vfxPrefab, pos) + @c Destroy(go, duration) 패턴.
 *
 * @note @c VfxFacade.cpp 는 별도 TU -- gl3w vs. EffekseerRendererGL 의 @c PFNGL* 재정의 충돌 방지.
 *       파사드는 @c VFXSystem* 을 포인터로만 다뤄 완전형(@c EffekseerRendererGL) 불필요.
 */
#ifndef __TOPDOWNSHOOTER_SPAWNS_VFX_INSTANCE_H__
#define __TOPDOWNSHOOTER_SPAWNS_VFX_INSTANCE_H__

#include <vmath.h>

// fwd
namespace SJH { class Effect; }
namespace SJH::Scene { class Actor; }
namespace TopdownShooter::VFX { class VFXSystem; }

namespace TopdownShooter::Spawns
{
    /// @brief 단발(one-shot) VFX 프리미티브 -- @c AudioInstance 의 Effekseer 대칭판.
    /// @details
    ///  @p fxParent 밑에 "VfxInstance" Actor 를 스폰하고 @c EffekseerPlayable(Static) +
    ///  @c AutoDespawnOnFinish 를 부착 후 즉시 @c Play().
    ///
    ///  자동 파괴 흐름:
    ///    EffekseerPlayable.finished_ -> AutoDespawnOnFinish.mDone = true
    ///    -> 다음 프레임 SweepFinishedChildren 가 Actor 파괴 (메모리 자동 관리).
    ///
    ///  조립(composition) 패턴 -- 상속 아님. Actor 비상속 +
    ///  기존 부품(@c EffekseerPlayable + @c AutoDespawnOnFinish) 재사용.
    /// @param fxParent 스폰 Actor 를 붙일 부모 Actor (FxRoot 등).
    /// @param vfx      @c VFXSystem 포인터. nullptr 이면 no-op.
    /// @param effect   @c ResourceRegistry::FindEffect 결과. nullptr 이면 no-op.
    /// @param pos      이펙트 월드 스폰 위치.
    /// @param yaw      Y 축 회전(라디안). 기본 0 (회전 없음).
    void SpawnVfxInstance(SJH::Scene::Actor& fxParent, VFX::VFXSystem* vfx,
                          SJH::Effect* effect, const vmath::vec3& pos, float yaw = 0.0f);
}

namespace TopdownShooter::VFX
{
    /// @brief VFX 단발 스폰 파사드 -- main 이 컨텍스트(fxRoot + VFXSystem)를 1회 등록한 뒤 key 로 호출.
    /// @details
    ///  컴포넌트는 VFX 를 직접 모르고, 빌더가 @c Spawn 람다를 seam 으로 주입한다
    ///  (@c PlayableDirector::Play 패턴).
    ///  내부에서 @c ResourceRegistry::Get().FindEffect(key) -> @c Spawns::SpawnVfxInstance 로 위임.
    ///  미등록(컨텍스트 nullptr) 또는 key 미존재면 조용히 no-op.

    /// @brief 전역 VFX 스폰 컨텍스트 등록 (main startup 1회 호출).
    /// @param fxRoot 스폰 Actor 를 붙일 루트 Actor (Director.Root() 자식 "FxRoot").
    /// @param vfx    @c VFXSystem 포인터.
    void SetSpawnContext(SJH::Scene::Actor* fxRoot, VFXSystem* vfx);

    /// @brief key 에 해당하는 Effekseer 이펙트를 @p pos 에 단발 스폰.
    /// @param key @c ResourceRegistry 에 등록된 Effect 키.
    /// @param pos 월드 스폰 위치.
    /// @param yaw Y 축 회전(라디안) -- 발사 방향 등. 기본 0 (회전 없음).
    void Spawn(const char* key, const vmath::vec3& pos, float yaw = 0.0f);

    /// @brief 등록된 스폰 컨텍스트 fxRoot 접근자.
    ///        단발 @c Spawn 으로 처리 불가한 합성 VFX(회전/지속 이펙트, UltimateLaser 등) 용.
    /// @return 등록된 fxRoot 포인터. @c SetSpawnContext 미호출 시 nullptr.
    SJH::Scene::Actor* GetSpawnFxRoot();

    /// @brief 등록된 스폰 컨텍스트 VFXSystem 접근자.
    /// @return 등록된 @c VFXSystem 포인터. @c SetSpawnContext 미호출 시 nullptr.
    VFXSystem*         GetSpawnVfx();
}

#endif // __TOPDOWNSHOOTER_SPAWNS_VFX_INSTANCE_H__
