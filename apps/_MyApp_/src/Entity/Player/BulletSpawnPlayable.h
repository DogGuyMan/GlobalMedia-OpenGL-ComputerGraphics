#ifndef __TOPDOWNSHOOTER_ENTITY_PLAYER_BULLET_SPAWN_PLAYABLE_H__
#define __TOPDOWNSHOOTER_ENTITY_PLAYER_BULLET_SPAWN_PLAYABLE_H__

#include "Entity/Player/PlayerBehavior.h"
#include "playable/playable_base.h"
#include "scene/actor.h"
#include <functional>
#include <memory>
#include <vmath.h>

namespace TopdownShooter::Entity::Player
{
    /// @brief Attack chain 안의 leaf — OnPlay 시 총알 Actor 한 발 스폰.
    /// @details BulletFactory delegate 로 Physics 의존 없음 — Entity 순환 의존 회피.
    class BulletSpawnPlayable : public SJH::Playable::PlayableBase
    {
      public:
        using BulletFactory =
            std::function<std::unique_ptr<SJH::Scene::Actor>(vmath::vec2 pos, vmath::vec2 dir)>;

        BulletSpawnPlayable(PlayerBehavior* behavior,
                             SJH::Scene::Actor* sceneRoot,
                             BulletFactory factory);
        ~BulletSpawnPlayable() override;

        void OnEnter() override {}
        void OnExit()  override {}

      protected:
        void OnPlay()        override;
        void OnUpdate(float) override {}

      private:
        PlayerBehavior*    mBehavior;
        SJH::Scene::Actor* mSceneRoot;
        BulletFactory      mFactory;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_PLAYER_BULLET_SPAWN_PLAYABLE_H__
