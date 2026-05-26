#include "Entity/Bullet/BulletContactHandler.h"
#include "Entity/Components/LifeComponents.h"
#include "scene/actor.h"

namespace TopdownShooter::Entity::Bullet
{
    BulletContactHandler::BulletContactHandler(int damage) : mDamage(damage) {}
    BulletContactHandler::~BulletContactHandler() = default;

    void BulletContactHandler::OnCollisionEnter(SJH::Scene::Actor* other)
    {
        HandleHit(other);
    }

    void BulletContactHandler::OnTriggerEnter(SJH::Scene::Actor* other)
    {
        HandleHit(other);
    }

    void BulletContactHandler::HandleHit(SJH::Scene::Actor* other)
    {
        if (!mAlive) return;
        mAlive          = false;
        mPendingDisable = true;

        if (other)
        {
            auto* life = other->GetComponent<Components::Life>();
            if (life) life->DoDamaged(mDamage);
        }
        // SetActive(false) 는 Update() 에서 처리 — Box2D step 콜백 내부 안전 지연
    }

    void BulletContactHandler::Update(float /*dt*/)
    {
        if (mPendingDisable && GetOwner())
        {
            mPendingDisable = false;
            GetOwner()->SetActive(false);
        }
    }
}
