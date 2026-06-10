/**
 * @file PlayerEntity.h
 * @brief 플레이어 엔티티 Accessor-facade - BaseEntity 위임 + 이동/무기 접근 + Dash/Attack verb.
 *
 * @details
 *  ### 책임
 *  - @c BaseEntity 를 상속해 Life/Physics/Director/Impulse 공통 facade 를 물려받는다.
 *  - OnEnter 에서 형제 Component(@c PhysicsMovement / @c Components::Weapon)를 비소유 캐시.
 *  - @c IMovable 구현 - @c DoForward 로 이동 위임 + 이동 중 dust FX 스폰 (interval 게이트).
 *  - @c Dash(dir) verb - @c PlayerLifeComponent::DoInvincible + @c DoImpulse + "dash" 연출 발화.
 *  - @c UseWeapon(aim) verb - @c Components::Weapon 위임 (투사체 발사).
 *
 *  ### 비-책임
 *  - [X] 입력 처리 - PlayerController 가 담당 (PlayerEntity 는 결과만 받아 verb 실행).
 *  - [X] 물리/이동 로직 - PhysicsMovement(IMovable) 가 담당.
 *  - [X] 무기 쿨다운/발사체 생성 - Components::Weapon 가 담당.
 *  - [X] HP/i-frame/사망 - Life(BaseEntity 위임) 가 담당.
 *
 *  ### 정통 매핑
 *  - Unity PlayerController.cs 의 "입력 -> 엔티티 verb" 접점 계층에 해당.
 *  - Facade 패턴 - 형제 Component 여럿을 단일 진입점으로 노출.
 *
 * @note @c mMovement 캐시 시 @c GetComponent<IMovable>() 대신 @c GetComponent<PhysicsMovement>() 를
 *       사용하는 이유: PlayerEntity 자신도 @c IMovable 를 구현하므로 인터페이스 조회 시 자기 자신이
 *       매칭되는 위험이 있다. 구체 타입(typeid 매칭)으로 캐시해 자기 매칭을 명시적으로 배제한다.
 */
#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER__

#include "Entity/BaseEntity.h"
#include "Entity/Components/Components.Interfaces.h" // IMovable
#include "Entity/Components/PlayerLifeComponents.h"
#include "Entity/Components/WeaponComponents.h" // Components::Weapon (UseWeapon 호출 - 완전형)
#include "timer/timer.h"                        // SJH::Timer::Timer (dust interval poll - 완전형)
#include <functional>
#include <utility> // std::move
#include <vmath.h>

namespace TopdownShooter::Entity
{
	/**
	 * @brief 플레이어 Accessor-facade - BaseEntity(공유) + Movement/Weapon 접근 + Dash/Attack verb.
	 * @details
	 *  @c BaseEntity 를 통해 Life/Physics/Director/Impulse 를 facade 로 물려받고,
	 *  추가로 @c PhysicsMovement / @c Components::Weapon 두 형제 Component 를 비소유 캐시한다.
	 *
	 *  - 이동: @c DoForward(dir, dt) - mMovement 에 위임 + 이동 중 dust FX interval 게이트 스폰.
	 *  - 대시: @c Dash(dir) - PlayerLifeComponent::DoInvincible + DoImpulse + "dash" 연출 rising-edge.
	 *  - 발사: @c UseWeapon(aim) - Components::Weapon::UseWeapon 위임.
	 *
	 *  dust FX seam(@c mOnMoveFx)은 빌더가 @c SetOnMoveFx 로 주입 - PlayerEntity 는 VFX 코드와 무관.
	 */
	class PlayerEntity : public BaseEntity, public IMovable
	{
	  protected:
		IMovable *mMovement = nullptr; ///< PhysicsMovement 비소유 캐시 (인터페이스 포인터 - 물리/비물리 양분 대비).
		Components::Weapon *mWeapon = nullptr; ///< Weapon Component 비소유 캐시 (투사체 발사 위임).

		// 이동 자리 dust FX - interval 마다 1회. timer 는 BaseEntity 중앙 컨테이너 위탁(핸들만 보유).
		std::function<void(const vmath::vec3 &)> mOnMoveFx; ///< dust 스폰 seam - 빌더가 VFX::Spawn 클로저를 주입.
		SJH::Timer::Timer *mDustTimer = nullptr;            ///< BaseEntity::Timers() 핸들 (비소유 - 컨테이너가 owner).
		float mDustInterval = 0.2f;                         ///< 이동 중 dust 스폰 간격(초).

