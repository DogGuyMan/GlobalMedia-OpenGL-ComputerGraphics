#include "Entity/Enemy/SimplePursueAI.h"
#include "Entity/Components/LifeComponents.h"
#include <box2d/box2d.h>
#include <cmath>

namespace TopdownShooter::Entity::Enemy
{
    SimplePursueAI::SimplePursueAI(SJH::Scene::Actor* target, b2Body* body, float speed)
        : mTarget(target), mBody(body), mSpeed(speed) {}

    SimplePursueAI::~SimplePursueAI() = default;

    void SimplePursueAI::Update(float /*dt*/)
    {
        if (!mBody || !mTarget) return;

        // 사망 감지 — 죽으면 추적 정지(속도 0)만. 비활성/despawn 은 Life 의 사망 지연(mDieTimer)이 담당.
        // (여기서 즉시 SetActive(false) 하면 사망 dissolve 연출이 0프레임이 되어 안 보인다 — Life 가 0.6s 후 비활성.)
        auto* life = GetOwner() ? GetOwner()->GetComponent<Components::Life>() : nullptr;
        if (life && !life->IsAlive())
        {
            mBody->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
            return;
        }

        const auto& tp = mTarget->GetTransform().Translate;
        const b2Vec2 ep = mBody->GetPosition();
        float dx = tp[0] - ep.x;
        float dy = -tp[2] - ep.y;   // XZ→Box2D XY (Z→-Y, spec §4.4)
        const float len = std::sqrt(dx*dx + dy*dy);
        if (len < 0.01f) return;
        mBody->SetLinearVelocity(b2Vec2(dx/len * mSpeed, dy/len * mSpeed));
    }
}
