/**
 * @file WaveController.h
 * @brief 웨이브 기반 Enemy 스폰/디스폰 관리 컴포넌트 @c WaveController 선언.
 *
 * @details
 *  ### 책임
 *  - @c WAVE_SPAWN_INTERVAL 마다 1마리 Enemy 스폰 (동시 생존 최대 @c WAVE_MAX_ENEMIES).
 *  - @c Life::SetOnDeath observer 로 live(@c mEnemies) -> dying(@c mDying) 이동 (IsActive 폴링 제거).
 *  - @c SweepDespawned 가 디졸브 완료(@c Life::IsDespawnReady) 적을
 *    @c RemoveChild -> @c OnExit -> @c DestroyBody 순으로 완전 제거.
 *  - 생존 0 + 이번 웨이브 스폰됨 조건 달성 시 @c mWave 증가 + 다음 웨이브 즉시 개시.
 *  - Player 사망(@c mPlayerDead 플래그) 감지 시 @c StageStateMachine::TryTransit(GameOver) 위임.
 *
 *  ### 비-책임
 *  - [X] Enemy Actor 조립 세부 - @c Bootstrap::BuildEnemy 팩토리 위임.
 *  - [X] Box2D body 생성/파괴 - @c Physics::Components 위임 (@c OnExit 자동 처리).
 *  - [X] FSM 전이 소유 - @c StageStateMachine 비소유 포인터(@c mStageFsm) 경유 호출.
 *
 *  ### Box2D Step 잠금 계약 (중요)
 *  @c OnEnemyDeath 는 @c b2World::Step 잠금 중(contact 콜백 경유)에 호출될 수 있다.
 *  따라서 @c OnEnemyDeath 에서는 enqueue(@c mDying 이동) 만 수행하고,
 *  실제 @c SetBodyEnabled / @c DestroyBody 는 Step 밖(@c SweepDespawned) 에서만 실행.
 *  -- doc/Box2DAPI.md sec.8 "Step 잠금 함정" 참조.
 *
 *  ### 정통 매핑
 *  - Unreal @c AGameMode::OnPawnKilled + Wave spawn timer 조합.
 *  - Cocos2D @c GameScene Update spawn loop 패턴.
 *
 * @note @c SweepDespawned 는 @c Director::Update 밖(main 렌더루프)에서 호출해야 한다.
 *       Actor 트리 순회 중 @c RemoveChild 호출 시 iterator 무효화 위험.
 */
#ifndef __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__
#define __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__

#include "scene/actor.h"
#include "timer/timer.h"   // SJH::Timer::Timer (spawn 간격 - Timer 객체화)
#include <vector>
#include <glm/glm.hpp>

class b2World;

namespace TopdownShooter::Stage
{
    class StageStateMachine; // 전방 선언 - Player 사망 시 GameOver 전이 구동 (역참조)

    /**
     * @brief 웨이브 기반 Enemy spawn/despawn 관리 컴포넌트.
     * @details
     *  per-frame 갱신 흐름:
     *  1. @c mPlayerDead 플래그 확인 -> true 이면 @c mStageFsm->TryTransit(GameOver) 후 조기 반환.
     *  2. 첫 프레임(@c mWave == 0) 에 웨이브 1 개시.
     *  3. 생존 0 + 스폰됨 조건 -> @c mWave++ + 타이머 리셋.
     *  4. @c mSpawnTimer.IsTimesUp() + 생존 < MAX -> @c SpawnEnemy().
     *
     *  deferred 파괴 흐름:
     *  - @c OnEnemyDeath(observer): @c mEnemies 에서 제거 + @c mDying 에 추가(enqueue 만).
     *  - @c SweepDespawned(Step 밖): dying 목록 순회 -> 디졸브 완료면 @c RemoveChild,
     *    아직 디졸브 중이면 body 비활성(@c SetBodyEnabled(false)).
     */
    class WaveController : public SJH::Scene::Component
    {
      public:
        /// @brief 생성자 - 필수 의존 주입.
        /// @param world           Box2D 월드 (Enemy 물리 생성 위탁).
        /// @param spawnParent     Enemy Actor 를 AddChild 할 부모 Actor.
        /// @param playerActor     Player Actor 비소유 포인터 (사망 observer 등록 대상).
        /// @param arenaHalfExtent 아레나 반-크기 (스폰 위치 랜덤 계산용).
        WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                       SJH::Scene::Actor* playerActor, float arenaHalfExtent);
        ~WaveController() override;

