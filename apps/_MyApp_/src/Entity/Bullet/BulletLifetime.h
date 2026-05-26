#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__

#include "scene/actor.h"

namespace TopdownShooter::Entity::Bullet
{
    /// @brief 수명 초과 시 Actor::SetActive(false).
    class BulletLifetime : public SJH::Scene::Component
    {
      public:
        explicit BulletLifetime(float lifetime) : mLifetime(lifetime) {}
        ~BulletLifetime() override = default;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override
        {
            mElapsed += dt;
            if (mElapsed >= mLifetime && GetOwner())
                GetOwner()->SetActive(false);
        }

      private:
        float mLifetime;
        float mElapsed = 0.0f;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__
