#include "Stage/Components/PickupTriggerLogger.h"

#include "scene/actor.h"
#include <spdlog/spdlog.h>

namespace TopdownShooter::Stage::Components
{
    // ctor/dtor out-of-line — vtable anchor 일원화로 ODR 안전 보장.
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
