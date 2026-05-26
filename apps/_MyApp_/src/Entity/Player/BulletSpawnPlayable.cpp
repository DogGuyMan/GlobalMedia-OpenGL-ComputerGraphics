#include "Entity/Player/BulletSpawnPlayable.h"
#include "Entity/Player/PlayerBehavior.h"
#include "scene/actor.h"
#include "object/transform.h"
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Entity::Player
{
    BulletSpawnPlayable::BulletSpawnPlayable(PlayerBehavior* behavior,
                                              SJH::Scene::Actor* sceneRoot,
                                              BulletFactory factory)
        : mBehavior(behavior), mSceneRoot(sceneRoot), mFactory(std::move(factory))
    {}

    BulletSpawnPlayable::~BulletSpawnPlayable() = default;

    void BulletSpawnPlayable::OnPlay()
    {
        if (mFactory && mBehavior && mSceneRoot)
        {
            auto* owner = mBehavior->GetOwner();
            if (owner)
            {
                const auto& t = owner->GetTransform().Translate;
                vmath::vec2 pos(t[0], t[2]);   // XZ 평면 → Box2D XY
                vmath::vec2 dir = mBehavior->GetAttackDirection();
                mSceneRoot->AddChild(mFactory(pos, dir));
            }
        }
        finished_ = true;
    }
}
