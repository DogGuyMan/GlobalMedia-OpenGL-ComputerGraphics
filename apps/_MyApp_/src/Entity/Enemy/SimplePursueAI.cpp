/**
 * @file SimplePursueAI.cpp
 * @brief SimplePursueAI 구현 - facade 캐시 + 사망/넉백 게이트 + 타깃 방향 속도 적용.
 * @details 매 프레임 타깃과 적의 거리 벡터를 정규화해 @c mSpeed 로 스케일한 속도를 body 에 적용한다.
 *          XZ 월드 좌표를 Box2D XY 로 사상 (Z -> -Y).
 */
#include "Entity/Enemy/SimplePursueAI.h"
#include "Entity/BaseEntity.h"
#include <box2d/box2d.h>
#include <cmath>

namespace TopdownShooter::Entity::Enemy
{
    SimplePursueAI::SimplePursueAI(SJH::Scene::Actor* target, b2Body* body, float speed)
        : mTarget(target), mBody(body), mSpeed(speed) {}

    SimplePursueAI::~SimplePursueAI() = default;

    void SimplePursueAI::OnEnter()
    {
        mEntity = GetOwner() ? GetOwner()->GetComponent<TopdownShooter::Entity::BaseEntity>() : nullptr;
    }

    void SimplePursueAI::Update(float /*dt*/)
    {
        if (!mBody || !mTarget) return;

        // 사망 - 추적 정지(속도 0). facade IsAlive (기존 raw GetComponent<Life> 매프레임 호출 제거).
        if (mEntity && !mEntity->IsAlive())
        {
            mBody->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
            return;
        }

        // 넉백 중 - 추적 속도 설정 skip(버스트 보존, 0 설정 아님). 0.3s 후 자동 재개.
        if (mEntity && mEntity->IsImpulseActive())
            return;

        const auto& tp = mTarget->GetTransform().Translate;
        const b2Vec2 ep = mBody->GetPosition();
        float dx = tp[0] - ep.x;
        float dy = -tp[2] - ep.y;   // XZ->Box2D XY (Z->-Y, spec sec.4.4)
        const float len = std::sqrt(dx*dx + dy*dy);
        if (len < 0.01f) return;
        mBody->SetLinearVelocity(b2Vec2(dx/len * mSpeed, dy/len * mSpeed));
    }
}
