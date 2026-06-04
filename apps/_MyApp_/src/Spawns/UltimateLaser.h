#ifndef __TOPDOWNSHOOTER_SPAWNS_ULTIMATE_LASER_H__
#define __TOPDOWNSHOOTER_SPAWNS_ULTIMATE_LASER_H__

#include <vmath.h>

// fwd — 포인터만 노출.
class b2World;
namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Spawns
{
    /// @brief 궁극기 — 플레이어 중심 회전 히트스캔 레이저 발동 (bullet_factory 패턴, 단 즉발·회전).
    /// @details VFX::SetSpawnContext 로 등록된 fxRoot+VFXSystem + "laser" effect 를 사용해
    ///          fxRoot 자식 Actor 1개를 스폰한다:
    ///            - RotatingHitscanLaser : 매 프레임 현재 각도로 RaycastAll(Enemy) → 적별 0.2s 틱 DoDamaged (3초)
    ///            - EffekseerPlayable    : 회전 laser VFX (1초당 1회전, 3초 후 자동 종료)
    ///            - AutoDespawnOnFinish  : EffekseerPlayable finished 시 SweepFinishedChildren 가 파괴
    /// @param world           물리 월드 (레이캐스트). nullptr 이면 no-op.
    /// @param centerWorld     발동 중심 (플레이어 world 좌표). 3초간 고정.
    /// @param startAngleBox2d 시작 회전각 (box2d 평면 라디안 = atan2(box2dForward.y, box2dForward.x)).
    /// @param shooter         발사 주체 (레이캐스트 ignore — 자해 방지). nullptr 허용.
    void SpawnUltimateLaser(b2World* world, const vmath::vec3& centerWorld,
                            float startAngleBox2d, SJH::Scene::Actor* shooter);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_ULTIMATE_LASER_H__
