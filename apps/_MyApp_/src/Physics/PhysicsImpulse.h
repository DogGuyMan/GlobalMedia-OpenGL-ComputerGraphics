/**
 * @file PhysicsImpulse.h
 * @brief 물리 기반 속도 버스트 Component -- Player Dash / Monster Knockback 범용 Impulse.
 *
 * @details
 *  ### 책임
 *  - @c DoImpulse(dir): dir 을 정규화 후 @c mImpulseForce 크기로 @c b2Body::SetLinearVelocity 설정.
 *    (b2World::Step 잠금과 무관 -- SetLinearVelocity 는 Step 중에도 안전)
 *  - 버스트 창(@c IMPULSE_DURATION) + 쿨다운(@c IMPULSE_COOLDOWN) 을 @c SJH::Timer 로 관리.
 *  - 타이머는 @c BaseEntity::Timers() (MultipleTimer) 에 위탁 -- OnEnter Register / OnExit Unregister.
 *  - @c IsActive(): 버스트 창 진행 중 여부 -> 이동 컨트롤러/AI 가 자유 이동 skip 게이트로 사용.
 *
 *  ### 비-책임
 *  - [X] body 생성/파괴 -- @c CircleBody / @c BoxBody 담당.
 *  - [X] 타이머 tick -- @c BaseEntity::Update 가 일괄 구동 (본 컴포넌트 Update 는 no-op).
 *
 *  ### 정통 매핑
 *  - Unity @c Rigidbody2D::AddForce(Impulse) 변형 (순간 속도 직접 설정 방식).
 *
 * @note @c DoImpulse 내부에서 @c b2Body::SetLinearVelocity 를 호출한다.
 *       이 호출은 ContactListener 콜백(= b2World::Step 잠금 중) 에서도 허용된다
 *       (doc/Box2DAPI.md sec.8 "허용 (상태 변경)" 항목 참조).
 */
#ifndef __MYAPP_PHYSICS_IMPULSE_H__
#define __MYAPP_PHYSICS_IMPULSE_H__

#include "Algebraic/Stat.h"
#include "Entity/Components/Components.Interfaces.h"
#include "Physics/PhysicsComponent.h"
#include "Physics/Constants.h"
#include "Entity/Player/PlayerEntity.h"
#include "Entity/BaseEntity.h"   // Entity::BaseEntity::Timers() (MultipleTimer 등록 위탁)
#include "scene/actor.h"
#include "timer/timer.h"   // SJH::Timer::Timer (header-only)
#include <cmath>

