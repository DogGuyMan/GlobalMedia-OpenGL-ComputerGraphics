/**
 * @file PickupTriggerLogger.h
 * @brief 픽업 감지 영역의 Trigger 이벤트를 spdlog 로 로깅하는 Component.
 *
 * @details
 *  ### 책임
 *  - @c SJH::Scene::Component + @c Physics::IContactable 을 다중 상속하여
 *    Box2D Sensor(isTrigger=true) 영역에 오브젝트가 진입/이탈할 때 spdlog::info 로 기록한다.
 *  - M3 물리 시각 검증용 컴포넌트 — 실제 아이템 획득 로직은 별도 Component 가 담당한다.
 *
 *  ### 비-책임
 *  - [X] HP 회복 / ammo refill 등 픽업 동작 — 별도 Component 가 IContactable 구현해 부착.
 *  - [X] 물리 바디 생성 — CreatePickupActor 팩토리(@c pickup_factory.h) 가 BoxBody 를 부착.
 *
 * @note ctor/dtor 를 .cpp 에 out-of-line 정의해 vtable anchor 를 단일 TU 에 고정한다 (ODR 안전).
 */
#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_PICKUP_TRIGGER_LOGGER_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_PICKUP_TRIGGER_LOGGER_H__

#include "Physics/Components.Interfaces.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage::Components
{
    /**
     * @brief 픽업 감지 영역 — Trigger 이벤트를 spdlog 로 로깅하는 Component (M3 시각 검증용).
     * @details
     *  @c SJH::Scene::Component (Update/OnEnter/OnExit hook) + @c Physics::IContactable
     *  (trigger 콜백) 를 다중 상속한다. 멤버 변수 없음 — 콜백만 구현.
     *  ctor/dtor 는 .cpp 에 out-of-line 정의해 vtable anchor 를 단일 TU 에 고정한다 (ODR 안전).
     *
     *  향후 실제 픽업 동작(HP 회복, ammo refill 등)은 별도 Component 가 @c IContactable 을
     *  구현해 부착하는 방식으로 확장한다.
     */
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
        /// @brief 다른 Actor 가 픽업 감지 영역에 진입할 때 spdlog::info 로 기록.
        /// @param other 진입한 Actor 포인터 (nullptr 이면 "(null)" 로 출력).
        void OnTriggerEnter(SJH::Scene::Actor* other) override;

        /// @brief 다른 Actor 가 픽업 감지 영역에서 이탈할 때 spdlog::info 로 기록.
        /// @param other 이탈한 Actor 포인터 (nullptr 이면 "(null)" 로 출력).
        void OnTriggerExit(SJH::Scene::Actor* other) override;
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_PICKUP_TRIGGER_LOGGER_H__
