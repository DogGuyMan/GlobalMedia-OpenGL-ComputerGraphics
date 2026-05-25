#ifndef __MYAPP_PHYSICS_BODY_H__
#define __MYAPP_PHYSICS_BODY_H__

#include "scene/actor.h"
#include <box2d/box2d.h>
#include <vmath.h>

namespace TopdownShooter::Physics
{
    /// @brief b2Body* non-owning 보유 Component. PhysicsSystem 의 b2World 가 lifetime 소유.
    /// @details
    ///   ### 좌표계 (spec §4.4)
    ///   - 물리: b2Vec2(x, y) 2D XY 평면
    ///   - 렌더: Transform.Translate = vmath::vec3(x, heightOffset, -y) 3D XZ 평면 (top-down)
    ///
    ///   ### Sensor (Unity Collider.isTrigger 매핑)
    ///   - mIsSensor=true  → b2Fixture::SetSensor(true). 물리 충돌 없음, 이벤트만.
    ///   - mIsSensor=false → b2Fixture::SetSensor(false) (default). 물리 충돌 + 이벤트.
    ///
    ///   ### isTrigger 단일 source-of-truth
    ///   - mIsSensor 가 유일한 진실. 런타임 변경은 SetSensor(bool) 만.
    class PhysicsBodyComponent : public SJH::Scene::Component
    {
    public:
        PhysicsBodyComponent() = default;

        /// @brief b2Body* 주입 — b2World::CreateBody 결과. user data 에 owner Actor* 자동 등록.
        PhysicsBodyComponent& SetBody(b2Body* body)
        {
            mBody = body;
            if (mBody)
                mBody->GetUserData().pointer = reinterpret_cast<uintptr_t>(GetOwner());
            return *this;
        }

        PhysicsBodyComponent& SetHeightOffset(float y)
        {
            mHeightOffset = y;
            return *this;
        }

        /// @brief Unity Collider.isTrigger 와 동일. 런타임 변경 시 모든 fixture 에 전파.
        PhysicsBodyComponent& SetSensor(bool s)
        {
            mIsSensor = s;
            if (mBody) {
                for (b2Fixture* f = mBody->GetFixtureList(); f; f = f->GetNext())
                    f->SetSensor(s);
            }
            return *this;
        }

        b2Body* GetBody()          const { return mBody; }
        float   GetHeightOffset()  const { return mHeightOffset; }
        bool    IsSensor()         const { return mIsSensor; }

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override {}

    private:
        b2Body* mBody         = nullptr;
        float   mHeightOffset = 0.0f;
        bool    mIsSensor     = false;
    };
}

#endif // __MYAPP_PHYSICS_BODY_H__
