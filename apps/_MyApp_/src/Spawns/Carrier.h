/**
 * @file Carrier.h
 * @brief 데미지 배달(Carrier) 컴포넌트 계열 - 접촉 시 대상에 피해/넉백/HitFx 를 전달하는 베이스 + 2종 concrete.
 *
 * @details
 *  ### 책임
 *  - @c CarrierBase : 접촉 대상에게 데미지 배달(@c Deliver) 하는 얇은 공통 베이스. Owner/Damage/HitFx 만 보유.
 *  - @c Projectile : 날아가는 투사체 + 수명 + self-despawn. 1회 접촉 후 소멸 (총알).
 *  - @c ContactCarrier : 센서 + 지속(persistent) 접촉 데미지. 적 body 에 부착되어 플레이어와 닿을 때마다 데미지.
 *
 *  ### 비-책임
 *  - [X] 물리 body 생성/이동 - Physics 컴포넌트(BoxBody/CircleBody) 가 담당.
 *  - [X] Actor lifetime 직접 제어 - despawn 은 @c SetActive(false) 마킹 후 상위 sweep 이 정리.
 *
 *  ### 정통 매핑
 *  - 한 Carrier 가 "데미지 데이터 + 전달 규칙" 만 들고 다니는 작은 운반자 - god-component 분해(확정 sec.6.4) 결과.
 *
 * @note Box2D Step(ContactListener 콜백) 중 b2Body 수정 금지 - despawn 은 @c Update 에서 지연 처리한다.
 */
#ifndef __TOPDOWNSHOOTER_SPAWNS_CARRIER_H__
#define __TOPDOWNSHOOTER_SPAWNS_CARRIER_H__

#include "Contracts/EntityContracts.h" // IDamageable / IImpulsable / ILivable
#include "Physics/Components.Interfaces.h"           // IContactable
#include "scene/actor.h"
#include <functional>
#include <utility>
#include <glm/glm.hpp>

namespace TopdownShooter::Spawn::Carrier
{
	/**
	 * @brief 데미지 배달 공통 베이스 (얇음 - 확정 sec.6.4). 수명/센서/이동은 subtype 책임.
	 * @details Owner(발사자) / Damage(피해량) / OnHitFx(타격 콜백) 만 보유하고, 접촉 시 대상에게
	 *          피해+넉백+HitFx 를 전달하는 @c Deliver 만 protected 로 제공한다. 접촉 감지(@c IContactable)
	 *          콜백 구현은 subtype(@c Projectile / @c ContactCarrier) 의 책임.
	 */
	class CarrierBase : public SJH::Scene::Component,
	                    public Physics::IContactable
	{
	  public:
		/// @brief 타격 지점(world 좌표)을 받아 추가 연출(스파크 FX 등)을 트리거하는 콜백 시그니처.
		using HitFx = std::function<void(const glm::vec3 &)>;

		/// @brief 발사 주체 지정 (Fluent). 자가 피해 방지에 사용 - @c Deliver 가 target==owner 면 무시.
		/// @param o 발사자 Actor.
		/// @return 체이닝용 자기 참조.
		CarrierBase &SetOwnerEntity(SJH::Scene::Actor *o)
		{
			mOwnerEntity = o;
			return *this;
		}
		/// @brief 피해량 지정 (Fluent).
		/// @param d 데미지 값.
		/// @return 체이닝용 자기 참조.
		CarrierBase &SetDamage(int d)
		{
			mDamage = d;
			return *this;
		}
		/// @brief 타격 시 호출할 연출 콜백 지정 (Fluent). C4 - SetOnHitFx 흡수.
		/// @param fx 타격 지점을 받는 HitFx (소유권 이동).
		/// @return 체이닝용 자기 참조.
		CarrierBase &SetOnHitFx(HitFx fx)
		{
			mOnHitFx = std::move(fx);
			return *this;
		} // C4 - SetOnHitFx 흡수

	  protected:
		/// @brief C1 배달: target 의 IDamageable->DoDamaged + (있으면) IImpulsable->DoImpulse 넉백 + onHitFx.
		/// @details target 이 null/자기자신/비활성이면 무시. HitFx 는 target 위치(spawn-at-point seam)로 호출.
		/// @param target        피해 대상 Actor.
		/// @param knockbackDir  넉백 방향 (world XZ in - IImpulsable 계약이 box2d 로 변환).
		void Deliver(SJH::Scene::Actor *target, glm::vec2 knockbackDir)
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

		SJH::Scene::Actor *mOwnerEntity = nullptr; ///< 발사자 (자가 피해 방지).
		int mDamage = 0;                           ///< 전달할 피해량.
		HitFx mOnHitFx;                            ///< 타격 시 호출할 연출 콜백 (없으면 미호출).
	};

