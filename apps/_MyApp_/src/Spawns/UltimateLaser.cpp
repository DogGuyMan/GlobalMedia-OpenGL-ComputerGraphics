/**
 * @file UltimateLaser.cpp
 * @brief UltimateLaser.h 구현 - RotatingHitscanLaser 내부 클래스 + SpawnUltimateLaser 본체.
 *
 * @details
 *  ### 내부 구조
 *  - 익명 네임스페이스 내 @c RotatingHitscanLaser (@c SJH::Scene::Component 상속):
 *    매 프레임 @c Timer::Tick(dt) 후 경과 각도를 계산해 @c RaycastAll 을 수행.
 *    적별 마지막 타격 elapsed 를 @c mLastHit (unordered_map) 에 기록해 틱 간격(ULTIMATE_TICK) 을 강제.
 *    3초(ULTIMATE_DURATION) 경과 시 데미지 루프 중단 - 파괴는 @c AutoDespawnOnFinish 에 위임.
 *  - @c SpawnUltimateLaser 는 fxRoot 아래 Actor 생성 후 데미지+VFX+AutoDespawn 컴포넌트를 순서대로 부착.
 *
 *  ### 좌표계 변환
 *  - OpenGL world 좌표 (x, h, -z) -> Box2D 평면 (x, -z).
 *    @c startBox2d = (centerWorld[0], -centerWorld[2]).
 *
 * @note GL/gl3w.h 를 최상단에 include - EffekseerRendererGL 의 시스템 gl3.h 와 충돌 방지.
 */
#include <GL/gl3w.h> // 반드시 최상단 - VFXSystem.h->EffekseerRendererGL(시스템 gl3.h) <-> resource_registry.h->gl3w.h 충돌 회피.

#include "Spawns/UltimateLaser.h"

#include "Spawns/AutoDespawnOnFinish.h"
#include "Spawns/VfxInstance.h"                       // VFX::GetSpawnFxRoot / GetSpawnVfx
#include "VFX/VFXSystem.h"                            // VFXSystem::GetManager (EffekseerPlayable ctor)
#include "VFX/EffekseerPlayable.h"                    // EffekseerPlayable + Spin/MaxDuration setter
#include "Physics/PhysicsRaycast.h"                   // RaycastAll
#include "Physics/PhysicsLayer.h"                     // PhysicsLayer::Enemy
#include "Contracts/EntityContracts.h"  // IDamageable
#include "Entity/Constants.h"                         // ULTIMATE_*
#include "resource_registry/resource_registry.h"      // FindEffect("laser")
#include "scene/actor.h"
#include "timer/timer.h"

#include <box2d/box2d.h>
#include <cmath>
#include <memory>
#include <unordered_map>

namespace TopdownShooter::Spawns
{
	namespace
	{
		/**
		 * @brief 회전 히트스캔 데미지 컴포넌트 - 매 프레임 현재 각도로 RaycastAll + 적별 틱 DoDamaged.
		 * @details
		 *  - @c ULTIMATE_DURATION(3초) 동안 경과 각도(startAngle + ROT_PER_SEC * elapsed) 방향으로 레이캐스트.
		 *  - 적별 @c mLastHit 에 elapsed 를 기록해 @c ULTIMATE_TICK 간격 내 중복 타격을 억제 (빔 DoT 패턴).
		 *  - 3초 경과 시 데미지 루프 중단. 파괴는 @c EffekseerPlayable + @c AutoDespawnOnFinish 에 위임.
		 *  - @c SJH::Timer::Timer 자가 보유 (BulletLifetime 패턴 - @c BaseEntity 비상속 단발 Actor 용).
		 */
		class RotatingHitscanLaser : public SJH::Scene::Component
		{
		  public:
			/// @brief 회전 히트스캔 레이저 컴포넌트 초기화.
			/// @param world      Box2D 물리 월드 (RaycastAll 대상).
			/// @param startBox2d 레이저 발원점 (box2d 좌표 - OpenGL (x,-z) 변환된 값).
			/// @param startAngle 시작 회전각 (box2d 라디안).
			/// @param damage     틱당 데미지.
			/// @param range      레이캐스트 최대 거리.
			/// @param ignore     레이캐스트 무시 Actor (발사 주체 자해 방지). nullptr 허용.
			RotatingHitscanLaser(b2World *world, glm::vec2 startBox2d, float startAngle,
			                     int damage, float range, SJH::Scene::Actor *ignore)
			    : mWorld(world), mStart(startBox2d), mStartAngle(startAngle),
			      mRange(range), mDamage(damage), mIgnore(ignore),
			      mTimer(Entity::ULTIMATE_DURATION)
			{
			}

