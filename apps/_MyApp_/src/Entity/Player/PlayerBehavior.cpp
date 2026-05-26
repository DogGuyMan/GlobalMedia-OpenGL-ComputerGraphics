#include "Entity/Player/PlayerBehavior.h"
#include "Entity/Components/LifeComponents.h"
#include "sprite/sprite_sequence_playable.h"
#include <box2d/box2d.h>
#include <vmath.h>
#include <cmath>

namespace TopdownShooter::Entity::Player
{
    PlayerBehavior::~PlayerBehavior() = default;

    void PlayerBehavior::Init(SJH::SpriteSequence::SpriteSequencePlayable* seq,
                               float normalSpeed, float dashSpeed,
                               float dashDuration, float dashCooldown,
                               float hitInvincibility)
    {
        mSpriteSeq        = seq;
        mNormalSpeed      = normalSpeed;
        mDashSpeed        = dashSpeed;
        mDashDuration     = dashDuration;
        mDashCooldown     = dashCooldown;
        mHitInvincibility = hitInvincibility;
    }

    bool PlayerBehavior::IsAlive() const
    {
        if (!GetOwner()) return false;
        auto* life = GetOwner()->GetComponent<Components::Life>();
        return life ? life->IsAlive() : false;
    }

    void PlayerBehavior::PlayClipInternal(int clipIdx)
    {
        if (mCurrentClip == clipIdx) return;

        // Move 발 파티클 처리 (D10)
        if (clipIdx != static_cast<int>(EPlayerClip::Move) && mMoveEffect) mMoveEffect->Stop();
        if (clipIdx == static_cast<int>(EPlayerClip::Move) && mMoveEffect) mMoveEffect->Play();

        mCurrentClip = clipIdx;
        if (mSpriteSeq) mSpriteSeq->PlayClip(clipIdx);
    }

    void PlayerBehavior::Idle()
    {
        PlayClipInternal(static_cast<int>(EPlayerClip::Idle));
    }

    void PlayerBehavior::Move(vmath::vec2 vel)
    {
        if (IsDashing()) return;
        PlayClipInternal(static_cast<int>(EPlayerClip::Move));
        if (mBody)
        {
            const float len = std::sqrt(vel[0]*vel[0] + vel[1]*vel[1]);
            if (len > 0.001f)
            {
                vmath::vec2 n(vel[0]/len, vel[1]/len);
                // XZ→Box2D XY (Z→-Y, spec §4.4)
                mBody->SetLinearVelocity(b2Vec2(n[0]*mNormalSpeed, -n[1]*mNormalSpeed));
            }
        }
    }

    void PlayerBehavior::Attack(vmath::vec2 dir)
    {
        const float len = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1]);
        if (len > 0.001f) mAttackDir = vmath::vec2(dir[0]/len, dir[1]/len);

        PlayClipInternal(static_cast<int>(EPlayerClip::Attack));
        if (mAttackPlayable) { mAttackPlayable->Stop(); mAttackPlayable->Play(); }
    }

    void PlayerBehavior::Hit(int damage)
    {
        if (mInvincibilityTimer > 0.0f || mDead) return;
        auto* life = GetOwner() ? GetOwner()->GetComponent<Components::Life>() : nullptr;
        if (life) life->DoDamaged(damage);
        mInvincibilityTimer = mHitInvincibility;

        if (IsAlive())
        {
            PlayClipInternal(static_cast<int>(EPlayerClip::Hit));
            if (mHitPlayable) { mHitPlayable->Stop(); mHitPlayable->Play(); }
        }
        else
        {
            Die();
        }
    }

    void PlayerBehavior::Dash(vmath::vec2 dir)
    {
        if (mDashCooldownTimer > 0.0f || IsDashing() || mDead) return;
        mDashTimer         = mDashDuration;
        mDashCooldownTimer = mDashCooldown;

        const float len = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1]);
        if (mBody && len > 0.001f)
        {
            vmath::vec2 n(dir[0]/len, dir[1]/len);
            mBody->SetLinearVelocity(b2Vec2(n[0]*mDashSpeed, -n[1]*mDashSpeed));
        }
        // Dash 클립 = Move 재사용 (D9)
        PlayClipInternal(static_cast<int>(EPlayerClip::Move));
        if (mDashPlayable) { mDashPlayable->Stop(); mDashPlayable->Play(); }
    }

    void PlayerBehavior::Die()
    {
        if (mDead) return;
        mDead = true;
        if (GetOwner()) GetOwner()->SetActive(false);
        // Die 클립 = Hit 재사용 (D9)
        PlayClipInternal(static_cast<int>(EPlayerClip::Hit));
        if (mDiePlayable) { mDiePlayable->Stop(); mDiePlayable->Play(); }
    }

    void PlayerBehavior::Update(float dt)
    {
        if (mDashTimer > 0.0f)          mDashTimer          -= dt;
        if (mDashCooldownTimer > 0.0f)  mDashCooldownTimer  -= dt;
        if (mInvincibilityTimer > 0.0f) mInvincibilityTimer -= dt;

        // Hit/Attack 클립이 끝나면 Idle 복귀
        if (mCurrentClip == static_cast<int>(EPlayerClip::Hit) ||
            mCurrentClip == static_cast<int>(EPlayerClip::Attack))
        {
            if (mSpriteSeq && mSpriteSeq->IsFinished())
                PlayClipInternal(static_cast<int>(EPlayerClip::Idle));
        }

        // 속도 기반 Idle/Move 자동 전환 (Attack/Hit 중에는 건드리지 않음)
        if (mBody &&
            mCurrentClip != static_cast<int>(EPlayerClip::Attack) &&
            mCurrentClip != static_cast<int>(EPlayerClip::Hit))
        {
            const float speed = mBody->GetLinearVelocity().Length();
            if (speed > 0.1f)
                PlayClipInternal(static_cast<int>(EPlayerClip::Move));
            else
                PlayClipInternal(static_cast<int>(EPlayerClip::Idle));
        }

        // 사망 감지 (DoDamaged 이후 다음 프레임에 감지)
        if (!mDead && !IsAlive()) Die();
    }
}
