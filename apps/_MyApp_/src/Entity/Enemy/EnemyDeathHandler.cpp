#include "Entity/Enemy/EnemyDeathHandler.h"
#include "Entity/Components/LifeComponents.h"

namespace TopdownShooter::Entity::Enemy
{
    void EnemyDeathHandler::Update(float /*dt*/)
    {
        if (mDead || !GetOwner()) return;
        auto* life = GetOwner()->GetComponent<Components::Life>();
        if (life && !life->IsAlive())
        {
            mDead = true;
            if (mOnDeathFx) mOnDeathFx(GetOwner()->GetTransform().Translate);
            GetOwner()->SetActive(false); // despawn — Bullet 의 SetActive(false) 정통과 정합
        }
    }
}