	/**
	 * @brief 비행 + 수명 + self-despawn 투사체. BulletContactHandler 흡수 (mAlive 가드 + 지연 despawn).
	 * @details 1회 접촉(@c OnCollisionEnter / @c OnTriggerEnter) 시 @c Deliver 후 즉시 @c DoDie 로 소멸 예약.
	 *          넉백 방향은 위치차분이 아니라 *비행 방향*(@c mLaunchDir) 을 쓴다 (관통 깊이로 부호 뒤집힘 회피).
	 */
	class Projectile : public CarrierBase, public Entity::IDieable
	{
	  public:
		/// @brief 피해량을 받는 생성자.
		/// @param damage 접촉 시 전달할 데미지.
		explicit Projectile(int damage)
		{
			mDamage = damage;
		}

		/// @brief 비행 방향 주입 (box2d XY, 정규화 가정) - 넉백 방향 소스.
		///        위치차분(enemy-bullet)은 접촉 시 관통 깊이에 따라 불안정/역전되므로 비행방향을 쓴다.
		/// @param box2dDir box2d 평면 비행 방향.
		void SetLaunchDir(glm::vec2 box2dDir)
		{
			mLaunchDir = box2dDir;
		}

		/// @brief Component 진입 hook (no-op).
		void OnEnter() override
		{
		}
		/// @brief Component 이탈 hook (no-op).
		void OnExit() override
		{
		}
		/// @brief 지연 despawn 처리 - mPendingDisable 마킹 시 소유 Actor 비활성화.
		/// @details Box2D 콜백 중 b2Body 수정 금지 -> despawn 을 Update 로 지연 (BulletContactHandler 패턴).
		void Update(float /*dt*/) override
		{
			// 콜백 중 b2Body 수정 금지 -> despawn 은 Update 에서 지연 (BulletContactHandler 패턴).
			if (mPendingDisable && GetOwner())
				GetOwner()->SetActive(false);
		}

		/// @brief 소멸 예약 - 다음 Update 에서 Actor 가 비활성화된다. IDieable 구현.
		void DoDie() override
		{
			mPendingDisable = true;
		}

		/// @brief 물리 충돌 진입 시 타격 처리. IContactable 구현.
		/// @param other 충돌 상대 Actor.
		void OnCollisionEnter(SJH::Scene::Actor *other) override
		{
			HandleHit(other);
		}
		/// @brief 센서 트리거 진입 시 타격 처리. IContactable 구현.
		/// @param other 트리거 상대 Actor.
		void OnTriggerEnter(SJH::Scene::Actor *other) override
		{
			HandleHit(other);
		}

	  private:
		/// @brief 1회성 타격 - mAlive 가드 후 Deliver(넉백=비행방향) + DoDie.
		/// @param other 타격 대상.
		void HandleHit(SJH::Scene::Actor *other)
		{
			if (!mAlive || !other || !GetOwner())
				return;
			mAlive = false;
			// 넉백 방향 = 총알 비행 방향(=플레이어->타겟 진행 방향, 안정).
			// box2d XY(mLaunchDir) -> world XZ(x, -y): IImpulsable::DoImpulse 계약이 world XZ in -> box2d 변환.
			// (위치차분(enemy-bullet)은 관통 깊이로 부호가 뒤집혀 "플레이어 쪽 돌진" 버그를 유발 -> 폐기.)
			Deliver(other, glm::vec2(mLaunchDir[0], -mLaunchDir[1]));
			DoDie();
		}

		glm::vec2 mLaunchDir{0.0f, 0.0f}; // box2d XY 비행방향 (SetLaunchDir 주입)
		bool mAlive = true;
		bool mPendingDisable = false;
	};

	/// @brief sensor + persistent (적 접촉 데미지). EnemyContactHandler 흡수 - 적 body 재사용(동작 보존).
	class ContactCarrier : public CarrierBase
	{
	  public:
		explicit ContactCarrier(int damage)
		{
			mDamage = damage;
		}

		void OnEnter() override
		{
		}
		void OnExit() override
		{
		}
		void Update(float /*dt*/) override
		{
		}

		void OnCollisionEnter(SJH::Scene::Actor *other) override
		{
			if (mOwnerEntity != nullptr && !mOwnerEntity->GetComponent<Entity::ILivable>()->IsAlive())
				return;
			Deliver(other, glm::vec2(0.0f));
		}
		void OnTriggerEnter(SJH::Scene::Actor *other) override
		{
			if (mOwnerEntity != nullptr && !mOwnerEntity->GetComponent<Entity::ILivable>()->IsAlive())
				return;
			Deliver(other, glm::vec2(0.0f));
		}
	};
}; // namespace TopdownShooter::Spawn::Carrier
#endif // __TOPDOWNSHOOTER_SPAWNS_CARRIER_H__
