#include "Stage/WaveController.h"
#include "Bootstrap/EnemyBuilder.h"
#include "Stage/Constants.h"
#include "Stage/Stage.h"                      // EStageStatus::GameOver
#include "Stage/State/StageStateMachine.h"    // StageStateMachine::TryTransit (GameOver 전이)
#include "Entity/Components/LifeComponents.h" // Life::SetOnDeath / IsDespawnReady (사망 observer)
#include "Physics/PhysicsComponent.h"         // FindPhysics / SetBodyEnabled (사망 시 충돌 정지)
#include <box2d/box2d.h>
#include <algorithm>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace TopdownShooter::Stage
{
    WaveController::WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                                    SJH::Scene::Actor* playerActor, float arenaHalfExtent)
        : mWorld(world), mSpawnParent(spawnParent),
          mPlayerActor(playerActor), mArenaHalfExtent(arenaHalfExtent),
          mSpawnTimer(WAVE_SPAWN_INTERVAL)
    {}

    WaveController::~WaveController() = default;

    void WaveController::OnEnter()
    {
        // Player 사망 observer 등록 — 사망(HP0) 시 플래그 set, Update 가 GameOver 전이 (IsActive 폴링 대체).
        // (Player 는 제거하지 않음 — 시체/물리 유지. enemy 만 SweepDespawned 로 완전 제거.)
        if (mPlayerActor)
            if (auto* life = mPlayerActor->GetComponent<Entity::Components::Life>())
                life->SetOnDeath([this](SJH::Scene::Actor*) { mPlayerDead = true; });
    }

    // 사망 통지(observer) — live(mEnemies)에서 dying 으로 이동 + 즉시 충돌 정지(body 비활성).
    // 실제 RemoveChild 는 SweepDespawned(Update 밖)가 디졸브 끝에 수행 (deferred — iterator 무효화 회피).
    void WaveController::OnEnemyDeath(SJH::Scene::Actor* e)
    {
        if (e == nullptr) return;
        mEnemies.erase(std::remove(mEnemies.begin(), mEnemies.end(), e), mEnemies.end());
        mDying.push_back(e);
        if (auto* phys = Physics::Components::FindPhysics(e))
            phys->SetBodyEnabled(false); // b2Body::SetEnabled(false) — 디졸브 동안 충돌/밀기 정지
    }

    void WaveController::SweepDespawned()
    {
        // 디졸브 끝(Life::IsDespawnReady) 적을 RemoveChild -> OnExit -> Physics::OnExit::DestroyBody.
        for (auto it = mDying.begin(); it != mDying.end();)
        {
            SJH::Scene::Actor* e    = *it;
            auto*              life = e ? e->GetComponent<Entity::Components::Life>() : nullptr;
            if (e == nullptr || life == nullptr || life->IsDespawnReady())
            {
                if (e && mSpawnParent) mSpawnParent->RemoveChild(e);
                it = mDying.erase(it);
            }
            else
                ++it;
        }
    }

    int WaveController::LiveCount() const
    {
        return static_cast<int>(mEnemies.size()); // mEnemies = 생존만 (사망 즉시 dying 이동) — 폴링 없음
    }

    vmath::vec2 WaveController::RandomEdgePos() const
    {
        const float h    = mArenaHalfExtent - 0.5f;
        const int   side = rand() % 4;
        const float t    = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f * h - h;
        switch (side)
        {
        case 0:  return vmath::vec2(-h,  t);
        case 1:  return vmath::vec2( h,  t);
        case 2:  return vmath::vec2( t, -h);
        default: return vmath::vec2( t,  h);
        }
    }

    void WaveController::SpawnEnemy()
    {
        if (!mWorld || !mSpawnParent || !mPlayerActor) return;

        Bootstrap::EnemyDeps d;
        d.world        = mWorld;
        d.spawnParent  = mSpawnParent;
        d.playerTarget = mPlayerActor;
        d.pos          = RandomEdgePos();
        d.hp           = WAVE_HP_BASE + mWave * WAVE_HP_PER_WAVE;
        d.speed        = WAVE_SPEED_BASE + static_cast<float>(mWave) * WAVE_SPEED_PER_WAVE;
        d.damage       = WAVE_CONTACT_DAMAGE;
        d.variant      = mSpawnCount % 3; // 3종 순환

        auto* enemy = Bootstrap::BuildEnemy(d);
        // 사망 observer 주입 — WaveController 가 적 컴포넌트 init-time 배선 소유 (Stage->Entity inward).
        if (enemy)
            if (auto* life = enemy->GetComponent<Entity::Components::Life>())
                life->SetOnDeath([this](SJH::Scene::Actor* e) { OnEnemyDeath(e); });
        ++mSpawnCount;
        mEnemies.push_back(enemy);
        mWaveSpawnedAny = true;
        spdlog::info("[Wave {}] Enemy spawned at ({:.1f},{:.1f})", mWave, d.pos[0], d.pos[1]);
    }

    void WaveController::Update(float dt)
    {
        // Player 사망 -> GameOver (observer 플래그, IsActive 폴링 대체).
        if (mStageFsm && mPlayerDead)
        {
            mStageFsm->TryTransit(EStageStatus::GameOver);
            return;
        }

        // 첫 웨이브 개시
        if (mWave == 0)
        {
            mWave = 1;
            mSpawnTimer.Reset();
        }

        // 전멸 감지 -> 다음 웨이브 (live 0 + 이번 웨이브 스폰됨). 디졸브 중 corpse(mDying)는 무관 — 즉시 다음 웨이브.
        if (mWave > 0 && mWaveSpawnedAny && LiveCount() == 0)
        {
            ++mWave;
            mWaveSpawnedAny = false;
            mSpawnTimer.Reset();
            spdlog::info("[Wave] All cleared -> Wave {}", mWave);
        }

        mSpawnTimer.Tick(dt);
        if (mSpawnTimer.IsTimesUp() && LiveCount() < WAVE_MAX_ENEMIES)
        {
            mSpawnTimer.Reset();
            SpawnEnemy();
        }
    }
}