namespace TopdownShooter::Physics
{
	/**
	 * @brief 범용 물리 속도 버스트 Component -- Player Dash + Monster Knockback.
	 * @details
	 *  @c DoImpulse(dir) 호출 시 방향을 정규화해 @c mImpulseForce 크기의 LinearVelocity 를 body 에 적용.
	 *  버스트 창(@c mActiveTimer) 과 쿨다운(@c mCooldownTimer) 은 @c BaseEntity::Timers() 에 위탁.
	 *  이동자(Controller/AI) 는 @c IsActive() 를 게이트로 Impulse 중 자유이동을 skip 한다.
	 */
	class Impulse : public SJH::Scene::Component,
	                public Entity::IImpulsable
	{
	public:
		/// @brief Impulse Component 초기화.
		/// @param impulseForce 버스트 시 적용할 LinearVelocity 크기 (물리 단위/s).
		Impulse(float impulseForce)
		    : mImpulseForce(impulseForce, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
		      mCooldown(IMPULSE_COOLDOWN, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed)
		{
			// timer 는 OnEnter 에서 중앙 컨테이너에 Register + arm-inactive.
		}

		/// @brief owner 의 Physics(body) + BaseEntity::Timers() 를 lookup, 타이머를 등록하고 arm-inactive 처리.
		void OnEnter() override
		{
			mBody = Components::FindPhysics(GetOwner());
			auto* be = GetOwner() ? GetOwner()->GetComponent<Entity::BaseEntity>() : nullptr;
			if (be == nullptr) return;
			mActiveTimer = be->Timers().Register("impulse.active", IMPULSE_DURATION);
			mActiveTimer->Tick(mActiveTimer->GetBaseTime());          // arm-inactive
			mCooldownTimer = be->Timers().Register("impulse.cooldown", mCooldown.GetValue());
			mCooldownTimer->Tick(mCooldownTimer->GetBaseTime());      // arm-inactive
		}
		/// @brief BaseEntity::Timers() 에서 타이머 Unregister + 캐시 포인터 초기화.
		void OnExit() override
		{
			if (auto* be = GetOwner() ? GetOwner()->GetComponent<Entity::BaseEntity>() : nullptr)
			{
				be->Timers().Unregister("impulse.active");
				be->Timers().Unregister("impulse.cooldown");
			}
			mBody = nullptr;
			mActiveTimer = nullptr;
			mCooldownTimer = nullptr;
		}

		/// @brief no-op. tick 은 @c BaseEntity::Update 가 @c MultipleTimer 를 일괄 구동.
		void Update(float /*dt*/) override {}

		/// @brief 방향 @p dir 로 속도 버스트 적용. 쿨다운 중이거나 버스트 창 활성이면 skip.
		/// @details 내부에서 @c b2Body::SetLinearVelocity 를 호출 -- Step 잠금 중에도 안전.
		///          좌표계 변환: XZ dir -> Box2D XY (z -> -y, spec sec.4.4).
		/// @param dir 이동 방향 (임의 크기 허용, 내부 정규화). 길이가 IMPULSE_LENGTH_EPS 이하이면 skip.
		void DoImpulse(vmath::vec2 dir) override
		{
			if (!mCooldownTimer || !mCooldownTimer->IsTimesUp() || IsActive()) return;   // 미등록/쿨다운 중/active -> 게이트
			if (!mBody || !mBody->GetBody()) return;
			const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
			if (len <= IMPULSE_LENGTH_EPS) return;
			vmath::vec2 n(dir[0] / len, dir[1] / len);
			const float force = mImpulseForce.GetValue();
			// XZ -> Box2D XY (Z -> -Y, spec §4.4)
			mBody->GetBody()->SetLinearVelocity(b2Vec2(n[0] * force, -n[1] * force));
			mActiveTimer->Reset();     // 버스트 창 발동 (0.3s)
			mCooldownTimer->Reset();   // 쿨다운 발동 (0.8s)
		}

		/// @brief Monster Knockback 편의 래퍼 -- @c DoImpulse 위임.
		/// @param fromXZ 넉백 방향 (XZ 좌표계, 임의 크기).
		void ApplyKnockback(vmath::vec2 fromXZ) { DoImpulse(fromXZ); }
		/// @brief 버스트 창(@c mActiveTimer) 이 아직 진행 중이면 true.
		///        이동 컨트롤러/AI 가 자유이동 skip 게이트로 사용.
		bool IsActive() const { return mActiveTimer && !mActiveTimer->IsTimesUp(); }

	private:
		Algebraic::Numeric::Stat mImpulseForce;             ///< 버스트 속도 크기 Stat (DashForce, 기본값 7.5).
		Algebraic::Numeric::Stat mCooldown;                 ///< 쿨다운 시간 Stat (CoolDownSpeed, 기본값 0.8s) -- cooldownTimer baseTime 소스.
		SJH::Timer::Timer*       mActiveTimer   = nullptr;  ///< 0.3s 버스트 창 (중앙 컨테이너 핸들, 비소유).
		SJH::Timer::Timer*       mCooldownTimer = nullptr;  ///< 0.8s 재발동 게이트 (중앙 컨테이너 핸들, 비소유).
		Components::Physics*     mBody          = nullptr;  ///< FindPhysics 결과 캐시 (비소유).
	};
} // namespace TopdownShooter::Physics

#endif // __MYAPP_PHYSICS_IMPULSE_H__
