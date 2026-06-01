#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_CONTACT_HANDLER_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_CONTACT_HANDLER_H__

#include "Physics/Components.Interfaces.h"
#include "scene/actor.h"

namespace TopdownShooter::Entity::Enemy
{
    /// @brief 플레이어에 접촉 시 IDamageable::DoDamaged(damage) 호출 (i-frame 게이트는 Life 거주).
    class EnemyContactHandler : public SJH::Scene::Component,
                                 public TopdownShooter::Physics::IContactable
    {
      public:
        explicit EnemyContactHandler(int damage);
        ~EnemyContactHandler() override;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float) override {}

        void OnCollisionEnter(SJH::Scene::Actor* other) override;

      private:
        int mDamage;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_CONTACT_HANDLER_H__