	  public:
		/// @brief 형제 Component(@c PhysicsMovement / @c Weapon) 캐시 + dust timer 등록.
		/// @details BaseEntity::OnEnter() 로 Life/Physics/Director/Impulse 먼저 캐시 후,
		///          @c PhysicsMovement 와 @c Weapon 을 추가로 캐시. dust interval timer 를
		///          @c BaseEntity::Timers() 에 "player.dust" 키로 등록.
		void OnEnter() override;

		/// @brief dust timer Unregister 후 BaseEntity::OnExit() 위임.
		/// @details "player.dust" 를 먼저 해제해 dangling 핸들을 방지한다.
		void OnExit() override;

		// -- accessor ------------------------------------------------------------

		/// @brief 캐시한 이동 Component 접근.
		/// @return IMovable 포인터 (미캐시 시 nullptr).
		IMovable *GetMovement() const
		{
			return mMovement;
		}

		/// @brief 캐시한 무기 Component 접근.
		/// @return Weapon 포인터 (미캐시 시 nullptr).
		Components::Weapon *GetWeapon() const
		{
			return mWeapon;
		}

		/// @brief dust FX seam 주입 - 빌더 전용 fluent.
		/// @param fx 이동 자리 dust 스폰 콜백. 인자는 플레이어 world 위치(@c vec3).
		/// @return *this (fluent builder 연쇄용).
		// dust FX seam 주입 (빌더 전용 fluent)
		PlayerEntity &SetOnMoveFx(std::function<void(const vmath::vec3 &)> fx)
		{
			mOnMoveFx = std::move(fx);
			return *this;
		}

		// -- verb ----------------------------------------------------------------

		/// @brief 이동 verb - mMovement 위임 + 이동 중 dust interval 스폰.
		/// @details PlayerController 가 매 프레임 호출. @p dir 크기(threshold 1e-6f)로 정지/이동 판정.
		///          dust timer Tick 은 BaseEntity::Update 일괄 처리 - DoForward 내 tick 없음.
		/// @param dir 이동 방향 벡터 (XZ, 크기 = 속도 배율 아님 - 단위 방향 권장).
		/// @param dt  직전 프레임 경과 시간(초).
		void DoForward(vmath::vec2 dir, float dt) override
		{
			if (mMovement)
				mMovement->DoForward(dir, dt); // IMovable
			// 이동 자리 dust - interval 경과 시 1회. PlayerController 가 매 프레임 DoForward(정지 시 dir~=0)를
			// 호출하므로 dir 크기로 이동 여부를 판정 (정지 중 스폰 방지). timer Tick 은 BaseEntity::Update 일괄.
			const bool moving = (dir[0] * dir[0] + dir[1] * dir[1]) > 1e-6f;
			if (moving && mOnMoveFx && mDustTimer && mDustTimer->IsTimesUp())
			{
				if (auto *owner = GetOwner())
					mOnMoveFx(owner->GetTransform().Translate);
				mDustTimer->Reset();
			}
		}

		/// @brief 대시 verb - 무적 활성 + 임펄스 발동 + rising-edge "dash" 연출.
		/// @details Shift 를 누르는 동안 매 프레임 호출되므로 @c IsImpulseActive() rising-edge 로
		///          "dash" Play 를 1회만 발화한다 (폭주 방지).
		///          @c PlayerLifeComponent::DoInvincible -> @c DoImpulse 순서 고정.
		/// @param dir 대시 방향 벡터 (XZ).
		void Dash(vmath::vec2 dir)
		{
			const bool wasActive = IsImpulseActive(); // rising-edge 판정용 (직전 버스트 활성?)
			auto *plife = dynamic_cast<Components::PlayerLifeComponent *>(mLife);
			plife->DoInvincible();
			DoImpulse(dir);
			// 대시 연출/음("dash") - 실제 발동(쿨다운/중복 아닌 false->true 전이)에서만 1회.
			//   DashImpulse 는 held(Shift 누르는 동안 매 프레임 Dash 호출)라 무조건 Play 하면 폭주 ->
			//   IsImpulseActive() edge 로 게이트. BaseEntity::Play -> director->Play("dash")(EVENT_DASH 사운드).
			if (!wasActive && IsImpulseActive())
				Play("dash");
		} // BaseEntity 기반(대시)

		/// @brief 원거리 발사 verb - @c Components::Weapon::UseWeapon 위임.
		/// @param aim 조준 방향 벡터 (XZ world 평면). 투사체 진행 방향 계산에 사용.
		void UseWeapon(vmath::vec2 aim)
		{
			if (mWeapon)
			{
				mWeapon->UseWeapon(aim);
			}
		} // ranged bullet
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER__
