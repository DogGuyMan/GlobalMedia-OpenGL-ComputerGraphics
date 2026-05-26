#include "Entity/Enemy/EnemyContactHandler.h"
#include "Entity/Player/PlayerBehavior.h"
#include "scene/actor.h"

namespace TopdownShooter::Entity::Enemy
{
    EnemyContactHandler::EnemyContactHandler(int damage) : mDamage(damage) {}
    EnemyContactHandler::~EnemyContactHandler() = default;

    void EnemyContactHandler::OnCollisionEnter(SJH::Scene::Actor* other)
    {
        if (!other) return;
        auto* pb = other->GetComponent<Player::PlayerBehavior>();
        if (pb) pb->Hit(mDamage);
    }
}
