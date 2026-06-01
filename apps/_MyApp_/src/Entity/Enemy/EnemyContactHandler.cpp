#include "Entity/Enemy/EnemyContactHandler.h"
#include "Entity/Components/Components.Interfaces.h"   // IDamageable
#include "scene/actor.h"

namespace TopdownShooter::Entity::Enemy
{
    EnemyContactHandler::EnemyContactHandler(int damage) : mDamage(damage) {}
    EnemyContactHandler::~EnemyContactHandler() = default;

    void EnemyContactHandler::OnCollisionEnter(SJH::Scene::Actor* other)
    {
        if (!other) return;
        // 게이트 완화(Task0)로 인터페이스 직접 조회. Life 가 IDamageable 다중상속 → slow-path dynamic_cast.
        // i-frame 게이트가 Life::DoDamaged 안에 있어 총알+접촉 모두 보호.
        if (auto* dmg = other->GetComponent<IDamageable>())
            dmg->DoDamaged(mDamage);
    }
}
