#ifndef __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__
#define __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__

#include "scene/actor.h"
#include "timer/timer.h"   // SJH::Timer::Timer (spawn 간격 — Timer 객체화)
#include <vector>
#include <vmath.h>

class b2World;

namespace TopdownShooter::Stage
{
    /// @brief 웨이브 기반 Enemy spawn 관리.
    /// @details
    ///   - WAVE_SPAWN_INTERVAL 마다 1마리 spawn (최대 WAVE_MAX_ENEMIES 동시 생존).
    ///   - mEnemies raw ptr 추적 — IsActive()==false 시 전멸 감지.
    ///   - 전멸 ->mWave++ + 다음 웨이브 즉시 개시.
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
        void        SpawnEnemy();
        vmath::vec2 RandomEdgePos() const;
        int         LiveCount() const;

        b2World*           mWorld;
        SJH::Scene::Actor* mSpawnParent;
        SJH::Scene::Actor* mPlayerActor;
        float              mArenaHalfExtent;

        std::vector<SJH::Scene::Actor*> mEnemies;
        int   mWave       = 0;
        SJH::Timer::Timer mSpawnTimer;   // 스폰 간격 누적기 (WAVE_SPAWN_INTERVAL, ctor 초기화)
        int   mSpawnCount = 0; // 누적 스폰 카운터 — ENEMY_FRONT variant 순환용
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__
