/**
 * @file PhysicsMovement.h
 * @brief b2Body::SetLinearVelocity 기반 이동 Component -- IMovable 물리 구현체.
 *
 * @details
 *  ### 책임
 *  - @c IMovable::DoForward(dir, dt) 구현: dir 을 정규화 후 @c mMoveSpeed 크기의 LinearVelocity 를 body 에 적용.
 *  - OnEnter 에서 owner Actor 의 @c Components::Physics (BoxBody/CircleBody) 를 polymorphic lookup.
 *  - 좌표계 변환: dir XZ -> Box2D XY (z -> -y, spec sec.4.4).
 *
 *  ### 비-책임
 *  - [X] b2World::Step 구동 -- @c PhysicsSystem::Step 담당.
 *  - [X] Transform 동기화 -- @c PhysicsSystem::SyncToTransform 담당.
 *  - [X] 버스트/넉백 -- @c Impulse Component 담당.
 *
 *  ### 정통 매핑
 *  - Unity @c Rigidbody2D.velocity 직접 설정 방식 이동.
 *
 * @note @c DoForward 내부의 @c b2Body::SetLinearVelocity 는 b2World::Step 잠금 중에도
 *       안전하게 호출 가능하다 (doc/Box2DAPI.md sec.8 "허용 (상태 변경)" 항목 참조).
 *       dt 인자는 b2World::Step 이 시간을 처리하므로 내부에서 미사용 (인터페이스 호환용).
 */
#ifndef __MYAPP_PHYSICS_MOVEMENT_H__
#define __MYAPP_PHYSICS_MOVEMENT_H__

#include "Algebraic/Stat.h"
#include "Contracts/EntityContracts.h"
#include "Physics/PhysicsComponent.h"
#include "scene/actor.h"

namespace TopdownShooter::Physics
{
    /**
     * @brief @c IMovable 의 물리 구현체 -- @c DoForward 가 @c b2Body::SetLinearVelocity 를 갱신.
     * @details
     *  - OnEnter 에서 owner Actor 의 @c Components::Physics (BoxBody/CircleBody) 를 polymorphic lookup.
     *  - @c DoForward(dir, dt): body->SetLinearVelocity(normalize(dir) * speed).
     *    @p dt 는 @c b2World::Step 이 처리하므로 내부에서 미사용 (인터페이스 호환용).
     *  - 좌표계: dir XZ -> Box2D XY (z -> -y, spec sec.4.4).
     *  - dir 이 영벡터이면 velocity 를 (0, 0) 으로 즉시 정지.
     */
    class PhysicsMovement : public SJH::Scene::Component,
                            public Entity::IMovable
    {
    public:
        /// @brief 이동 속도 0 으로 초기화. SetMoveSpeed 로 나중에 설정.
        PhysicsMovement()
            : mMoveSpeed(0.0f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
        {
        }

        /// @brief 이동 속도를 직접 지정해 초기화.
        /// @param movespeed 초기 이동 속도 (물리 단위/s).
        explicit PhysicsMovement(float movespeed)
            : mMoveSpeed(movespeed, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
        {
        }

        /// @brief owner Actor 의 @c Components::Physics 를 lookup 해 @c mPhysicsBody 에 캐시.
        void OnEnter() override
        {
            mPhysicsBody = Components::FindPhysics(GetOwner());
        }

        /// @brief 캐시된 @c mPhysicsBody 포인터 초기화.
        void OnExit() override
        {
            mPhysicsBody = nullptr;
        }

        /// @brief no-op. 이동은 @c DoForward 직접 호출로 처리.
        void Update(float /*dt*/) override {}

        /// @brief 물리 body 에 @p dir 방향으로 @c mMoveSpeed 크기의 LinearVelocity 를 적용.
        /// @details 좌표계 변환: XZ dir -> Box2D XY (z -> -y, spec sec.4.4).
        ///          dir 이 영벡터이면 즉시 정지. @p dt 는 미사용(인터페이스 호환용).
        /// @param dir 이동 방향 (XZ 좌표계, 임의 크기 허용 -- 내부 정규화).
        /// @param dt  미사용 (b2World::Step 이 시간 처리).
        void DoForward(glm::vec2 dir, float /*dt*/) override
        {
            if (!mPhysicsBody || !mPhysicsBody->GetBody()) return;
            if (dir[0] == 0.0f && dir[1] == 0.0f) {
                mPhysicsBody->GetBody()->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
                return;
            }
            // dir XZ  Box2D XY (Z  -Y, spec sec.4.4)
            glm::vec2 n = glm::normalize(dir);
            const float speed = mMoveSpeed.GetValue();
            mPhysicsBody->GetBody()->SetLinearVelocity(b2Vec2(n[0] * speed, -n[1] * speed));
        }

    private:
        Algebraic::Numeric::Stat mMoveSpeed;              ///< 이동 속도 Stat (MoveSpeed 타입).
        Components::Physics*     mPhysicsBody = nullptr;  ///< FindPhysics 결과 캐시 (비소유).
    };
}

#endif // __MYAPP_PHYSICS_MOVEMENT_H__
