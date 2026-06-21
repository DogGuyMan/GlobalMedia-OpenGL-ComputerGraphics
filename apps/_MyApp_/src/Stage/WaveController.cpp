/**
 * @file WaveController.cpp
 * @brief @c WaveController 구현 - 웨이브 스폰 / 사망 observer / deferred sweep.
 *
 * @details
 *  ### 구현 전략
 *  - 스폰: @c mSpawnTimer(@c SJH::Timer::Timer) tick -> @c IsTimesUp() + 생존 < MAX 조건 시 @c SpawnEnemy().
 *  - 사망 통지: @c OnEnemyDeath (observer 콜백) 는 enqueue(@c mEnemies -> @c mDying 이동) 만 수행.
 *    Box2D @c b2World::Step 잠금 중(contact 콜백 경유 @c DoDie) 호출될 수 있으므로
 *    body 변경(@c SetBodyEnabled / @c DestroyBody) 은 Step 밖(@c SweepDespawned) 에서만 실행.
 *  - Sweep: @c SweepDespawned 는 main 렌더루프가 @c b2World::Step 종료 후 명시적 호출.
 *    dying 목록을 순회해 디졸브 완료(@c IsDespawnReady) 시 @c RemoveChild (OnExit 자동 호출 -> DestroyBody),
 *    아직 디졸브 중이면 @c SetBodyEnabled(false) 로 충돌만 정지(idempotent).
 *
 *  ### 웨이브 클리어 로직
 *  - 조건: @c mWave > 0 && @c mWaveSpawnedAny && @c LiveCount() == 0
 *  - @c mDying(디졸브 중 corpse) 는 wave-clear 판정에 무관 - 즉시 다음 웨이브 개시.
 *  - @c mWaveSpawnedAny 가드: 웨이브 시작 직후 아직 스폰 전 프레임에서의 오발화 방지.
 *
 * @note @c SweepDespawned 는 반드시 @c Director::Update 밖에서 호출.
 *       Update 내부(Actor 트리 순회 중) @c RemoveChild 호출 시 iterator 무효화 위험.
 */
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
        // Player 사망 observer 등록 - 사망(HP0) 시 플래그 set, Update 가 GameOver 전이 (IsActive 폴링 대체).
        // (Player 는 제거하지 않음 - 시체/물리 유지. enemy 만 SweepDespawned 로 완전 제거.)
        if (mPlayerActor)
            if (auto* life = mPlayerActor->GetComponent<Entity::Components::Life>())
                life->SetOnDeath([this](SJH::Scene::Actor*) { mPlayerDead = true; });
    }

    // 사망 통지(observer) - live(mEnemies)->dying 이동만 (enqueue). [!] Box2D body 변경(SetEnabled/DestroyBody)은
    // b2World::Step 잠금 중(여기 = contact 콜백 경유 DoDie) 금지 - 실제 body 비활성/파괴는 SweepDespawned(Step 밖) 담당.
    void WaveController::OnEnemyDeath(SJH::Scene::Actor* e)
    {
        if (e == nullptr) return;
        mEnemies.erase(std::remove(mEnemies.begin(), mEnemies.end(), e), mEnemies.end());
        mDying.push_back(e);
    }

    void WaveController::SweepDespawned()
    {
        // main 이 b2World::Step 끝난 뒤(잠금 해제) 호출 - 여기서만 body 변경 안전.
        // 각 dying 적: (1) 디졸브 중이면 body 비활성(충돌 정지, idempotent) (2) 디졸브 끝(IsDespawnReady)이면 완전 제거.
        for (auto it = mDying.begin(); it != mDying.end();)
        {
            SJH::Scene::Actor* e    = *it;
            auto*              life = e ? e->GetComponent<Entity::Components::Life>() : nullptr;
            if (e == nullptr || life == nullptr || life->IsDespawnReady())
            {
                if (e && mSpawnParent) mSpawnParent->RemoveChild(e); // OnExit -> Physics::OnExit::DestroyBody
                it = mDying.erase(it);
            }
            else
            {
                if (auto* phys = Physics::Components::FindPhysics(e))
                    phys->SetBodyEnabled(false); // 디졸브 동안 충돌 정지 (Step 밖 = 잠금 해제 = 안전)
                ++it;
            }
        }
    }

    int WaveController::LiveCount() const
    {
        return static_cast<int>(mEnemies.size()); // mEnemies = 생존만 (사망 즉시 dying 이동) - 폴링 없음
    }

    glm::vec2 WaveController::RandomEdgePos() const
    {
        const float h    = mArenaHalfExtent - 0.5f;
        const int   side = rand() % 4;
        const float t    = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f * h - h;
        switch (side)
        {
        case 0:  return glm::vec2(-h,  t);
        case 1:  return glm::vec2( h,  t);
        case 2:  return glm::vec2( t, -h);
        default: return glm::vec2( t,  h);
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
        // 사망 observer 주입 - WaveController 가 적 컴포넌트 init-time 배선 소유 (Stage->Entity inward).
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

        // 전멸 감지 -> 다음 웨이브 (live 0 + 이번 웨이브 스폰됨). 디졸브 중 corpse(mDying)는 무관 - 즉시 다음 웨이브.
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
