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
	/// @brief 범용 물리 속도 버스트 — Player Dash + Monster Knockback.
	/// @details 시간 로직은 SJH::Timer (active/cooldown). 이동자(Controller/AI)는 IsActive() 게이트로 자유이동 skip.
	class Impulse : public SJH::Scene::Component,
	                public Entity::IImpulsable
	{
	public:
		Impulse()
		    : mImpulseForce(IMPULSE_FORCE, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
		      mCooldown(IMPULSE_COOLDOWN, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed)
		{
			// timer 는 OnEnter 에서 중앙 컨테이너에 Register + arm-inactive.
		}

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

		void Update(float /*dt*/) override {}   // tick은 BaseEntity::Update 가 일괄 구동

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

		void ApplyKnockback(vmath::vec2 fromXZ) { DoImpulse(fromXZ); } // convenience (Monster Knockback)
		bool IsActive() const { return mActiveTimer && !mActiveTimer->IsTimesUp(); }   // 버스트 창 진행 중

	private:
		Algebraic::Numeric::Stat mImpulseForce;       // Stat(DashForce, base 7.5)
		Algebraic::Numeric::Stat mCooldown;           // Stat(CoolDownSpeed, base 0.8) — cooldownTimer baseTime 소스
		SJH::Timer::Timer*       mActiveTimer   = nullptr;   // 0.3s 버스트 창 (중앙 컨테이너 핸들, 비소유)
		SJH::Timer::Timer*       mCooldownTimer = nullptr;   // 0.8s 재발동 게이트 (중앙 컨테이너 핸들, 비소유)
		Components::Physics*     mBody = nullptr;     // FindPhysics — 비소유
	};
} // namespace TopdownShooter::Physics

#endif // __MYAPP_PHYSICS_IMPULSE_H__
