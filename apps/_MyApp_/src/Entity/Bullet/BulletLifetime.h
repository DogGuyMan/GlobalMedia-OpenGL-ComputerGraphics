/**
 * @file BulletLifetime.h
 * @brief 총알의 수명을 카운트업해 만료 시 Actor 를 비활성화하는 Component.
 * @details
 *  Timer 자가 보유(패턴 A) - BaseEntity 중앙 timer 가 아닌 자체 @c SJH::Timer::Timer 멤버를 직접 tick.
 *  총알은 매우 짧은 수명의 일회성 Actor 라 중앙 등록 부담 없이 self-contained 가 적합.
 */
#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__

#include "scene/actor.h"
#include "timer/timer.h"

namespace TopdownShooter::Entity::Bullet
{
    /**
     * @brief 수명 초과 시 @c Actor::SetActive(false). (SJH::Timer::Timer 자가 보유 - 패턴 A)
     * @details Timer 는 카운트업(0->base) - BulletLifetime 의 기존 카운트업과 방향 동일하므로
     *          "역전 함정"(카운트다운 전환 시 부호 뒤집힘) 없음. baseTime = lifetime(항상 양수).
     */
    class BulletLifetime : public SJH::Scene::Component
    {
      public:
        /// @brief 수명을 초 단위로 받아 내부 Timer 의 baseTime 으로 설정.
        /// @param lifetime 총알 수명(초, 양수).
        explicit BulletLifetime(float lifetime) : mTimer(lifetime) {}
        ~BulletLifetime() override = default;

        void OnEnter() override {}
        void OnExit()  override {}
        /// @brief Timer 를 tick 하고 만료되면 owner Actor 를 비활성화.
        /// @param dt 직전 프레임 경과 시간(초).
        void Update(float dt) override
        {
            mTimer.Tick(dt);
            if (mTimer.IsTimesUp() && GetOwner())
                GetOwner()->SetActive(false);
        }

      private:
        SJH::Timer::Timer mTimer;   ///< 수명 카운트업 Timer. baseTime = 수명(양수). const baseTime_ 라 재대입 불가하나 멤버 보유 OK.
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__