			void OnEnter() override {}
			void OnExit() override {}
			void Update(float dt) override
			{
				if (mWorld == nullptr) return;
				mTimer.Tick(dt);
				if (mTimer.IsTimesUp()) return; // 3초 경과 - 데미지 중단 (파괴는 AutoDespawnOnFinish)

				const float       elapsed = mTimer.GetPassedTime();
				const float       theta   = mStartAngle + Entity::ULTIMATE_ROT_PER_SEC * elapsed;
				const glm::vec2 dir(std::cos(theta), std::sin(theta));

				auto hits = Physics::RaycastAll(*mWorld, mStart, dir, mRange,
				                                Physics::PhysicsLayer::Enemy, mIgnore);
				for (auto &h : hits)
				{
					if (h.actor == nullptr) continue;
					// 적별 틱 - 마지막 타격 후 ULTIMATE_TICK 경과 시만 (빔에 머무는 동안 DoT, 매프레임 폭증 방지).
					float &last = mLastHit[h.actor];
					if (last > 0.0f && (elapsed - last) < Entity::ULTIMATE_TICK) continue;
					last = elapsed;
					if (auto *dmg = h.actor->GetComponent<Entity::IDamageable>())
						dmg->DoDamaged(mDamage);
				}
			}

		  private:
			b2World           *mWorld;       ///< Box2D 물리 월드 - RaycastAll 레이캐스트 대상.
			glm::vec2        mStart;       ///< box2d 시작점(플레이어). OpenGL (x,-z) 로 변환된 값.
			float              mStartAngle;  ///< box2d 시작 각도(라디안). 매 프레임 elapsed * ROT_PER_SEC 누적.
			float              mRange;       ///< 레이캐스트 최대 거리 (ULTIMATE_RANGE 상수).
			int                mDamage;      ///< 틱당 데미지 (ULTIMATE_DAMAGE 상수).
			SJH::Scene::Actor *mIgnore;      ///< 레이캐스트 ignore 대상 (플레이어 자신 - 자해 방지).
			SJH::Timer::Timer  mTimer;       ///< 3초 수명 타이머 (BulletLifetime 패턴). IsTimesUp 후 데미지 중단.
			std::unordered_map<SJH::Scene::Actor *, float> mLastHit; ///< 적별 마지막 타격 elapsed - 틱 간격(ULTIMATE_TICK) 강제.
		};
	} // namespace

	void SpawnUltimateLaser(b2World *world, const glm::vec3 &centerWorld,
	                        float startAngleBox2d, SJH::Scene::Actor *shooter)
	{
		if (world == nullptr) return;
		auto *fxRoot = VFX::GetSpawnFxRoot();
		if (fxRoot == nullptr) return; // context 미등록 - no-op

		auto *a                     = fxRoot->AddChild(std::make_unique<SJH::Scene::Actor>("UltimateLaser"));
		a->GetTransform().Translate = centerWorld;

		// 데미지 - box2d 회전 히트스캔 (RaycastAll Enemy, 적별 0.2s 틱). world=(x,h,-z) -> box2d=(x,-z).
		const glm::vec2 startBox2d(centerWorld[0], -centerWorld[2]);
		a->AddComponent<RotatingHitscanLaser>(world, startBox2d, startAngleBox2d,
		                                      Entity::ULTIMATE_DAMAGE, Entity::ULTIMATE_RANGE, shooter);

		// 시각 - 회전 laser VFX (3초 자동종료 -> AutoDespawnOnFinish -> main 의 SweepFinishedChildren 파괴).
		auto *vfx     = VFX::GetSpawnVfx();
		auto *laserFx = SJH::ResourceRegistry::Get().FindEffect("laser");
		if (vfx != nullptr && laserFx != nullptr)
		{
			auto *efk = a->AddComponent<VFX::EffekseerPlayable>(
			    vfx->GetManager(), laserFx, centerWorld, VFX::TrackPolicy::Static, startAngleBox2d);
			efk->SetIsLoop(true);                                 // laser.efk 가 3초보다 짧으면 반복
			efk->SetSpinRadPerSec(Entity::ULTIMATE_ROT_PER_SEC);  // 1초당 1회전 (데미지 각도와 동기)
			efk->SetMaxDurationSec(Entity::ULTIMATE_DURATION);    // 3초 후 강제 종료 + finished
			efk->Play();
			a->AddComponent<AutoDespawnOnFinish>(efk);
		}
	}
} // namespace TopdownShooter::Spawns
