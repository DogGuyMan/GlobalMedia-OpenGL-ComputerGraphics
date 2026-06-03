#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__

#include "scene/actor.h"
#include "timer/timer.h"

namespace TopdownShooter::Entity::Bullet
{
    /// @brief 수명 초과 시 Actor::SetActive(false). (SJH::Timer::Timer 자가 보유 — 패턴 A)
    /// @details Timer 는 카운트업(0->base) — BulletLifetime 의 기존 카운트업과 방향 동일하므로
    ///          §5.1 "역전 함정" 없음. baseTime = lifetime(항상 양수).
    class BulletLifetime : public SJH::Scene::Component
    {
      public:
        explicit BulletLifetime(float lifetime) : mTimer(lifetime) {}
        ~BulletLifetime() override = default;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override
        {
            mTimer.Tick(dt);
            if (mTimer.IsTimesUp() && GetOwner())
                GetOwner()->SetActive(false);
        }

      private:
        SJH::Timer::Timer mTimer;   // baseTime = 수명(양수). const baseTime_ 이라 재대입 불가하나 멤버 보유 OK
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__
