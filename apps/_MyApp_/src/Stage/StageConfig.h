/**
 * @file StageConfig.h
 * @brief @c CreateStageActor 의 PoD 입력 구조체 @c StageConfig 정의.
 *
 * @details
 *  ### 책임
 *  - 스테이지 조립에 필요한 외부 의존(b2World / ResourceRegistry)과 파라미터(아레나 크기 / 픽업 위치 / 시작 상태)를
 *    값-묶음(PoD)으로 전달.
 *  - main.cpp 의 startup() 이 채워서 @c StageBuilder 에 주입하는 입력 계약.
 *
 *  ### 비-책임
 *  - [X] Actor 생성/자원 등록 - @c StageBuilder(@c CreateStageActor) 담당.
 *  - [X] world/registry lifetime 소유 - 호출자(main)가 소유, 본 구조체는 비소유 포인터만 보관.
 *
 *  ### 정통 매핑
 *  - Unreal `FActorSpawnParameters` - 스폰 시 주입하는 옵션 묶음.
 *
 * @note world / registry 는 필수(nullptr 이면 @c CreateStageActor 가 assert). heavy include 회피를 위해 전방 선언만 사용.
 */
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
