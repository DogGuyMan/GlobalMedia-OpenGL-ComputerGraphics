#include "Stage/WaveController.h"
#include "Bootstrap/EnemyBuilder.h"
#include "Stage/Constants.h"
#include <box2d/box2d.h>
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
        ++mSpawnCount;
        mEnemies.push_back(enemy);
        spdlog::info("[Wave {}] Enemy spawned at ({:.1f},{:.1f})", mWave, d.pos[0], d.pos[1]);
    }

    void WaveController::Update(float dt)
    {
        // 첫 웨이브 개시
        if (mWave == 0)
        {
            mWave       = 1;
            mSpawnTimer.Reset();
        }

        // 전멸 감지 ->다음 웨이브
        if (mWave > 0 && !mEnemies.empty() && LiveCount() == 0)
        {
            ++mWave;
            mEnemies.clear();
            mSpawnTimer.Reset();
            spdlog::info("[Wave] All cleared ->Wave {}", mWave);
        }

        mSpawnTimer.Tick(dt);
        if (mSpawnTimer.IsTimesUp() && LiveCount() < WAVE_MAX_ENEMIES)
        {
            mSpawnTimer.Reset();
            SpawnEnemy();
        }
    }
}
