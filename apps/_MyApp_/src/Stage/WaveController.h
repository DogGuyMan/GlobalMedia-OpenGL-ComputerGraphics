#ifndef __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__
#define __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__

#include "scene/actor.h"
#include "timer/timer.h"   // SJH::Timer::Timer (spawn 간격 — Timer 객체화)
#include <vector>
#include <vmath.h>

class b2World;

namespace TopdownShooter::Stage
{
    class StageStateMachine; // 전방 선언 — Player 사망 시 GameOver 전이 구동 (역참조)

    /// @brief 웨이브 기반 Enemy spawn/despawn 관리.
    /// @details
    ///   - WAVE_SPAWN_INTERVAL 마다 1마리 spawn (최대 WAVE_MAX_ENEMIES 생존).
    ///   - 사망 observer(Life::SetOnDeath)로 live(mEnemies)->dying 이동 (IsActive 폴링 제거).
    ///   - SweepDespawned(Update 밖)가 디졸브 끝(IsDespawnReady) 적을 RemoveChild->OnExit->DestroyBody.
    ///   - live 0(+이번 웨이브 스폰됨) -> mWave++ + 다음 웨이브 즉시 개시.
    class WaveController : public SJH::Scene::Component
    {
      public:
        WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                       SJH::Scene::Actor* playerActor, float arenaHalfExtent);
        ~WaveController() override;

        void OnEnter() override;   // Player Life 에 사망 observer 등록 (GameOver 트리거)
        void OnExit()  override {}
        void Update(float dt) override;

        /// @brief deferred 파괴 sweep — 디졸브 끝(Life::IsDespawnReady) 적을 RemoveChild->OnExit->DestroyBody.
        ///        Director.Update *밖*(main 렌더루프)에서 호출 — 트리 순회 중 RemoveChild 의 iterator 무효화 회피.
        void SweepDespawned();

        // FSM 역참조 주입 (startup) — Player 사망 시 GameOver 전이 구동
        void SetStageStateMachine(StageStateMachine* fsm) { mStageFsm = fsm; }

        // 진행 상태 조회 — ctx->waveCtrl 경유 (HUD/오버레이 등)
        int  WaveLevel()       const { return mWave; }       // 현재 웨이브 레벨 (1~)
        int  AliveEnemyCount() const { return LiveCount(); } // 현재 생존 Enemy 수

      private:
        void        SpawnEnemy();
        void        OnEnemyDeath(SJH::Scene::Actor* e); // 사망 observer 콜백 — live->dying + 즉시 body 비활성
        vmath::vec2 RandomEdgePos() const;
        int         LiveCount() const;

        b2World*           mWorld;
        SJH::Scene::Actor* mSpawnParent;
        SJH::Scene::Actor* mPlayerActor;
        float              mArenaHalfExtent;

        std::vector<SJH::Scene::Actor*> mEnemies;  // 생존(live) — 사망 시 mDying 로 이동 (LiveCount=size, 폴링 없음)
        std::vector<SJH::Scene::Actor*> mDying;    // 사망·디졸브 중 — SweepDespawned 가 IsDespawnReady 후 RemoveChild
        bool  mPlayerDead     = false; // Player 사망 observer 플래그 (Update 가 GameOver 전이)
        bool  mWaveSpawnedAny = false; // 이번 웨이브 1+ 스폰됨 (wave-clear 가드 — 시작 시 오발화 방지)
        int   mWave       = 0;
        SJH::Timer::Timer mSpawnTimer;   // 스폰 간격 누적기 (WAVE_SPAWN_INTERVAL, ctor 초기화)
        int   mSpawnCount = 0; // 누적 스폰 카운터 — ENEMY_FRONT variant 순환용

        StageStateMachine* mStageFsm = nullptr; // 비소유 — game_application 이 소유 (Player 사망->GameOver)
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__
