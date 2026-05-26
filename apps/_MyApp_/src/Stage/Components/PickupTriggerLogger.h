#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_PICKUP_TRIGGER_LOGGER_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_PICKUP_TRIGGER_LOGGER_H__

#include "Physics/Components.Interfaces.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage::Components
{
    /// @brief 픽업/감지 영역 — Trigger 이벤트를 로그로 출력 (M3 시각 검증용).
    /// @details
    ///   - SJH::Scene::Component (Update/OnEnter/OnExit hook) + Physics::IContactable (trigger 콜백) 다중 상속.
    ///   - 멤버 변수 없음 — 콜백만. ctor/dtor 는 .cpp 에 out-of-line 정의해 vtable anchor 일원화 (ODR 안전).
    ///   - 향후 picking-up 동작 (HP 회복, ammo refill 등) 은 별도 Component 가 IContactable 구현해 부착.
    class PickupTriggerLogger : public SJH::Scene::Component,
                                public TopdownShooter::Physics::IContactable
    {
    public:
        PickupTriggerLogger();
        ~PickupTriggerLogger() override;

        // SJH::Scene::Component hook — pure virtual 충족용 stub.
        void OnEnter() override {}
        void OnExit() override {}
        void Update(float /*dt*/) override {}

        // Physics::IContactable — trigger enter/exit 만 로깅. stay/contact 류는 base default(empty) 사용.
        void OnTriggerEnter(SJH::Scene::Actor* other) override;
        void OnTriggerExit(SJH::Scene::Actor* other) override;
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_PICKUP_TRIGGER_LOGGER_H__
