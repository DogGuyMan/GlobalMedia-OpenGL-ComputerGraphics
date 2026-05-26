#ifndef __TOPDOWNSHOOTER_STAGE_STAGE_BUILDER_H__
#define __TOPDOWNSHOOTER_STAGE_STAGE_BUILDER_H__

#include "Stage/StageConfig.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Stage
{
    /// @brief Stage Actor 생성 — walls + pickups + StageState Component 가 child/component 로 매단 일반 Actor 반환.
    /// @details
    ///   - plane mesh / wallMat / pickupMat / simple.vs/fs Program 은 cfg.registry 에 자동 등록
    ///     (key: "stage_plane" / "stage_wall" / "stage_pickup" / "stage_solid_plane").
    ///   - 같은 key 가 이미 있으면 Find 로 재사용 (idempotent).
    ///   - cfg.world 또는 cfg.registry 가 nullptr 이면 assert.
    /// @param cfg StageConfig — world + registry 필수.
    /// @return Stage Actor (호출자가 Director::Root().AddChild 로 위탁)
    std::unique_ptr<SJH::Scene::Actor> CreateStageActor(const StageConfig& cfg);
}

#endif // __TOPDOWNSHOOTER_STAGE_STAGE_BUILDER_H__
