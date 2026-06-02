#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_DEATH_HANDLER_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_DEATH_HANDLER_H__

#include "scene/actor.h"
#include <functional>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Entity::Enemy
{
    /// @brief 적 사망 self-poll — Life::IsAlive()==false 감지 시 death FX delegate + despawn (M6 Q4).
    /// @details
    ///   - 사망 self-poll — Life::IsAlive()==false 감지 시 death FX delegate + despawn. 죽음은 적(자신)이 소유.
    ///   - death FX 는 std::function delegate 로 주입 (Entity→Spawns 의존 회피, BulletFactory 정통).
    ///   - 현 Components::Life::DoDie 빈 구현 + enemy despawn 부재 를 본 핸들러가 보완.
    class EnemyDeathHandler : public SJH::Scene::Component
    {
      public:
        using DeathFx = std::function<void(const vmath::vec3&)>;
        explicit EnemyDeathHandler(DeathFx fx) : mOnDeathFx(std::move(fx)) {}

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;

      private:
        DeathFx mOnDeathFx;
        bool    mDead = false;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_DEATH_HANDLER_H__
