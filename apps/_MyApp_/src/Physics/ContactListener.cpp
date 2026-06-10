/**
 * @file ContactListener.cpp
 * @brief PhysicsContactListener 구현 - body userdata Actor 복원 + sensor 분기 + IContactable 디스패치.
 *
 * @details
 *  BeginContact/EndContact 공통 흐름:
 *  1. 두 fixture 의 body userdata 에서 owner Actor* 복원 (@c ActorFromBody).
 *  2. 어느 쪽이든 sensor 면 Trigger, 둘 다 solid 면 Collision 으로 @c Phase 결정.
 *  3. 양방향으로 @c Dispatch 두 번 호출 - self/other 를 서로 바꿔 양쪽 Actor 가 모두 콜백을 받게 한다.
 */
#include "Physics/ContactListener.h"
#include "Physics/Components.Interfaces.h"

#include "scene/actor.h"

#include <spdlog/spdlog.h>

namespace TopdownShooter::Physics
{
    namespace
    {
        // b2Body::GetUserData().pointer 에 등록된 Actor* 회수.
        // Components::Physics::SetBody 가 reinterpret_cast<uintptr_t>(Actor*) 로 등록.
        SJH::Scene::Actor* ActorFromBody(b2Body* body)
        {
            if (!body) return nullptr;
            uintptr_t raw = body->GetUserData().pointer;
            return reinterpret_cast<SJH::Scene::Actor*>(raw);
        }

        // 디스패치할 접촉 단계 태그 (Begin/End x Trigger/Collision 4종).
        enum class Phase { TriggerEnter, TriggerExit, CollisionEnter, CollisionExit };

        // Actor(self) 의 모든 Component 중 IContactable 구현체를 찾아 other 와 함께 phase 콜백 전달.
        void Dispatch(SJH::Scene::Actor* self, SJH::Scene::Actor* other, Phase phase)
        {
            if (!self) return;
            self->ForEachComponent([&](SJH::Scene::Component* c) {
                if (auto* listener = dynamic_cast<IContactable*>(c)) {
                    switch (phase) {
                    case Phase::TriggerEnter:   listener->OnTriggerEnter(other);   break;
                    case Phase::TriggerExit:    listener->OnTriggerExit(other);    break;
                    case Phase::CollisionEnter: listener->OnCollisionEnter(other); break;
                    case Phase::CollisionExit:  listener->OnCollisionExit(other);  break;
                    }
                }
            });
        }
    }

    // 접촉 시작 - sensor 분기로 Trigger/Collision Enter 를 양쪽 Actor 에 전달.
    void PhysicsContactListener::BeginContact(b2Contact* contact)
    {
        if (!contact) return;
        b2Fixture* fA = contact->GetFixtureA();
        b2Fixture* fB = contact->GetFixtureB();
        if (!fA || !fB) return;

        SJH::Scene::Actor* aA = ActorFromBody(fA->GetBody());
        SJH::Scene::Actor* aB = ActorFromBody(fB->GetBody());

        // 하나라도 sensor  양쪽 모두 OnTriggerEnter (Unity Collider.isTrigger 정통)
        const bool eitherSensor = fA->IsSensor() || fB->IsSensor();
        const Phase phase = eitherSensor ? Phase::TriggerEnter : Phase::CollisionEnter;

        Dispatch(aA, aB, phase);
        Dispatch(aB, aA, phase);
    }

    // 접촉 종료 - sensor 분기로 Trigger/Collision Exit 를 양쪽 Actor 에 전달.
    void PhysicsContactListener::EndContact(b2Contact* contact)
    {
        if (!contact) return;
        b2Fixture* fA = contact->GetFixtureA();
        b2Fixture* fB = contact->GetFixtureB();
        if (!fA || !fB) return;

        SJH::Scene::Actor* aA = ActorFromBody(fA->GetBody());
        SJH::Scene::Actor* aB = ActorFromBody(fB->GetBody());

        const bool eitherSensor = fA->IsSensor() || fB->IsSensor();
        const Phase phase = eitherSensor ? Phase::TriggerExit : Phase::CollisionExit;

        Dispatch(aA, aB, phase);
        Dispatch(aB, aA, phase);
    }
}
