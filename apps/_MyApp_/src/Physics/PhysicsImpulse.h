#ifndef __MYAPP_PHYSICS_IMPULSE_H__
#define __MYAPP_PHYSICS_IMPULSE_H__

#include "Algebraic/Stat.h"
#include "Entity/Components/Components.Interfaces.h"
#include "Physics/PhysicsComponent.h"
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
		    : mImpulseForce(2.5f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce), // 7.5→2.5 (1/3 — 넉백 세기 튜닝)
		      mCooldown(0.8f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed),
		      mActiveTimer(kDurationSec),
		      mCooldownTimer(mCooldown.GetValue())
		{
			// arm-inactive — 생성 직후 finished(비활성). 평소 IsActive=false / 쿨다운 해제. (Reset 으로 발동)
			mActiveTimer.Tick(mActiveTimer.GetBaseTime());
			mCooldownTimer.Tick(mCooldownTimer.GetBaseTime());
		}

		void OnEnter() override { mBody = Components::FindPhysics(GetOwner()); }
		void OnExit() override { mBody = nullptr; }

		void Update(float dt) override
		{
			mActiveTimer.Tick(dt);
			mCooldownTimer.Tick(dt);
		}

		void DoImpulse(vmath::vec2 dir) override
		{
			if (!mCooldownTimer.IsTimesUp() || IsActive()) return;   // 쿨다운 중 or 이미 active → 게이트
			if (!mBody || !mBody->GetBody()) return;
			const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
			if (len <= 0.001f) return;
			vmath::vec2 n(dir[0] / len, dir[1] / len);
			const float force = mImpulseForce.GetValue();
			// XZ → Box2D XY (Z → -Y, spec §4.4)
			mBody->GetBody()->SetLinearVelocity(b2Vec2(n[0] * force, -n[1] * force));
			mActiveTimer.Reset();     // 버스트 창 발동 (0.3s)
			mCooldownTimer.Reset();   // 쿨다운 발동 (0.8s)
		}

		void ApplyKnockback(vmath::vec2 fromXZ) { DoImpulse(fromXZ); } // convenience (Monster Knockback)
		bool IsActive() const { return !mActiveTimer.IsTimesUp(); }    // 버스트 창 진행 중

	private:
		static constexpr float kDurationSec = 0.3f;   // active 창 (plain — 맞는 enum 없음)
		Algebraic::Numeric::Stat mImpulseForce;       // Stat(DashForce, base 7.5)
		Algebraic::Numeric::Stat mCooldown;           // Stat(CoolDownSpeed, base 0.8) — cooldownTimer baseTime 소스
		SJH::Timer::Timer        mActiveTimer;        // 0.3s 버스트 창
		SJH::Timer::Timer        mCooldownTimer;      // 0.8s 재발동 게이트
		Components::Physics*     mBody = nullptr;     // FindPhysics — 비소유
	};
} // namespace TopdownShooter::Physics

#endif // __MYAPP_PHYSICS_IMPULSE_H__