        /// @brief Player @c Life 에 사망 observer 등록 - 사망 시 @c mPlayerDead = true 플래그 set.
        void OnEnter() override;
        void OnExit()  override {}

        /// @brief per-frame 갱신 - Player 사망 감지 / 웨이브 진행 / 스폰 타이머 tick.
        /// @param dt 프레임 delta time(초).
        void Update(float dt) override;

        /// @brief deferred 파괴 sweep - 디졸브 끝(Life::IsDespawnReady) 적을 RemoveChild->OnExit->DestroyBody.
        ///        Director.Update *밖*(main 렌더루프)에서 호출 - 트리 순회 중 RemoveChild 의 iterator 무효화 회피.
        void SweepDespawned();

        /// @brief FSM 역참조 주입 (startup) - Player 사망 시 GameOver 전이 구동.
        /// @param fsm @c StageStateMachine 비소유 포인터 (@c game_application 이 소유).
        void SetStageStateMachine(StageStateMachine* fsm) { mStageFsm = fsm; }

        /// @brief 현재 웨이브 레벨 조회 (1부터 시작).
        /// @return 현재 웨이브 번호. 게임 시작 전(Update 미호출)이면 0.
        int  WaveLevel()       const { return mWave; }

        /// @brief 현재 생존 Enemy 수 조회.
        /// @return @c mEnemies.size() - 사망 즉시 @c mDying 으로 이동하므로 폴링 없이 정확.
        int  AliveEnemyCount() const { return LiveCount(); }

      private:
        /// @brief 웨이브 파라미터(HP/속도/데미지)를 적용해 Enemy 1마리 스폰 + observer 배선.
        void        SpawnEnemy();

        /// @brief 사망 observer 콜백 - @c mEnemies 에서 제거 후 @c mDying 에 enqueue.
        /// @details Box2D Step 잠금 중 호출될 수 있으므로 body 변경 금지 (enqueue 전용).
        /// @param e 사망한 Enemy @c Actor 포인터.
        void        OnEnemyDeath(SJH::Scene::Actor* e);

        /// @brief 아레나 4변 중 랜덤 가장자리 좌표 반환 (스폰 위치).
        /// @return 아레나 경계 근방 2D 위치.
        glm::vec2 RandomEdgePos() const;

        /// @brief @c mEnemies 크기를 @c int 로 반환 (생존 수).
        int         LiveCount() const;

        b2World*           mWorld;          ///< Box2D 월드 (비소유). Enemy 물리 바디 생성 위탁.
        SJH::Scene::Actor* mSpawnParent;    ///< Enemy @c Actor 를 AddChild 할 부모 (비소유).
        SJH::Scene::Actor* mPlayerActor;    ///< Player @c Actor 비소유 포인터 - 사망 observer 등록 대상.
        float              mArenaHalfExtent; ///< 아레나 반-크기(월드 단위) - @c RandomEdgePos 계산 기준.

        std::vector<SJH::Scene::Actor*> mEnemies;  ///< 생존(live) Enemy 목록. 사망 시 @c mDying 으로 이동.
        std::vector<SJH::Scene::Actor*> mDying;    ///< 사망/디졸브 중 Enemy 목록. @c SweepDespawned 가 처리.
        bool  mPlayerDead     = false;  ///< Player 사망 observer 플래그 - @c Update 가 GameOver 전이 트리거.
        bool  mWaveSpawnedAny = false;  ///< 이번 웨이브 1+ 스폰 여부 - wave-clear 조기 오발화 방지 가드.
        int   mWave           = 0;      ///< 현재 웨이브 레벨 (1~). @c Update 첫 호출 시 1로 초기화.
        SJH::Timer::Timer mSpawnTimer;  ///< 스폰 간격 타이머 (@c WAVE_SPAWN_INTERVAL 주기, ctor 초기화).
        int   mSpawnCount     = 0;      ///< 누적 스폰 카운터 - Enemy variant(3종) 순환 인덱스.

        StageStateMachine* mStageFsm = nullptr; ///< @c StageStateMachine 비소유 포인터 - Player 사망 시 GameOver 위임.
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_WAVE_CONTROLLER_H__
