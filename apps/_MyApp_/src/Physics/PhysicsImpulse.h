#ifndef __MYAPP_PHYSICS_IMPULSE_H__
#define __MYAPP_PHYSICS_IMPULSE_H__

#include "Algebraic/Stat.h"
#include "Entity/Components/Components.Interfaces.h"
#include "Physics/PhysicsComponent.h"
#include "scene/actor.h"
#include <cmath>

namespace TopdownShooter::Physics
{
	/// @brief 범용 물리 속도 버스트 — Player Dash + Monster Knockback. PhysicsMovement 옆 거주(b2Body 동일 취급).
	/// @details
	///   - body 는 OnEnter 에서 Components::FindPhysics(GetOwner()) 로 해소(비소유).
	///   - DoImpulse(dir): cd/active 게이트 → SetLinearVelocity(normalize(dir)*force) XZ→XY(Z→-Y) → 타이머 arm.
	///   - i-frame/FX 슬롯 부착 금지(미니 god 방지). i-frame=Life, FX=연출.
	///   - 속도싸움(후속): dash 입력 바인딩 시 controller 가 IsActive() 중 DoForward suppress. 현재 미배선.
	class Impulse : public SJH::Scene::Component,
	                public Entity::IImpulsable
	{
	public:
		Impulse()
		    : mImpulseForce(7.5f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
		      mCooldown(0.8f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed)
		{
		}

		void OnEnter() override { mBody = Components::FindPhysics(GetOwner()); }
		void OnExit() override { mBody = nullptr; }

		void Update(float dt) override
		{
			if (mActiveTimer > 0.0f) mActiveTimer -= dt;
			if (mCooldownTimer > 0.0f) mCooldownTimer -= dt;
		}

		void DoImpulse(vmath::vec2 dir) override
		{
			if (mCooldownTimer > 0.0f || IsActive()) return;
			if (!mBody || !mBody->GetBody()) return;
			const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
			if (len <= 0.001f) return;
			vmath::vec2 n(dir[0] / len, dir[1] / len);
			const float force = mImpulseForce.GetValue();
			// XZ → Box2D XY (Z → -Y, spec §4.4) — PhysicsMovement/PB::Dash 미러
			mBody->GetBody()->SetLinearVelocity(b2Vec2(n[0] * force, -n[1] * force));
			mActiveTimer = mDurationSec;
			mCooldownTimer = mCooldown.GetValue();
		}

		void ApplyKnockback(vmath::vec2 fromXZ) { DoImpulse(fromXZ); } // convenience (Monster Knockback)
		bool IsActive() const { return mActiveTimer > 0.0f; }

	private:
		Algebraic::Numeric::Stat mImpulseForce; // Stat(DashForce, base 7.5)
		Algebraic::Numeric::Stat mCooldown;     // Stat(CoolDownSpeed, base 0.8 — sec 저장)
		float mDurationSec = 0.3f;              // plain (맞는 enum 없음)
		float mActiveTimer = 0.0f;
		float mCooldownTimer = 0.0f;
		Components::Physics *mBody = nullptr; // FindPhysics — 비소유
	};
} // namespace TopdownShooter::Physics

#endif // __MYAPP_PHYSICS_IMPULSE_H__
