/**
 * @file PickupTriggerLogger.cpp
 * @brief PickupTriggerLogger 의 ctor/dtor out-of-line 정의 + Trigger 콜백 구현.
 *
 * @details
 *  ctor/dtor 를 이 TU 에 out-of-line 으로 정의해 vtable anchor 를 단일 .cpp 에
 *  고정한다. 이로써 ODR 위반 없이 헤더 inline 보다 안전하게 vtable 을 관리한다.
 *
 *  OnTriggerEnter / OnTriggerExit 는 spdlog::info 로 "[Pickup]" 태그와 함께
 *  진입/이탈한 Actor 이름을 출력한다.
 */
#include "Stage/Components/PickupTriggerLogger.h"

#include "scene/actor.h"
#include <spdlog/spdlog.h>

namespace TopdownShooter::Stage::Components
{
    // ctor/dtor out-of-line — vtable anchor 를 이 TU 에 고정해 ODR 안전 보장.
    PickupTriggerLogger::PickupTriggerLogger()  = default;
    PickupTriggerLogger::~PickupTriggerLogger() = default;

    void PickupTriggerLogger::OnTriggerEnter(SJH::Scene::Actor* other)
    {
        spdlog::info("[Pickup] OnTriggerEnter — other='{}'",
                     other ? other->GetName().c_str() : "(null)");
    }

    void PickupTriggerLogger::OnTriggerExit(SJH::Scene::Actor* other)
    {
        spdlog::info("[Pickup] OnTriggerExit — other='{}'",
                     other ? other->GetName().c_str() : "(null)");
    }
}
