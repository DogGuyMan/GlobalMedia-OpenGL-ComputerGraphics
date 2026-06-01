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

        enum class Phase { TriggerEnter, TriggerExit, CollisionEnter, CollisionExit };

        // Actor 의 모든 Component 중 IContactable 구현체에 콜백 전달.
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
