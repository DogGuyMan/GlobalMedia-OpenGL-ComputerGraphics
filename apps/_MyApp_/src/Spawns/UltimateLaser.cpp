#include <GL/gl3w.h> // 반드시 최상단 — VFXSystem.h→EffekseerRendererGL(시스템 gl3.h) ↔ resource_registry.h→gl3w.h 충돌 회피.

#include "Spawns/UltimateLaser.h"

#include "Spawns/AutoDespawnOnFinish.h"
#include "Spawns/VfxInstance.h"                       // VFX::GetSpawnFxRoot / GetSpawnVfx
#include "VFX/VFXSystem.h"                            // VFXSystem::GetManager (EffekseerPlayable ctor)
#include "VFX/EffekseerPlayable.h"                    // EffekseerPlayable + Spin/MaxDuration setter
#include "Physics/PhysicsRaycast.h"                   // RaycastAll
#include "Physics/PhysicsLayer.h"                     // PhysicsLayer::Enemy
#include "Entity/Components/Components.Interfaces.h"  // IDamageable
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
		/// @brief 회전 히트스캔 데미지 — 매 프레임 현재 각도로 RaycastAll + 적별 틱 DoDamaged. 3초 후 데미지 중단.
		///        Timer 자가보유 (BulletLifetime 패턴 — 비-BaseEntity 단발 Actor). 파괴는 EffekseerPlayable+AutoDespawn 담당.
		class RotatingHitscanLaser : public SJH::Scene::Component
		{
		  public:
			RotatingHitscanLaser(b2World *world, vmath::vec2 startBox2d, float startAngle,
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
				if (mTimer.IsTimesUp()) return; // 3초 경과 — 데미지 중단 (파괴는 AutoDespawnOnFinish)

				const float       elapsed = mTimer.GetPassedTime();
				const float       theta   = mStartAngle + Entity::ULTIMATE_ROT_PER_SEC * elapsed;
				const vmath::vec2 dir(std::cos(theta), std::sin(theta));

				auto hits = Physics::RaycastAll(*mWorld, mStart, dir, mRange,
				                                Physics::PhysicsLayer::Enemy, mIgnore);
				for (auto &h : hits)
				{
					if (h.actor == nullptr) continue;
					// 적별 틱 — 마지막 타격 후 ULTIMATE_TICK 경과 시만 (빔에 머무는 동안 DoT, 매프레임 폭증 방지).
					float &last = mLastHit[h.actor];
					if (last > 0.0f && (elapsed - last) < Entity::ULTIMATE_TICK) continue;
					last = elapsed;
					if (auto *dmg = h.actor->GetComponent<Entity::IDamageable>())
						dmg->DoDamaged(mDamage);
				}
			}

		  private:
			b2World           *mWorld;
			vmath::vec2        mStart;      // box2d 시작점(플레이어)
			float              mStartAngle; // box2d 시작 각도(라디안)
			float              mRange;
			int                mDamage;
			SJH::Scene::Actor *mIgnore;
			SJH::Timer::Timer  mTimer;      // 3초 (BulletLifetime 패턴)
			std::unordered_map<SJH::Scene::Actor *, float> mLastHit; // 적별 마지막 타격 elapsed
		};
	} // namespace

	void SpawnUltimateLaser(b2World *world, const vmath::vec3 &centerWorld,
	                        float startAngleBox2d, SJH::Scene::Actor *shooter)
	{
		if (world == nullptr) return;
		auto *fxRoot = VFX::GetSpawnFxRoot();
		if (fxRoot == nullptr) return; // context 미등록 — no-op

		auto *a                     = fxRoot->AddChild(std::make_unique<SJH::Scene::Actor>("UltimateLaser"));
		a->GetTransform().Translate = centerWorld;

		// 데미지 — box2d 회전 히트스캔 (RaycastAll Enemy, 적별 0.2s 틱). world=(x,h,-z) → box2d=(x,-z).
		const vmath::vec2 startBox2d(centerWorld[0], -centerWorld[2]);
		a->AddComponent<RotatingHitscanLaser>(world, startBox2d, startAngleBox2d,
		                                      Entity::ULTIMATE_DAMAGE, Entity::ULTIMATE_RANGE, shooter);

		// 시각 — 회전 laser VFX (3초 자동종료 → AutoDespawnOnFinish → main 의 SweepFinishedChildren 파괴).
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
