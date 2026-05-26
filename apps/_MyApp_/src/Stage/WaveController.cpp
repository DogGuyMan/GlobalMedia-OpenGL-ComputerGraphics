#include "Stage/WaveController.h"
#include "Entity/Enemy/enemy_factory.h"
#include <box2d/box2d.h>
#include <cstdlib>
#include <spdlog/spdlog.h>

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

        Entity::Enemy::EnemyConfig cfg;
        cfg.world        = mWorld;
        cfg.pos          = RandomEdgePos();
        cfg.playerTarget = mPlayerActor;
        cfg.hp           = 20 + mWave * 5;
        cfg.speed        = 1.5f + static_cast<float>(mWave) * 0.3f;
        cfg.damage       = 10;

        auto* enemy = mSpawnParent->AddChild(Entity::Enemy::CreateEnemyActor(cfg));
        mEnemies.push_back(enemy);
        spdlog::info("[Wave {}] Enemy spawned at ({:.1f},{:.1f})", mWave, cfg.pos[0], cfg.pos[1]);
    }

    void WaveController::Update(float dt)
    {
        // 첫 웨이브 개시
        if (mWave == 0)
        {
            mWave       = 1;
            mSpawnTimer = 0.0f;
        }

        // 전멸 감지 → 다음 웨이브
        if (mWave > 0 && !mEnemies.empty() && LiveCount() == 0)
        {
            ++mWave;
            mEnemies.clear();
            mSpawnTimer = 0.0f;
            spdlog::info("[Wave] All cleared → Wave {}", mWave);
        }

        mSpawnTimer += dt;
        if (mSpawnTimer >= kSpawnInterval && LiveCount() < kMaxEnemies)
        {
            mSpawnTimer = 0.0f;
            SpawnEnemy();
        }
    }
}
