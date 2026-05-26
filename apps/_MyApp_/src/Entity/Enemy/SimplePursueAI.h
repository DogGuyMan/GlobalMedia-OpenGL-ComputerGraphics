#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__

#include "scene/actor.h"
#include <vmath.h>

class b2Body;

namespace TopdownShooter::Entity::Enemy
{
    /// @brief 매 프레임 Player 위치를 향해 SetLinearVelocity. 사망 시 정지 + SetActive(false).
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
