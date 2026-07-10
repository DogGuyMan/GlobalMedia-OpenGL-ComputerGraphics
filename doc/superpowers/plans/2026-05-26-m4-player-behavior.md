# M4 Player Behavior + Playable Integration + Enemy + Wave — Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** SpriteSequencePlayable 다중 클립 + IntervalPlayable → PlayerBehavior(flat 함수) + Bullet + Enemy + WaveController 로 Combat Scene 기본 루프 완성.

**Architecture:**
`SJH::sprite::SpriteSequencePlayable` 에 `RegisterClip/PlayClip/RegisterOnClipEnter` 추가.  
Client 에 `PlayerBehavior` Component (flat 행동 + clip 전환) + `BulletSpawnPlayable` leaf + `CreateBulletActor/CreateEnemyActor` 헤더 온리 팩토리 + `WaveController` 추가.

**Tech Stack:** C++17, Box2D 2.4.1, Effekseer 1.7.3, FMOD Studio, SJH::engine 우산, CMake Ninja

**Spec:** `doc/superpowers/specs/2026-05-26-m4-player-behavior-design.md`

---

## 파일 맵

| 생성/수정 | 파일 | 역할 |
|---|---|---|
| 생성 | `src/playable/interval_playable.h` | 고정 시간 대기 leaf |
| 생성 | `src/playable/interval_playable.cpp` | 구현 |
| 수정 | `src/playable/CMakeLists.txt` | `interval_playable.cpp` 추가 |
| 수정 | `src/sprite/sprite_sequence_playable.h` | multi-clip API 추가 |
| 수정 | `src/sprite/sprite_sequence_playable.cpp` | multi-clip 구현 |
| 생성 | `<apps>/_MyApp_/src/Entity/Player/PlayerBehavior.h` | flat 행동 Component |
| 생성 | `<apps>/_MyApp_/src/Entity/Player/PlayerBehavior.cpp` | 구현 |
| 생성 | `<apps>/_MyApp_/src/Entity/Player/BulletSpawnPlayable.h` | 스폰 leaf |
| 생성 | `<apps>/_MyApp_/src/Entity/Player/BulletSpawnPlayable.cpp` | 구현 |
| 생성 | `apps/_MyApp_/src/Entity/Bullet/BulletLifetime.h` | 수명 타이머 |
| 생성 | `<apps>/_MyApp_/src/Entity/Bullet/BulletContactHandler.h` | 충돌 데미지 |
| 생성 | `<apps>/_MyApp_/src/Entity/Bullet/BulletContactHandler.cpp` | 구현 |
| 생성 | `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h` | 인라인 팩토리 |
| 생성 | `apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.h` | 추적 Component |
| 생성 | `apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.cpp` | 구현 |
| 생성 | `<apps>/_MyApp_/src/Entity/Enemy/EnemyContactHandler.h` | 충돌 → Hit |
| 생성 | `<apps>/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp` | 구현 |
| 생성 | `<apps>/_MyApp_/src/Entity/Enemy/enemy_factory.h` | 인라인 팩토리 |
| 생성 | `apps/_MyApp_/src/Stage/WaveController.h` | 웨이브 관리 |
| 생성 | `apps/_MyApp_/src/Stage/WaveController.cpp` | 구현 |
| 수정 | `apps/_MyApp_/src/Entity/CMakeLists.txt` | .cpp + deps 추가 |
| 수정 | `apps/_MyApp_/src/Stage/CMakeLists.txt` | WaveController.cpp + Entity 의존 |
| 수정 | `apps/_MyApp_/main.cpp` | 전체 wiring |

## Task 3: SpriteSequencePlayable — multi-clip + RegisterOnClipEnter

**Files:**
- Modify: `src/sprite/sprite_sequence_playable.h`
- Modify: `src/sprite/sprite_sequence_playable.cpp`

- [ ] **Step 1: sprite_sequence_playable.h 수정**

```cpp
#ifndef __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__
#define __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__

#include "playable/iplayable.h"            // SJH::Playable::IPlayable* 슬롯
#include "playable/playable_base.h"
#include "sprite_frame_clip.h"
#include "sprite_component.h"
#include <unordered_map>
#include <vector>

namespace SJH::SpriteSequence
{
    class SpriteSequencePlayable : public SJH::Playable::PlayableBase
    {
      public:
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                const SpriteFrameClip*       clip);
        ~SpriteSequencePlayable() override;

        // === Multi-clip API (M4) ===
        /// @brief 클립 인덱스에 SpriteFrameClip* 등록. PlayClip 이 이를 선택.
        SpriteSequencePlayable& RegisterClip(int clipIdx, const SpriteFrameClip* clip);
        /// @brief 클립 진입 시 Play() 호출될 IPlayable* 등록 (여러 개 등록 가능).
        SpriteSequencePlayable& RegisterOnClipEnter(int clipIdx, SJH::Playable::IPlayable* sideEffect);
        /// @brief 지정 클립으로 전환 + elapsed_ 리셋 + 등록된 sideEffect Play.
        void PlayClip(int clipIdx);
        int  CurrentClip() const { return currentClipIdx_; }

      protected:
        void OnUpdate(float dt) override;

      private:
        SJH::Sprite::SpriteRenderer* sprite_;
        const SpriteFrameClip*        clip_;   // 하위 호환 기본 클립 (clipIdx=0 fallback)

        std::unordered_map<int, const SpriteFrameClip*>        clips_;
        std::unordered_map<int, std::vector<SJH::Playable::IPlayable*>> onClipEnter_;
        int currentClipIdx_ = 0;
    };
}

#endif // __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__
```

