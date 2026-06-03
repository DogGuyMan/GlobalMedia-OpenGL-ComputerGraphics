#ifndef __TOPDOWNSHOOTER_SPAWNS_CARRIER_H__
#define __TOPDOWNSHOOTER_SPAWNS_CARRIER_H__

#include "Entity/Components/Components.Interfaces.h" // IDamageable / IImpulsable
#include "Physics/Components.Interfaces.h"           // IContactable
#include "scene/actor.h"
#include <functional>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Spawn::Carrier
{
	/// @brief 데미지 배달 공통 베이스 (얇음 — 확정 §6.4). 수명/센서/이동은 subtype 책임.
	class CarrierBase : public SJH::Scene::Component,
	                    public Physics::IContactable
	{
	  public:
		using HitFx = std::function<void(const vmath::vec3 &)>;

		CarrierBase &SetOwnerEntity(SJH::Scene::Actor *o)
		{
			mOwnerEntity = o;
			return *this;
		}
		CarrierBase &SetDamage(int d)
		{
			mDamage = d;
			return *this;
		}
		CarrierBase &SetOnHitFx(HitFx fx)
		{
			mOnHitFx = std::move(fx);
			return *this;
		} // C4 — SetOnHitFx 흡수

	  protected:
		/// @brief C1 배달: target 의 IDamageable->DoDamaged + (있으면) IImpulsable->DoImpulse 넉백 + onHitFx.
		void Deliver(SJH::Scene::Actor *target, vmath::vec2 knockbackDir)
		{
			if (!target || target == mOwnerEntity || !target->IsActive())
				return;
			if (auto *dmg = target->GetComponent<Entity::IDamageable>())
				dmg->DoDamaged(mDamage);
			if (auto *imp = target->GetComponent<Entity::IImpulsable>())
				imp->DoImpulse(knockbackDir);
			if (mOnHitFx)
				mOnHitFx(target->GetTransform().Translate); // spawn-at-point seam 유지
		}

		SJH::Scene::Actor *mOwnerEntity = nullptr; // 발사자 (자가 피해 방지)
		int                mDamage      = 0;
		HitFx              mOnHitFx;
	};

	/// @brief flying + lifetime + self-despawn. BulletContactHandler 흡수 (mAlive 가드 + 지연 despawn).
	class Projectile : public CarrierBase, public Entity::IDieable
	{
	  public:
		explicit Projectile(int damage) { mDamage = damage; }

		/// @brief 비행 방향 주입 (box2d XY, 정규화 가정) — 넉백 방향 소스.
		///        위치차분(enemy-bullet)은 접촉 시 관통 깊이에 따라 불안정/역전되므로 비행방향을 쓴다.
		void SetLaunchDir(vmath::vec2 box2dDir) { mLaunchDir = box2dDir; }

		void OnEnter() override {}
		void OnExit() override {}
		void Update(float /*dt*/) override
		{
			// 콜백 중 b2Body 수정 금지 -> despawn 은 Update 에서 지연 (BulletContactHandler 패턴).
			if (mPendingDisable && GetOwner())
				GetOwner()->SetActive(false);
		}

		void DoDie() override { mPendingDisable = true; }

		void OnCollisionEnter(SJH::Scene::Actor *other) override { HandleHit(other); }
		void OnTriggerEnter(SJH::Scene::Actor *other) override { HandleHit(other); }

	  private:
		void HandleHit(SJH::Scene::Actor *other)
		{
			if (!mAlive || !other || !GetOwner())
				return;
			mAlive = false;
			// 넉백 방향 = 총알 비행 방향(=플레이어->타겟 진행 방향, 안정).
			// box2d XY(mLaunchDir) -> world XZ(x, -y): IImpulsable::DoImpulse 계약이 world XZ in -> box2d 변환.
			// (위치차분(enemy-bullet)은 관통 깊이로 부호가 뒤집혀 "플레이어 쪽 돌진" 버그를 유발 -> 폐기.)
			Deliver(other, vmath::vec2(mLaunchDir[0], -mLaunchDir[1]));
			DoDie();
		}

		vmath::vec2 mLaunchDir{0.0f, 0.0f}; // box2d XY 비행방향 (SetLaunchDir 주입)
		bool        mAlive          = true;
		bool        mPendingDisable = false;
	};

	/// @brief sensor + persistent (적 접촉 데미지). EnemyContactHandler 흡수 — 적 body 재사용(동작 보존).
	class ContactCarrier : public CarrierBase
	{
	  public:
		explicit ContactCarrier(int damage) { mDamage = damage; }

		void OnEnter() override {}
		void OnExit() override {}
		void Update(float /*dt*/) override {}

		void OnCollisionEnter(SJH::Scene::Actor *other) override { Deliver(other, vmath::vec2(0.0f)); }
		void OnTriggerEnter(SJH::Scene::Actor *other) override { Deliver(other, vmath::vec2(0.0f)); }
	};
}; // namespace TopdownShooter::Spawn::Carrier
#endif // __TOPDOWNSHOOTER_SPAWNS_CARRIER_H__
