#ifndef __TOPDOWNSHOOTER_STAGE_STAGE_CONFIG_H__
#define __TOPDOWNSHOOTER_STAGE_STAGE_CONFIG_H__

#include "Stage/Stage.h"
#include "Stage/Constants.h"
#include <vector>
#include <vmath.h>

// b2World / ResourceRegistry forward decl — heavy include 회피.
class b2World;
namespace SJH
{
    class ResourceRegistry;
}

namespace TopdownShooter::Stage
{
    /// @brief CreateStageActor 의 PoD 입력. main.cpp 가 startup() 안에서 채워서 주입.
    /// @details
    ///   - world / registry 는 *필수* — nullptr 이면 CreateStageActor 가 assert.
    ///   - pickupPositions 가 비어 있으면 Pickup Sensor 0 개 (벽만).
    struct StageConfig
    {
        b2World*               world           = nullptr;   // 필수
        SJH::ResourceRegistry* registry        = nullptr;   // 필수
        float                  arenaHalfExtent = ARENA_HALF_EXTENT; // 벽 안쪽 절반 크기
        float                  wallThickness   = WALL_THICKNESS;
        std::vector<vmath::vec2> pickupPositions = { vmath::vec2(0.0f, 3.0f) };
        EStageStatus           startStatus     = EStageStatus::Title;
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_STAGE_CONFIG_H__