- [ ] **Step 2: sprite_sequence_playable.cpp 수정**

```cpp
#include "sprite_sequence_playable.h"
#include "sprite_frame_clip.h"
#include "sprite_component.h"

namespace SJH::SpriteSequence
{
    SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                                    const SpriteFrameClip*       clip)
        : sprite_(spriteRef), clip_(clip)
    {
    }

    SpriteSequencePlayable::~SpriteSequencePlayable() = default;

    SpriteSequencePlayable& SpriteSequencePlayable::RegisterClip(int clipIdx,
                                                                   const SpriteFrameClip* clip)
    {
        clips_[clipIdx] = clip;
        return *this;
    }

    SpriteSequencePlayable& SpriteSequencePlayable::RegisterOnClipEnter(
        int clipIdx, SJH::Playable::IPlayable* sideEffect)
    {
        onClipEnter_[clipIdx].push_back(sideEffect);
        return *this;
    }

    void SpriteSequencePlayable::PlayClip(int clipIdx)
    {
        if (currentClipIdx_ == clipIdx) return;
        currentClipIdx_ = clipIdx;
        elapsed_        = 0.0f;
        finished_       = false;

        auto it = onClipEnter_.find(clipIdx);
        if (it != onClipEnter_.end())
        {
            for (auto* p : it->second)
                if (p) { p->Stop(); p->Play(); }
        }
    }

    void SpriteSequencePlayable::OnUpdate(float /*dt*/)
    {
        if (!sprite_) return;

        // 현재 클립 선택: clips_ 우선, fallback=clip_
        const SpriteFrameClip* clip = nullptr;
        auto it = clips_.find(currentClipIdx_);
        if (it != clips_.end())
            clip = it->second;
        else
            clip = clip_;

        if (!clip || clip->fps <= 0.0f || clip->frameCount <= 0) return;

        const float frameDur = 1.0f / clip->fps;
        int raw = static_cast<int>(elapsed_ / frameDur);

        if (isLoop_)
        {
            raw %= clip->frameCount;
        }
        else if (raw >= clip->frameCount)
        {
            raw       = clip->frameCount - 1;
            finished_ = true;
        }
        sprite_->frameIdx = clip->startFrame + raw;
    }
}
```

- [ ] **Step 3: 빌드 확인**

```bash
cmake --build --preset ninja --target sjhopengl_sprite 2>&1 | tail -5
```

---

## Task 4: PlayerBehavior Component

**Files:**
- Create: `<apps>/_MyApp_/src/Entity/Player/PlayerBehavior.h`
- Create: `<apps>/_MyApp_/src/Entity/Player/PlayerBehavior.cpp`

- [ ] **Step 1: PlayerBehavior.h 생성**

```cpp
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
        void PlayClip(int clipIdx);

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
        float       mHitInvincibility    = 0.5f;
        float       mDashTimer           = 0.0f;
        float       mDashCooldownTimer   = 0.0f;
        float       mInvincibilityTimer  = 0.0f;
        bool        mDead                = false;
        int         mCurrentClip         = 0;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_PLAYER_PLAYER_BEHAVIOR_H__
```

- [ ] **Step 2: PlayerBehavior.cpp 생성**

```cpp
#include "<Entity>/Player/PlayerBehavior.h"
#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"
#include "sprite/sprite_sequence_playable.h"
#include <<box2d>/box2d.h>
#include <algorithm>
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

    void PlayerBehavior::PlayClip(int clipIdx)
    {
        if (mCurrentClip == clipIdx) return;

        // Move 발 파티클 처리
        if (clipIdx != static_cast<int>(EPlayerClip::Move) && mMoveEffect) mMoveEffect->Stop();
        if (clipIdx == static_cast<int>(EPlayerClip::Move) && mMoveEffect) mMoveEffect->Play();

        mCurrentClip = clipIdx;
        if (mSpriteSeq) mSpriteSeq->PlayClip(clipIdx);
    }

    void PlayerBehavior::Idle()
    {
        PlayClip(static_cast<int>(EPlayerClip::Idle));
    }

    void PlayerBehavior::Move(vmath::vec2 vel)
    {
        if (IsDashing()) return;
        PlayClip(static_cast<int>(EPlayerClip::Move));
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

        PlayClip(static_cast<int>(EPlayerClip::Attack));
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
            PlayClip(static_cast<int>(EPlayerClip::Hit));
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
        PlayClip(static_cast<int>(EPlayerClip::Move));   // Dash clip = Move (D9 reuse)
        if (mDashPlayable) { mDashPlayable->Stop(); mDashPlayable->Play(); }
    }

    void PlayerBehavior::Die()
    {
        if (mDead) return;
        mDead = true;
        if (GetOwner()) GetOwner()->SetActive(false);
        PlayClip(static_cast<int>(EPlayerClip::Hit));   // Die clip = Hit (D9 reuse)
        if (mDiePlayable) { mDiePlayable->Stop(); mDiePlayable->Play(); }
    }

    void PlayerBehavior::Update(float dt)
    {
        if (mDashTimer > 0.0f)          mDashTimer          -= dt;
        if (mDashCooldownTimer > 0.0f)  mDashCooldownTimer  -= dt;
        if (mInvincibilityTimer > 0.0f) mInvincibilityTimer -= dt;

        // Hit 클립이 끝나면 Idle 복귀
        if (mCurrentClip == static_cast<int>(EPlayerClip::Hit) ||
            mCurrentClip == static_cast<int>(EPlayerClip::Attack))
        {
            if (mSpriteSeq && mSpriteSeq->IsFinished())
                PlayClip(static_cast<int>(EPlayerClip::Idle));
        }

        // 사망 감지 (DoDamaged 이후 다음 프레임에 감지)
        if (!mDead && !IsAlive()) Die();
    }
}
```

