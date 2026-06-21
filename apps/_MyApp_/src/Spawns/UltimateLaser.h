/**
 * @file UltimateLaser.h
 * @brief 궁극기 회전 히트스캔 레이저 스폰 인터페이스.
 *
 * @details
 *  ### 책임
 *  - fxRoot 아래 "UltimateLaser" Actor 1개를 스폰하고, 아래 3 컴포넌트를 부착한다.
 *    (1) @c RotatingHitscanLaser  - 매 프레임 현재 각도로 @c RaycastAll(Enemy) 후 적별 0.2s 틱 DoDamaged (3초간).
 *    (2) @c EffekseerPlayable     - 회전 laser VFX (1초당 1회전, 3초 후 SetMaxDurationSec 강제 종료).
 *    (3) @c AutoDespawnOnFinish   - EffekseerPlayable finished 시 @c SweepFinishedChildren 이 파괴.
 *  ### 비-책임
 *  - [X] Actor 파괴 타이밍 - @c OneShotSweeper::SweepFinishedChildren 이 담당.
 *  - [X] fxRoot / VFXSystem 등록 - @c VFX::SetSpawnContext 호출 측(Builder) 이 사전 등록.
 *  - [X] "laser" effect 리소스 로드 - @c ResourceRegistry::FindEffect 가 담당.
 *  ### 정통 매핑
 *  - Unreal @c UGameplayAbility + Beam @c UNiagaraComponent - 범위 즉발 + VFX + 틱 데미지 조합.
 *
 * @note @p world 또는 @c VFX::GetSpawnFxRoot() 가 nullptr 이면 no-op.
 *       "laser" effect 또는 VFXSystem 미등록 시 VFX 생략, 데미지 컴포넌트(@c RotatingHitscanLaser) 는 정상 부착.
 */
#ifndef __TOPDOWNSHOOTER_SPAWNS_ULTIMATE_LASER_H__
#define __TOPDOWNSHOOTER_SPAWNS_ULTIMATE_LASER_H__

#include <glm/glm.hpp>

// fwd - 포인터만 노출.
class b2World;
namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Spawns
{
    /// @brief 궁극기 - 플레이어 중심 회전 히트스캔 레이저 발동 (bullet_factory 패턴, 단 즉발/회전).
    /// @details VFX::SetSpawnContext 로 등록된 fxRoot+VFXSystem + "laser" effect 를 사용해
    ///          fxRoot 자식 Actor 1개를 스폰한다:
    ///            - RotatingHitscanLaser : 매 프레임 현재 각도로 RaycastAll(Enemy) -> 적별 0.2s 틱 DoDamaged (3초)
    ///            - EffekseerPlayable    : 회전 laser VFX (1초당 1회전, 3초 후 자동 종료)
    ///            - AutoDespawnOnFinish  : EffekseerPlayable finished 시 SweepFinishedChildren 가 파괴
    /// @param world           물리 월드 (레이캐스트). nullptr 이면 no-op.
    /// @param centerWorld     발동 중심 (플레이어 world 좌표). 3초간 고정.
    /// @param startAngleBox2d 시작 회전각 (box2d 평면 라디안 = atan2(box2dForward.y, box2dForward.x)).
    /// @param shooter         발사 주체 (레이캐스트 ignore - 자해 방지). nullptr 허용.
    void SpawnUltimateLaser(b2World* world, const glm::vec3& centerWorld,
                            float startAngleBox2d, SJH::Scene::Actor* shooter);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_ULTIMATE_LASER_H__
