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

        // 사망 감지 — 죽으면 정지 후 비활성화
        auto* life = GetOwner() ? GetOwner()->GetComponent<Components::Life>() : nullptr;
        if (life && !life->IsAlive())
        {
            mBody->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
            if (GetOwner()) GetOwner()->SetActive(false);
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