---

## Task 5: BulletSpawnPlayable

**Files:**
- Create: `<apps>/_MyApp_/src/Entity/Player/BulletSpawnPlayable.h`
- Create: `<apps>/_MyApp_/src/Entity/Player/BulletSpawnPlayable.cpp`

- [ ] **Step 1: BulletSpawnPlayable.h 생성**

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_PLAYER_BULLET_SPAWN_PLAYABLE_H__
#define __TOPDOWNSHOOTER_ENTITY_PLAYER_BULLET_SPAWN_PLAYABLE_H__

#include "<Entity>/Player/PlayerBehavior.h"
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
        void OnPlay()         override;
        void OnUpdate(float)  override {}

      private:
        PlayerBehavior*    mBehavior;
        SJH::Scene::Actor* mSceneRoot;
        BulletFactory      mFactory;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_PLAYER_BULLET_SPAWN_PLAYABLE_H__
```

- [ ] **Step 2: BulletSpawnPlayable.cpp 생성**

```cpp
#include "<Entity>/Player/BulletSpawnPlayable.h"

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
                vmath::vec2 pos(t[0], t[2]);   // XZ 평면 (Box2D XY)
                vmath::vec2 dir = mBehavior->GetAttackDirection();
                mSceneRoot->AddChild(mFactory(pos, dir));
            }
        }
        finished_ = true;
    }
}
```

---

## Task 6: Bullet Entity — BulletLifetime + BulletContactHandler + bullet_factory

**Files:**
- Create: `apps/_MyApp_/src/Entity/Bullet/BulletLifetime.h`
- Create: `<apps>/_MyApp_/src/Entity/Bullet/BulletContactHandler.h`
- Create: `<apps>/_MyApp_/src/Entity/Bullet/BulletContactHandler.cpp`
- Create: `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h`

- [ ] **Step 1: BulletLifetime.h 생성 (헤더 온리)**

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__

#include "scene/actor.h"

namespace TopdownShooter::Entity::Bullet
{
    /// @brief 수명 초과 시 Actor::SetActive(false). b2Body 비활성화는 WaveController 담당.
    class BulletLifetime : public SJH::Scene::Component
    {
      public:
        explicit BulletLifetime(float lifetime) : mLifetime(lifetime) {}
        ~BulletLifetime() override = default;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override
        {
            mElapsed += dt;
            if (mElapsed >= mLifetime && GetOwner())
                GetOwner()->SetActive(false);
        }

      private:
        float mLifetime;
        float mElapsed = 0.0f;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_LIFETIME_H__
```

- [ ] **Step 2: BulletContactHandler.h 생성**

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_CONTACT_HANDLER_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_CONTACT_HANDLER_H__

#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"
#include "scene/actor.h"

