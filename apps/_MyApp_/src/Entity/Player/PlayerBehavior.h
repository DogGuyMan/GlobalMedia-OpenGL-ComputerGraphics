#ifndef __TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_BEHAVIOR_H__
#define __TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_BEHAVIOR_H__

#include "playable/iplayable.h"
#include "scene/actor.h"
#include <vmath.h>

// forward declarations
namespace SJH::SpriteSequence { class SpriteSequencePlayable; }
class b2Body;

namespace TopdownShooter::Entity::Player
{
    enum class EPlayerClip : int
    {
        Idle   = 0,
        Move   = 1,
        Attack = 2,
        Hit    = 3,
    };

    class PlayerBehavior : public SJH::Scene::Component
    {
      public:
        PlayerBehavior()  = default;
        ~PlayerBehavior() override;

        // === 초기화 ===
        void Init(SJH::SpriteSequence::SpriteSequencePlayable* seq,
                  float normalSpeed, float dashSpeed = 7.5f,
                  float dashDuration = 0.3f, float dashCooldown = 0.8f,
                  float hitInvincibility = 0.5f);
        void SetBody(b2Body* body)               { mBody = body; }
        void SetSceneRoot(SJH::Scene::Actor* r)  { mSceneRoot = r; }

        // === Playable 슬롯 ===
        void SetAttackPlayable(SJH::Playable::IPlayable* p) { mAttackPlayable = p; }
        void SetHitPlayable   (SJH::Playable::IPlayable* p) { mHitPlayable    = p; }
        void SetDashPlayable  (SJH::Playable::IPlayable* p) { mDashPlayable   = p; }
        void SetDiePlayable   (SJH::Playable::IPlayable* p) { mDiePlayable    = p; }
        void SetMoveEffect    (SJH::Playable::IPlayable* p) { mMoveEffect     = p; }

        // === Flat 행동 API ===
        void Idle();
        void Move(vmath::vec2 vel);
        void Attack(vmath::vec2 dir);
        void Hit(int damage);
        void Dash(vmath::vec2 dir);
        void Die();

        vmath::vec2 GetAttackDirection() const { return mAttackDir; }
        bool        IsAlive()            const;
        bool        IsDashing()          const { return mDashTimer > 0.0f; }

        // === Component hooks ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;

      private:
        void PlayClipInternal(int clipIdx);

        SJH::SpriteSequence::SpriteSequencePlayable* mSpriteSeq = nullptr;
        b2Body*            mBody      = nullptr;
        SJH::Scene::Actor* mSceneRoot = nullptr;

        SJH::Playable::IPlayable* mAttackPlayable = nullptr;
        SJH::Playable::IPlayable* mHitPlayable    = nullptr;
        SJH::Playable::IPlayable* mDashPlayable   = nullptr;
        SJH::Playable::IPlayable* mDiePlayable    = nullptr;
        SJH::Playable::IPlayable* mMoveEffect     = nullptr;

        vmath::vec2 mAttackDir           = vmath::vec2(0.0f, -1.0f);
        float       mNormalSpeed         = 3.0f;
        float       mDashSpeed           = 7.5f;
        float       mDashDuration        = 0.3f;
        float       mDashCooldown        = 0.8f;
        float       mDashTimer           = 0.0f;
        float       mDashCooldownTimer   = 0.0f;
        float       mHitInvincibility    = 0.5f;
        float       mInvincibilityTimer  = 0.0f;
        bool        mDead                = false;
        int         mCurrentClip         = 0;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_BEHAVIOR_H__
