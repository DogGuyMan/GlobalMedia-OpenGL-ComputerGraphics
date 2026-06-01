#ifndef __MYAPP_PHYSICS_MOVEMENT_H__
#define __MYAPP_PHYSICS_MOVEMENT_H__

#include "Algebraic/Stat.h"
#include "Entity/Components/Components.Interfaces.h"
#include "Physics/PhysicsComponent.h"
#include "scene/actor.h"

namespace TopdownShooter::Physics
{
    /// @brief Movement 의 PhysicsBody 변형 — DoForward 가 b2Body velocity 갱신.
    /// @details
    ///   - OnEnter 에서 owner Actor 의 Components::Physics (BoxBody/CircleBody) 를 polymorphic lookup.
    ///   - DoForward(dir, dt): body->SetLinearVelocity(normalize(dir) * speed).
    ///     dt 는 b2World::Step 이 처리 (인터페이스 호환 위해 인자는 유지).
    ///   - 좌표계: dir XZ  Box2D XY (Z  -Y, spec §4.4).
    class PhysicsMovement : public SJH::Scene::Component,
                            public Entity::IMovable
    {
    public:
        PhysicsMovement()
            : mMoveSpeed(0.0f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
        {
        }

        explicit PhysicsMovement(float movespeed)
            : mMoveSpeed(movespeed, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
        {
        }

        void OnEnter() override
        {
            mPhysicsBody = Components::FindPhysics(GetOwner());
        }

        void OnExit() override
        {
            mPhysicsBody = nullptr;
        }

        void Update(float /*dt*/) override {}

        void DoForward(vmath::vec2 dir, float /*dt*/) override
        {
            if (!mPhysicsBody || !mPhysicsBody->GetBody()) return;
            if (dir[0] == 0.0f && dir[1] == 0.0f) {
                mPhysicsBody->GetBody()->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
                return;
            }
            // dir XZ  Box2D XY (Z  -Y, spec §4.4)
            vmath::vec2 n = vmath::normalize(dir);
            const float speed = mMoveSpeed.GetValue();
            mPhysicsBody->GetBody()->SetLinearVelocity(b2Vec2(n[0] * speed, -n[1] * speed));
        }

    private:
        Algebraic::Numeric::Stat mMoveSpeed;
        Components::Physics*     mPhysicsBody = nullptr;
    };
}

#endif // __MYAPP_PHYSICS_MOVEMENT_H__