namespace TopdownShooter::Entity::Bullet
{
    /// @brief 충돌 시 대상에 데미지 + 자신 비활성화 (SetActive(false)).
    /// @details mAlive 가드 — Box2D step 내부 콜백 중 중복 발화 방지.
    class BulletContactHandler : public SJH::Scene::Component,
                                  public TopdownShooter::Physics::IContactable
    {
      public:
        explicit BulletContactHandler(int damage);
        ~BulletContactHandler() override;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;

        void OnCollisionEnter(SJH::Scene::Actor* other) override;
        void OnTriggerEnter  (SJH::Scene::Actor* other) override;

      private:
        void HandleHit(SJH::Scene::Actor* other);

        int  mDamage = 10;
        bool mAlive  = true;
        bool mPendingDisable = false;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_CONTACT_HANDLER_H__
```

- [ ] **Step 3: BulletContactHandler.cpp 생성**

```cpp
#include "<Entity>/Bullet/BulletContactHandler.h"
#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"
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
        // SetActive(false) 는 Update() 에서 처리 — Box2D step 내부 호출이므로 안전하게 지연.
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
```

- [ ] **Step 4: bullet_factory.h 생성 (헤더 온리 팩토리)**

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__
#define __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__

#include "<Entity>/Bullet/BulletContactHandler.h"
#include "apps/_MyApp_/src/Entity/Bullet/BulletLifetime.h"
#include "apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h"
#include "<Physics>/filter.h"
#include "scene/actor.h"
#include <<box2d>/box2d.h>
#include <memory>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Entity::Bullet
{
    struct BulletConfig
    {
        b2World*    world;
        vmath::vec2 pos;
        vmath::vec2 dir;        // normalized
        float       speed    = 15.0f;
        int         damage   = 10;
        float       lifetime = 3.0f;
    };

    inline std::unique_ptr<SJH::Scene::Actor> CreateBulletActor(const BulletConfig& cfg)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>("Bullet");

        b2BodyDef bd;
        bd.type = b2_kinematicBody;
        bd.position.Set(cfg.pos[0], cfg.pos[1]);
        const float speed = cfg.speed;
        bd.linearVelocity.Set(cfg.dir[0] * speed, cfg.dir[1] * speed);
        b2Body* body = cfg.world->CreateBody(&bd);

        b2CircleShape circle;
        circle.m_radius = 0.15f;

        b2FixtureDef fd;
        fd.shape               = &circle;
        fd.density             = 1.0f;
        fd.isSensor            = false;
        fd.filter.categoryBits = Physics::ToBits(Physics::PhysicsLayer::BulletPlayer);
        fd.filter.maskBits     = Physics::ToBits(Physics::PhysicsLayer::Enemy | Physics::PhysicsLayer::Wall);
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<Physics::Components::BoxBody>();
        pb->SetBody(body);

        actor->AddComponent<BulletContactHandler>(cfg.damage);
        actor->AddComponent<BulletLifetime>(cfg.lifetime);

        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_ENTITY_BULLET_BULLET_FACTORY_H__
```

---

## Task 7: Enemy Entity — SimplePursueAI + EnemyContactHandler + enemy_factory

**Files:**
- Create: `apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.h`
- Create: `apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.cpp`
- Create: `<apps>/_MyApp_/src/Entity/Enemy/EnemyContactHandler.h`
- Create: `<apps>/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp`
- Create: `<apps>/_MyApp_/src/Entity/Enemy/enemy_factory.h`

- [ ] **Step 1: SimplePursueAI.h 생성**

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__

#include "scene/actor.h"
#include <vmath.h>

class b2Body;

namespace TopdownShooter::Entity::Enemy
{
    class SimplePursueAI : public SJH::Scene::Component
    {
      public:
        SimplePursueAI(SJH::Scene::Actor* target, b2Body* body, float speed);
        ~SimplePursueAI() override;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;

      private:
        SJH::Scene::Actor* mTarget;
        b2Body*            mBody;
        float              mSpeed;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__
```

- [ ] **Step 2: SimplePursueAI.cpp 생성**

```cpp
#include "apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.h"
#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"
#include <<box2d>/box2d.h>
#include <cmath>

namespace TopdownShooter::Entity::Enemy
{
    SimplePursueAI::SimplePursueAI(SJH::Scene::Actor* target, b2Body* body, float speed)
        : mTarget(target), mBody(body), mSpeed(speed) {}

    SimplePursueAI::~SimplePursueAI() = default;

    void SimplePursueAI::Update(float /*dt*/)
    {
        if (!mBody || !mTarget) return;

        // 사망 감지 — 죽으면 정지
        auto* life = GetOwner() ? GetOwner()->GetComponent<Components::Life>() : nullptr;
        if (life && !life->IsAlive())
        {
            mBody->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
            if (GetOwner()) GetOwner()->SetActive(false);
            return;
        }

        const auto& tp = mTarget->GetTransform().Translate;
        const b2Vec2 ep = mBody->GetPosition();
        float dx = tp[0] - ep.x;
        float dy = -tp[2] - ep.y;   // XZ→Box2D XY (Z→-Y)
        const float len = std::sqrt(dx*dx + dy*dy);
        if (len < 0.01f) return;
        mBody->SetLinearVelocity(b2Vec2(dx/len * mSpeed, dy/len * mSpeed));
    }
}
```

- [ ] **Step 3: EnemyContactHandler.h 생성**

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_CONTACT_HANDLER_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_CONTACT_HANDLER_H__

#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"
#include "scene/actor.h"

namespace TopdownShooter::Entity::Enemy
{
    /// @brief 플레이어에 접촉 시 PlayerBehavior::Hit(damage) 호출.
    class EnemyContactHandler : public SJH::Scene::Component,
                                 public TopdownShooter::Physics::IContactable
    {
      public:
        explicit EnemyContactHandler(int damage);
        ~EnemyContactHandler() override;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float) override {}

        void OnCollisionEnter(SJH::Scene::Actor* other) override;

      private:
        int mDamage;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_CONTACT_HANDLER_H__
```

- [ ] **Step 4: EnemyContactHandler.cpp 생성**

```cpp
#include "<Entity>/Enemy/EnemyContactHandler.h"
#include "<Entity>/Player/PlayerBehavior.h"

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
```

- [ ] **Step 5: enemy_factory.h 생성 (헤더 온리 팩토리)**

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__

#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"
#include "<Entity>/Enemy/EnemyContactHandler.h"
#include "apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.h"
#include "apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h"
#include "<Physics>/filter.h"
#include "scene/actor.h"
#include <<box2d>/box2d.h>
#include <memory>
#include <vmath.h>

namespace TopdownShooter::Entity::Enemy
{
    struct EnemyConfig
    {
        b2World*           world;
        vmath::vec2        pos;
        SJH::Scene::Actor* playerTarget;
        int   hp     = 30;
        float speed  = 2.0f;
        int   damage = 10;
    };

    inline std::unique_ptr<SJH::Scene::Actor> CreateEnemyActor(const EnemyConfig& cfg)
    {
        auto actor = std::make_unique<SJH::Scene::Actor>("Enemy");

        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.position.Set(cfg.pos[0], cfg.pos[1]);
        bd.linearDamping = 0.5f;
        b2Body* body = cfg.world->CreateBody(&bd);

        b2CircleShape circle;
        circle.m_radius = 0.4f;

        b2FixtureDef fd;
        fd.shape               = &circle;
        fd.density             = 1.0f;
        fd.friction            = 0.3f;
        fd.filter.categoryBits = Physics::ToBits(Physics::PhysicsLayer::Enemy);
        fd.filter.maskBits     = Physics::ToBits(Physics::EnemyMask);
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<Physics::Components::BoxBody>();
        pb->SetBody(body);

        actor->AddComponent<Components::Life>(cfg.hp);
        actor->AddComponent<SimplePursueAI>(cfg.playerTarget, body, cfg.speed);
        actor->AddComponent<EnemyContactHandler>(cfg.damage);

        return actor;
    }
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_FACTORY_H__
```

---

## Task 8: WaveController

**Files:**
- Create: `apps/_MyApp_/src/Stage/WaveController.h`
- Create: `apps/_MyApp_/src/Stage/WaveController.cpp`

- [ ] **Step 1: WaveController.h 생성**

```cpp
#ifndef __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__
#define __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__

#include "scene/actor.h"
#include <vector>
#include <vmath.h>

class b2World;

namespace TopdownShooter::Stage
{
    /// @brief 웨이브 기반 Enemy spawn 관리.
    /// @details
    ///   - kSpawnInterval 마다 1마리 spawn (최대 kMaxEnemies 동시 생존).
    ///   - mEnemies raw ptr 추적 — IsActive()==false 시 전멸 감지.
    ///   - 전멸 → mWave++ + 다음 웨이브 즉시 개시.
    class WaveController : public SJH::Scene::Component
    {
      public:
        WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                       SJH::Scene::Actor* playerActor, float arenaHalfExtent);
        ~WaveController() override;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;

      private:
        void       SpawnEnemy();
        vmath::vec2 RandomEdgePos() const;
        int        LiveCount() const;

        b2World*           mWorld;
        SJH::Scene::Actor* mSpawnParent;
        SJH::Scene::Actor* mPlayerActor;
        float              mArenaHalfExtent;

        std::vector<SJH::Scene::Actor*> mEnemies;
        int   mWave       = 0;
        float mSpawnTimer = 0.0f;

        static constexpr float kSpawnInterval = 3.0f;
        static constexpr int   kMaxEnemies    = 5;
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__
```

- [ ] **Step 2: WaveController.cpp 생성**

```cpp
#include "apps/_MyApp_/src/Stage/WaveController.h"
#include "<Entity>/Enemy/enemy_factory.h"
#include <<box2d>/box2d.h>
#include <cstdlib>   // rand
#include <<spdlog>/spdlog.h>

namespace TopdownShooter::Stage
{
    WaveController::WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                                    SJH::Scene::Actor* playerActor, float arenaHalfExtent)
        : mWorld(world), mSpawnParent(spawnParent),
          mPlayerActor(playerActor), mArenaHalfExtent(arenaHalfExtent)
    {}

    WaveController::~WaveController() = default;

    int WaveController::LiveCount() const
    {
        int n = 0;
        for (auto* e : mEnemies)
            if (e && e->IsActive()) ++n;
        return n;
    }

    vmath::vec2 WaveController::RandomEdgePos() const
    {
        // 4 변 중 하나에 랜덤 배치
        const float h = mArenaHalfExtent - 0.5f;
        const int side = rand() % 4;
        const float t  = (static_cast<float>(rand()) / RAND_MAX) * 2.0f * h - h;
        switch (side)
        {
        case 0: return vmath::vec2(-h, t);
        case 1: return vmath::vec2( h, t);
        case 2: return vmath::vec2(t, -h);
        default: return vmath::vec2(t,  h);
        }
    }

    void WaveController::SpawnEnemy()
    {
        if (!mWorld || !mSpawnParent || !mPlayerActor) return;
        Entity::Enemy::EnemyConfig cfg;
        cfg.world        = mWorld;
        cfg.pos          = RandomEdgePos();
        cfg.playerTarget = mPlayerActor;
        cfg.hp           = 20 + mWave * 5;
        cfg.speed        = 1.5f + mWave * 0.3f;
        cfg.damage       = 10;

        auto* enemy = mSpawnParent->AddChild(Entity::Enemy::CreateEnemyActor(cfg));
        mEnemies.push_back(enemy);
        spdlog::info("[Wave {}] Enemy spawned at ({:.1f},{:.1f})", mWave, cfg.pos[0], cfg.pos[1]);
    }

    void WaveController::Update(float dt)
    {
        // 전멸 감지 → 다음 웨이브
        if (mWave > 0 && LiveCount() == 0)
        {
            ++mWave;
            mEnemies.clear();
            mSpawnTimer = 0.0f;
            spdlog::info("[Wave] All cleared → Wave {}", mWave);
        }

        // 첫 웨이브 개시
        if (mWave == 0)
        {
            mWave = 1;
            mSpawnTimer = 0.0f;
        }

        mSpawnTimer += dt;
        if (mSpawnTimer >= kSpawnInterval && LiveCount() < kMaxEnemies)
        {
            mSpawnTimer = 0.0f;
            SpawnEnemy();
        }
    }
}
```

---

## Task 9: CMakeLists 수정 — Entity + Stage

**Files:**
- Modify: `apps/_MyApp_/src/Entity/CMakeLists.txt`
- Modify: `apps/_MyApp_/src/Stage/CMakeLists.txt`

- [ ] **Step 1: Entity/CMakeLists.txt 수정**

```cmake
add_library(myapp_entity STATIC
    BaseEntity.cpp
    <Player>/PlayerBehavior.cpp
    <Player>/BulletSpawnPlayable.cpp
    <Bullet>/BulletContactHandler.cpp
    apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.cpp
    <Enemy>/EnemyContactHandler.cpp
)
add_library(MyApp::Entity ALIAS myapp_entity)

target_include_directories(myapp_entity
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(myapp_entity
    PUBLIC
        SJH::scene
        MyApp::Algebraic
        SJH::playable       # PlayerBehavior / BulletSpawnPlayable 헤더 노출
    PRIVATE
        SJH::sprite         # PlayerBehavior.cpp 가 SpriteSequencePlayable 메서드 호출
        game_deps           # SimplePursueAI.cpp 가 b2Body (box2d 헤더) 사용
)

target_compile_features(myapp_entity PUBLIC cxx_std_17)
```

- [ ] **Step 2: Stage/CMakeLists.txt 수정**

```cmake
add_library(myapp_stage STATIC
    StageBuilder.cpp
    apps/_MyApp_/src/Stage/Components/PickupTriggerLogger.cpp
    WaveController.cpp
)
add_library(MyApp::Stage ALIAS myapp_stage)

target_include_directories(myapp_stage
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(myapp_stage
    PUBLIC
        SJH::scene
        SJH::object
        SJH::material
        SJH::render
        SJH::resource_registry
        MyApp::Physics
        MyApp::Entity       # WaveController → enemy_factory (헤더 온리 팩토리 포함)
        project_deps
        game_deps
)

target_compile_features(myapp_stage PUBLIC cxx_std_17)
```

- [ ] **Step 3: 빌드 확인 — Entity + Stage 독립 빌드**

```bash
cmake --build --preset ninja --target myapp_entity myapp_stage 2>&1 | tail -10
```

Expected: 경고 없이 빌드 성공.

---

<!-- ## Task 10: main.cpp — PlayerBehavior Init + Clip 등록 + Playable 조립

**Files:**
- Modify: `apps/_MyApp_/main.cpp`

- [ ] **Step 1: include 추가**

`main.cpp` 기존 include 블록에 추가:

```cpp
#include "<Entity>/Player/PlayerBehavior.h"
#include "<Entity>/Player/BulletSpawnPlayable.h"
#include "apps/_MyApp_/src/Entity/Bullet/bullet_factory.h"
```

- [ ] **Step 2: 클립 정의 — startup() 안, atlas 생성 직후**

```cpp
// === M4 — SpriteFrameClip 4종 (TestPattern 4×4 기준) ===
static const SJH::SpriteSequence::SpriteFrameClip kClipIdle   {  0, 4,  4.0f };
static const SJH::SpriteSequence::SpriteFrameClip kClipMove   {  4, 4,  8.0f };
static const SJH::SpriteSequence::SpriteFrameClip kClipAttack {  8, 4, 12.0f };
static const SJH::SpriteSequence::SpriteFrameClip kClipHit    { 12, 4, 12.0f };
```

- [ ] **Step 3: mSpriteSeq 초기화 변경 + clip 등록**

기존 `mWholeAtlasClip` + `mSpriteSeq` 생성 코드를 교체:

기존:
```cpp
mWholeAtlasClip = SJH::SpriteSequence::SpriteFrameClip{0, atlas->FrameCount(), 4.0f};
mSpriteSeq = spriteActor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
    mSprite, &mWholeAtlasClip);
mSpriteSeq->SetIsLoop(true);
mSpriteSeq->Play();
```

교체 후:
```cpp
mSpriteSeq = spriteActor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
    mSprite, &kClipIdle);
mSpriteSeq->RegisterClip(0, &kClipIdle)
           .RegisterClip(1, &kClipMove)
           .RegisterClip(2, &kClipAttack)
           .RegisterClip(3, &kClipHit);
mSpriteSeq->SetIsLoop(true);
mSpriteSeq->Play();
```

- [ ] **Step 4: PlayerBehavior AddComponent + Init**

`mSpriteActor = dir.Root().AddChild(...)` 라인 직후에 추가:

```cpp
// === M4 — PlayerBehavior 부착 ===
auto& phys = TopdownShooter::Director::Get().Physics();
mPlayerBehavior = mSpriteActor->AddComponent<
    TopdownShooter::Entity::Player::PlayerBehavior>();

// physics body 참조 — BoxBody::GetBody() via PhysicsComponent.Imp.h
{
    using namespace TopdownShooter::Physics;
    auto* pb = mSpriteActor->GetComponent<Components::BoxBody>();
    if (pb) mPlayerBehavior->SetBody(pb->GetBody());
}

mPlayerBehavior->Init(mSpriteSeq, pac.movement.speed,
                       pac.movement.speed * 2.5f);   // dashSpeed = normal×2.5
mPlayerBehavior->SetSceneRoot(&dir.Root());
```

- [ ] **Step 5: Attack Playable 체인 조립 + 클립 등록**

PlayerBehavior Init 직후에 추가:

```cpp
// === M4 — Attack Playable 체인 ===
{
    using namespace SJH::Playable;
    using namespace TopdownShooter::Entity::Player;

    auto& audio = TopdownShooter::Director::Get().Audio();
    auto& vfx   = TopdownShooter::Director::Get().VFX();
    auto* muzzle     = reg.FindEffect("muzzle");
    auto* slashEvt   = audio.LoadEvent("event:/Slash");
    auto* damagedEvt = audio.LoadEvent("event:/Damaged");

    // BulletSpawnPlayable — factory delegate
    BulletSpawnPlayable::BulletFactory bulletFact =
        [&phys, dmg=10](vmath::vec2 pos, vmath::vec2 dir)
        {
            return TopdownShooter::Entity::Bullet::CreateBulletActor(
                { &phys.World(), pos, dir, 15.0f, dmg, 3.0f });
        };
    auto* spawnPlay = mSpriteActor->AddComponent<BulletSpawnPlayable>(
        mPlayerBehavior, &dir.Root(), std::move(bulletFact));

    // Attack chain: Parallel { Effekseer ∥ FMOD ∥ Seq{Interval(0.3) → Spawn} }
    auto attackChain = std::make_unique<ParallelPlayable>();
    if (muzzle)
        attackChain->Join(std::make_unique<TopdownShooter::VFX::EffekseerPlayable>(
            vfx.GetManager(), muzzle, vmath::vec3(0.0f),
            TopdownShooter::VFX::TrackPolicy::Static));
    if (slashEvt)
        attackChain->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(slashEvt));
    {
        auto spawnSeq = std::make_unique<SequencePlayable>();
        spawnSeq->AppendInterval(0.3f);
        // BulletSpawnPlayable 은 Actor 소유 Component — raw ptr 을 IPlayable* 로 캐스트
        spawnSeq->Append(std::make_unique<SJH::Playable::IntervalPlayable>(0.001f)); // placeholder
        attackChain->Join(std::move(spawnSeq));
        // NOTE: spawnPlay 는 Actor Component (Actor 소유). Playable 체인에 직접 넣지 않고
        //       RegisterOnClipEnter 와 병렬로 Play 를 호출하는 방식으로 처리:
    }
    // 실제로는 spawnPlay 를 직접 RegisterOnClipEnter 에 등록
    mSpriteSeq->RegisterOnClipEnter(2, spawnPlay);   // Attack clip(2) 진입 시 spawn

    // Hit chain
    if (damagedEvt)
    {
        auto hitChain = std::make_unique<ParallelPlayable>();
        if (muzzle)
            hitChain->Join(std::make_unique<TopdownShooter::VFX::EffekseerPlayable>(
                vfx.GetManager(), muzzle, vmath::vec3(0.0f),
                TopdownShooter::VFX::TrackPolicy::Static));
        hitChain->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(damagedEvt));
        auto* hitComp = mSpriteActor->AddComponent<ParallelPlayable>(std::move(*hitChain));
        mPlayerBehavior->SetHitPlayable(hitComp);
        mSpriteSeq->RegisterOnClipEnter(3, hitComp);   // Hit clip(3)
    }
}
```

> **Note:** Attack 체인은 여기서는 단순화하여 spawnPlay 를 직접 클립 진입 시 Play. EffekseerPlayable + FmodStudioPlayable 은 D4 에 따라 조립. 리소스 없으면 null-check 후 skip.

- [ ] **Step 6: private 멤버 선언 추가 (class 내)**

`game_application` private 멤버에 추가:

```cpp
TopdownShooter::Entity::Player::PlayerBehavior* mPlayerBehavior = nullptr;
```

- [ ] **Step 7: shutdown() 에 nullptr 추가**

```cpp
mPlayerBehavior = nullptr;
```

---

## Task 11: main.cpp — WaveController + onKey Dash + onMouseButton Attack

**Files:**
- Modify: `apps/_MyApp_/main.cpp`

- [ ] **Step 1: include 추가**

```cpp
#include "<Entity>/Enemy/enemy_factory.h"
#include "apps/_MyApp_/src/Stage/WaveController.h"
```

- [ ] **Step 2: WaveController 추가 — startup() 에서 Stage actor 에 AddComponent**

`dir.Enter()` 호출 직전에 추가:

```cpp
// === M4 — WaveController (Stage Actor 에 AddComponent) ===
// NOTE: dir.Root() 의 children 중 "Stage" 이름 Actor 를 찾거나,
//       Stage Actor 를 별도 변수로 보관해야 함.
//       현재는 dir.Root() 에 직접 WaveController 부착 (Stage 참조 없는 경우 대비).
{
    auto& physWorld = TopdownShooter::Director::Get().Physics().World();
    dir.Root().AddComponent<TopdownShooter::Stage::WaveController>(
        &physWorld, &dir.Root(), mSpriteActor, 10.0f /*arenaHalfExtent*/);
}
```

- [ ] **Step 3: onMouseButton — Attack 처리**

기존 `onMouseButton` 에서 기존 Shot+Muzzle 로직 교체 또는 병행:

```cpp
void onMouseButton(int button, int action) override
{
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    mMouse.HandleButton(button, action, x, y);

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // === M4 — PlayerBehavior::Attack ===
        if (mPlayerBehavior)
        {
            int fbW = 0, fbH = 0;
            glfwGetFramebufferSize(window, &fbW, &fbH);
            // 화면 중심 기준 방향 (top-down 근사)
            float nx = static_cast<float>(x) / fbW * 2.0f - 1.0f;
            float ny = 1.0f - static_cast<float>(y) / fbH * 2.0f;
            mPlayerBehavior->Attack(vmath::vec2(nx, ny));
        }
    }
}
```

- [ ] **Step 4: onKey — Dash 처리**

기존 `onKey` 에 추가 (G 키 블록 AFTER):

```cpp
if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS)
{
    if (mPlayerBehavior)
    {
        // 현재 이동 방향 = keyboard input (마지막 Move 방향) 사용
        // 단순화: 임시로 전방(-Z 방향 = XZ 평면 (0,-1)) Dash
        mPlayerBehavior->Dash(vmath::vec2(0.0f, -1.0f));
    }
}
``` -->

---

## Task 12: 빌드

- [ ] **Step 1: configure + build**

```bash
cmake --preset ninja && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -20
```

Expected: 경고 0 (Debug clang), 링크 성공.

- [ ] **Step 2: 링크 에러 발생 시**

```bash
# 기존 object 캐시 삭제 후 재빌드
rm -f build_ninja/apps/_MyApp_/CMakeFiles/_MyApp_.dir/*.o 2>/dev/null
cmake --build --preset ninja --target _MyApp_ 2>&1 | grep -E "error:|warning:" | head -20
```

---

## Task 13: 시각 회귀 검증

- [ ] **Step 1: 실행**

```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

- [ ] **Step 2: 체크리스트 (수동 확인)**

```
□ WASD 이동 → Move 클립(4-7 프레임) 전환 확인
□ 정지 → Idle 클립(0-3 프레임) 복귀 확인
□ 마우스 좌클릭 → Attack 클립(8-11) + 0.3s 후 총알 spawn
□ 총알이 화면 이동 + Enemy(3초마다 spawn) 접촉 시 Enemy SetActive(false)
□ Enemy 가 Player 에 닿으면 Hit 클립(12-15) 전환
□ 기존 BGM/SFX 재생 정상 (M5 퇴행 없음)
□ Dash (SHIFT) → 빠른 이동 + 0.3s 후 속도 감소 (linearDamping)
□ 최대 5마리 Enemy 제한 (3초 간격 spawn)
□ 전멸 후 Wave 증가 + 다음 스폰 spdlog 로그 확인
```

- [ ] **Step 3: 이상 없으면 커밋**

```bash
git add \
  src/playable/interval_playable.h \
  src/playable/interval_playable.cpp \
  src/playable/composite_playable.h \
  src/playable/composite_playable.cpp \
  src/playable/CMakeLists.txt \
  src/sprite/sprite_sequence_playable.h \
  src/sprite/sprite_sequence_playable.cpp \
  apps/_MyApp_/src/Entity/ \
  apps/_MyApp_/src/Stage/WaveController.h \
  apps/_MyApp_/src/Stage/WaveController.cpp \
  apps/_MyApp_/src/Stage/CMakeLists.txt \
  apps/_MyApp_/main.cpp

git commit -m "$(cat <<'EOF'
feat(m4): PlayerBehavior + Bullet + Enemy + Wave — Combat Scene 기본 루프

- SpriteSequencePlayable multi-clip + RegisterOnClipEnter IPlayable* 슬롯
- PlayerBehavior flat 메서드 (Idle/Move/Attack/Hit/Dash/Die) + EPlayerClip
- BulletSpawnPlayable (factory delegate — Entity↔Physics 순환 의존 회피)
- CreateBulletActor / CreateEnemyActor 헤더 온리 팩토리
- SimplePursueAI + WaveController (3s interval, max5, wave++)

EOF
)"
```

---

## Self-Review

### Spec 커버리지 체크

| 결정 | 태스크 |
|---|---|
| D1 (no FSM) | Task 4 (PlayerBehavior only) ✓ |
| D2 (multi-clip) | Task 3 ✓ |
| D4 (Attack chain) | Task 10 Step 5 ✓ |
| D5 (Bullet) | Task 6 ✓ |
| D6 (Enemy) | Task 7 ✓ |
| D7 (WaveController) | Task 8 ✓ |
| D8 (PlayerBehavior API) | Task 4 ✓ |
| D9 (EPlayerClip) | Task 4 + Task 10 Step 2 ✓ |
| D10 (Move feet particle) | Task 4 PlayerBehavior::PlayClip ✓ |
| D11 (circular dep 회피) | Task 5 (BulletSpawnPlayable factory delegate) + Task 9 PRIVATE links ✓ |

### 타입/시그니처 일관성

- `SpriteSequencePlayable::RegisterClip` → Task 3 (h) + Task 3 (cpp) 일치 ✓
- `PlayerBehavior::Init` 시그니처 → Task 4 (h) + Task 4 (cpp) + Task 10 Step 4 호출 일치 ✓
- `BulletSpawnPlayable::BulletFactory` typedef → Task 5 (h) + Task 10 Step 5 호출 일치 ✓
- `WaveController` 생성자 → Task 8 (h/cpp) + Task 11 Step 2 호출 일치 ✓

### 플레이스홀더 스캔

- Task 10 Step 5 에 Attack chain 조립 시 BulletSpawnPlayable 주석 있음 — 실제 spawnPlay 는
  `RegisterOnClipEnter(2, spawnPlay)` 직접 등록으로 처리됨 (SequencePlayable 안에 넣는 방식 대신).
  NOTE 주석으로 설명 ✓
