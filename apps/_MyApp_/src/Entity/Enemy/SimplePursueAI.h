#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__

#include "scene/actor.h"
#include <vmath.h>

class b2Body;

namespace TopdownShooter::Entity::Enemy
{
    /// @brief 매 프레임 Player 위치를 향해 SetLinearVelocity. 사망 시 추적 정지(속도 0)만 —
    ///        비활성/despawn 은 Life 의 사망 지연(mDieTimer)이 담당(사망 dissolve 시간 확보).
    class SimplePursueAI : public SJH::Scene::Component
    {
      public:
        SimplePursueAI(SJH::Scene::Actor* target, b2Body* body, float speed);
        ~SimplePursueAI() override;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;

      private:
        SJH::Scene::Actor* mTarget;
        b2Body*            mBody;
        float              mSpeed;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__
