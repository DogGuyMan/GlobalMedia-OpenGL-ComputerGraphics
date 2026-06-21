/**
 * @file BaseEntity.h
 * @brief Player/Enemy 가 공유하는 엔티티 공통 Accessor-facade Component.
 *
 * @details
 *  ### 책임
 *  - OnEnter 시 형제 Component 4종(Life/Physics/Director/Impulse)을 비소유 캐시.
 *  - ILivable/IDieable/IDamageable/IImpulsable 인터페이스를 형제 Component 로 위임.
 *  - 엔티티 timer 의 유일한 보유처 (@c mTimers) - 형제가 Register 위탁, Update 가 일괄 tick.
 *
 *  ### 비-책임
 *  - [X] 게임 로직 - 로직 0. 형제 Component 의 구현으로 전부 위임 (facade).
 *  - [X] 형제 Component 의 lifetime 소유 - 포인터만 비소유 캐시 (Actor 가 owner).
 *
 *  ### 정통 매핑
 *  - Unity `MonoBehaviour` GetComponent 캐시 + 인터페이스 위임 패턴.
 *
 * @note 헤더는 Playable/Physics 완전형을 끌어오지 않도록 포인터 멤버 + 전방 선언만 사용 -
 *       완전형은 @c BaseEntity.cpp 에서만 포함.
 */
#ifndef _TOPDOWNSHOOTER_ENTITY_BASE__
#define _TOPDOWNSHOOTER_ENTITY_BASE__

#include "Contracts/EntityContracts.h"   // ILivable/IDieable/IDamageable/IImpulsable
#include "scene/actor.h"
#include "timer/multiple_timer.h"   // MultipleTimer (값 멤버 - 완전형 필요)
#include <string>

// fwd - 포인터 멤버만 보유(완전형은 .cpp 에서). Entity 헤더가 Playable/Physics 헤더를 안 끌어오게.
namespace TopdownShooter::Entity::Components { class Life; }
namespace TopdownShooter::Physics::Components { class Physics; }
namespace TopdownShooter::Physics { class Impulse; }
namespace TopdownShooter::Playable { class PlayableDirector; }

namespace TopdownShooter::Entity
{
	/**
	 * @brief 엔티티 공통 Accessor-facade (Player/Enemy 공유). 로직 0 - 형제 Component 비소유 캐시 + 위임/노출.
	 * @details
	 *  ILivable/IDieable/IDamageable/IImpulsable 4 인터페이스를 다중 상속하나, 모든 구현은
	 *  형제 Component(Life/Impulse)로 위임한다. 형제가 아직 없으면(nullptr) 안전한 기본값 반환.
	 *  엔티티의 timer 는 본 클래스의 @c mTimers 한 곳에만 존재하며, 형제가 핸들을 Register 위탁한다.
	 */
	class BaseEntity : public SJH::Scene::Component,
	                   public ILivable, public IDieable, public IDamageable, public IImpulsable,
	                   public ITimerOwner, public IImpulseState
	{
	  protected:
		Components::Life*                    mLife     = nullptr;   ///< Life Component 비소유 캐시 (HP/i-frame/사망).
		Physics::Components::Physics*        mPhysics  = nullptr;   ///< 물리 바디 = entityRigidbody/Collider.
		Playable::PlayableDirector*          mDirector = nullptr;   ///< 연출+Audio (named playable) Director.
		Physics::Impulse*                    mImpulse  = nullptr;   ///< 넉백/대시 임펄스. null-guard = 비엔티티 바디 대비.
		SJH::Timer::MultipleTimer            mTimers;               ///< 엔티티 timer 유일 보유처 (형제가 Register 위탁).

	  public:
		/// @brief 형제 Component 4종(Life/Physics/Director/Impulse)을 GetComponent/FindPhysics 로 캐시.
		void OnEnter() override;   // 4캐시 (.cpp - GetComponent/FindPhysics 완전형 필요)
		/// @brief 캐시한 형제 포인터 4종을 nullptr 로 초기화 (dangling 방지).
		void OnExit()  override;
		/// @brief 중앙 timer 컨테이너 일괄 tick - 엔티티 timer 의 유일한 구동 지점.
		/// @param dt 직전 프레임 경과 시간(초).
		void Update(float dt) override { mTimers.Update(dt); }   // 중앙 tick 구동 (형제 timer 일괄)

		// -- 중앙 Timer 컨테이너 (엔티티 timer 단일 보유처) --
		/// @brief 엔티티 timer 컨테이너 접근 - 형제 Component 가 핸들 Register/Unregister 위탁.
		/// @return MultipleTimer 레퍼런스.
		SJH::Timer::MultipleTimer&       Timers() override { return mTimers; }
		/// @brief 엔티티 timer 컨테이너 const 접근.
		/// @return MultipleTimer const 레퍼런스.
		const SJH::Timer::MultipleTimer& Timers() const { return mTimers; }

		// -- Life 위임 (ILivable/IDieable/IDamageable) --
		/// @brief 생존 여부 - Life 위임. Life 미캐시 시 false.
		/// @return 살아 있으면 true.
		bool IsAlive()  const override;
		/// @brief 현재 HP - Life 위임. Life 미캐시 시 0.
		/// @return 현재 체력.
		int  GetHp()    const override;
		/// @brief 최대 HP - Life 위임. Life 미캐시 시 0.
		/// @return 최대 체력.
		int  GetMaxHp() const override;
		/// @brief 피격 처리 - Life 위임 (i-frame 판정은 Life 내부).
		/// @param damage 받는 데미지 양.
		void DoDamaged(int damage) override;   // i-frame 은 Life 내부
		/// @brief 사망 처리 - Life 위임.
		void DoDie()               override;
		// -- Impulse 위임 (IImpulsable) - 넉백/대시 --
		/// @brief 일회성 속도 버스트(넉백/대시) - Impulse 위임.
		/// @param dir 임펄스 방향 (world XZ).
		void DoImpulse(glm::vec2 dir) override;
		/// @brief 임펄스 버스트 활성 창 여부 - 이동 suppress 게이트로 사용.
		/// @return 버스트가 활성 중이면 true (Impulse 미캐시 시 false).
		bool IsImpulseActive() const override; // 버스트 활성 창 = 이동 suppress 게이트
		// -- accessor --
		/// @brief 캐시한 물리 바디 Component 노출.
		/// @return Physics 포인터 (미캐시 시 nullptr).
		Physics::Components::Physics* GetPhysics()  const { return mPhysics; }
		/// @brief 캐시한 연출 Director 노출 (포인터 반환이라 전방 선언으로 충분).
		/// @return PlayableDirector 포인터 (미캐시 시 nullptr).
		Playable::PlayableDirector*   GetDirector() const { return mDirector; } // 포인터 반환 = fwd OK
		/// @brief named playable 재생 요청 - Director::Play 위임.
		/// @param key 재생할 playable 키.
		void Play(const std::string& key);     // .cpp (director->Play 완전형 필요)
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_BASE__
