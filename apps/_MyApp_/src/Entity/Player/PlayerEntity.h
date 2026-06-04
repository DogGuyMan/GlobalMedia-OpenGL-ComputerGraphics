#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER__

#include "Entity/Components/PlayerLifeComponents.h"
#include "Entity/BaseEntity.h"
#include "Entity/Components/Components.Interfaces.h"   // IMovable
#include "Entity/Components/WeaponComponents.h"        // Components::Weapon (UseWeapon 호출 — 완전형)
#include "timer/timer.h"                               // SJH::Timer::Timer (dust interval poll — 완전형)
#include <functional>
#include <utility>                                     // std::move
#include <vmath.h>

namespace TopdownShooter::Entity
{
	/// @brief 플레이어 Accessor-facade — BaseEntity(공유) + Movement/Weapon 접근 + Dash/Attack verb.
	class PlayerEntity : public BaseEntity, public IMovable
	{
	  protected:
		IMovable*           mMovement = nullptr;  // PhysicsMovement (인터페이스 캐시 — 물리/비물리 양분기)
		Components::Weapon* mWeapon   = nullptr;

		// 이동 자리 dust FX — interval 마다 1회. timer 는 BaseEntity 중앙 컨테이너 위탁(핸들만 보유).
		std::function<void(const vmath::vec3 &)> mOnMoveFx;            // spawn-at-point seam (빌더가 VFX::Spawn 주입)
		SJH::Timer::Timer*                       mDustTimer    = nullptr; // BaseEntity::Timers() 핸들 (비소유)
		float                                    mDustInterval = 0.2f;    // 이동 중 dust 스폰 간격(초)

	  public:
		void OnEnter() override;   // BaseEntity::OnEnter() + mMovement/mWeapon 캐시 + dust timer 등록
		void OnExit()  override;   // dust timer Unregister

		// accessor
		IMovable*           GetMovement() const { return mMovement; }
		Components::Weapon* GetWeapon()   const { return mWeapon; }

		// dust FX seam 주입 (빌더 전용 fluent)
		PlayerEntity &SetOnMoveFx(std::function<void(const vmath::vec3 &)> fx) { mOnMoveFx = std::move(fx); return *this; }

		// verb
		void DoForward(vmath::vec2 dir, float dt) override
		{
			if (mMovement) mMovement->DoForward(dir, dt); // IMovable
			// 이동 자리 dust — interval 경과 시 1회. PlayerController 가 매 프레임 DoForward(정지 시 dir≈0)를
			// 호출하므로 dir 크기로 이동 여부를 판정 (정지 중 스폰 방지). timer Tick 은 BaseEntity::Update 일괄.
			const bool moving = (dir[0] * dir[0] + dir[1] * dir[1]) > 1e-6f;
			if (moving && mOnMoveFx && mDustTimer && mDustTimer->IsTimesUp())
			{
				if (auto *owner = GetOwner())
					mOnMoveFx(owner->GetTransform().Translate);
				mDustTimer->Reset();
			}
		}
		void Dash(vmath::vec2 dir)   { 
			auto* plife = dynamic_cast<Components::PlayerLifeComponent*>(mLife);
			plife->DoInvincible();
			DoImpulse(dir); 
		}                       // BaseEntity 기반(대시)
		void Attack(vmath::vec2 aim) { if (mWeapon) mWeapon->UseWeapon(aim); } // ranged bullet
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER__
