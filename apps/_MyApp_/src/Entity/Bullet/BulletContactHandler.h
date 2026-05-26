#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_CONTACT_HANDLER_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_CONTACT_HANDLER_H__

#include "Physics/Components.Interfaces.h"
#include "scene/actor.h"

namespace TopdownShooter::Entity::Bullet
{
    /// @brief 충돌 시 대상에 데미지 + 자신 비활성화.
    /// @details mAlive 가드 — Box2D step 내부 콜백 중 중복 발화 방지.
    ///          SetActive(false) 는 Update() 에서 지연 처리 (콜백 중 b2Body 수정 금지).
    class BulletContactHandler : public SJH::Scene::Component,
                                  public TopdownShooter::Physics::IContactable
    {
      public:
        explicit BulletContactHandler(int damage);
        ~BulletContactHandler() override;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;

        void OnCollisionEnter(SJH::Scene::Actor* other) override;
        void OnTriggerEnter  (SJH::Scene::Actor* other) override;

      private:
        void HandleHit(SJH::Scene::Actor* other);

        int  mDamage         = 10;
        bool mAlive          = true;
        bool mPendingDisable = false;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_CONTACT_HANDLER_H__
