/**
 * @file EntityContracts.h
 * @brief 엔티티 능력(capability) 인터페이스 모음 + 방향 양자화 유틸 - *의존-제로 시임*.
 *
 * @details
 *  ### 거주지 (2026-06-11 이동)
 *  본 헤더는 구 @c Entity/Components/Components.Interfaces.h 에서 *이주*했다. 이유: Physics/
 *  InputHandler/Spawns/Playable 이 이 인터페이스를 구현/소비하느라 @c Entity/ 를 include 하면
 *  서브시스템 레벨 *양방향 사이클*(C1~C4)이 생긴다. 인터페이스를 @c Entity/ 밖 의존-제로 위치
 *  (@c Contracts/)로 빼면 소비처는 @c Contracts/ 만 의존 -> Entity 역의존 소멸(DIP).
 *  네임스페이스는 @c TopdownShooter::Entity 불변(호출처 코드 무변경, include 경로만 변경).
 *  설계 정본: docs/superpowers/plans/2026-06-11-client-cycle-c1-c4-dip-plan.md
 *
 *  ### 책임
 *  - 게임플레이 능력을 잘게 나눈 순수 가상 인터페이스 선언:
 *    ILivable(생존/HP) / IDieable(사망) / IDamageable(피격) / IAttackable(공격) /
 *    IMovable(이동) / IImpulsable(순간 가속) / IActorPresentation(연출 sink).
 *  - 능력 시임(C1/C2 절단용): ITimerOwner(중앙 타이머 접근) / IImpulseState(임펄스 게이트) /
 *    IPlayerCommand(대시/발사 verb) / IFireTrigger(발사 핀치 통지).
 *  - 4방향 enum(@c EFacing) + 포즈 enum(@c EPose) + 방향 벡터 양자화 자유 함수(@c Quantize4).
 *
 *  ### 비-책임
 *  - [X] 상태/데이터 보유 - 각 인터페이스는 계약만 선언, 구현은 concrete Component 가 담당.
 *  - [X] 외부 의존 - 본 헤더 의존은 @c <glm/glm.hpp>/@c <cmath> + @c MultipleTimer *전방선언* 뿐(의존-제로).
 *
 * @note 모든 인터페이스는 복사/이동 금지(가상 소멸자 + delete 5종). Component 는 Actor 가 소유.
 */
#ifndef _TOPDOWNSHOOTER_CONTRACTS_ENTITYCONTRACTS_H__
#define _TOPDOWNSHOOTER_CONTRACTS_ENTITYCONTRACTS_H__

#include <glm/glm.hpp>
#include <cmath>   // Quantize4 의 std::abs

// fwd - ITimerOwner::Timers() 반환 타입. 완전형은 소비처(.cpp)가 timer/multiple_timer.h 로 포함.
namespace SJH::Timer
{
	class MultipleTimer;
}

namespace TopdownShooter::Entity
{
	/**
	 * @brief 생존 상태/HP 조회 능력 - HP 를 갖는 모든 엔티티가 구현.
	 * @details @c Life Component 가 구현. @c BaseEntity 가 위임 노출(ILivable as facade).
	 */
	class ILivable
	{
	  protected:
		ILivable() = default;

	  public:
		virtual ~ILivable() = default;
		ILivable(const ILivable &) = delete;
		ILivable operator=(const ILivable &) = delete;
		ILivable(ILivable &&) = delete;
		ILivable operator=(ILivable &) = delete;

		/// @brief 살아 있는가(HP > 0).
		/// @return HP 가 0 보다 크면 true.
		virtual bool IsAlive() const = 0;
		/// @brief 현재 HP.
		/// @return 현재 체력 값.
		virtual int GetHp() const = 0;
		/// @brief 최대 HP.
		/// @return 최대 체력 값.
		virtual int GetMaxHp() const = 0;
		// virtual ostream &GetHpState(ostream &) const = 0;
	};
	/**
	 * @brief 사망 처리 능력 - HP 0 도달 시 사망 연출/제거를 발동.
	 * @details @c Life Component 가 구현(one-shot 가드 + 사망 FX/observer seam 발동).
	 */
	class IDieable
	{
	  protected:
		IDieable() = default;

	  public:
		virtual ~IDieable() = default;
		IDieable(const IDieable &) = delete;
		IDieable operator=(const IDieable &) = delete;
		IDieable(IDieable &&) = delete;
		IDieable operator=(IDieable &) = delete;
		/// @brief 사망 처리 발동(사망 FX/연출/observer 통지). 구현체는 one-shot 보장.
		virtual void DoDie() = 0;
	};
	/**
	 * @brief 피격(데미지 수신) 능력 - 데미지 적용 + i-frame/사망 판정.
	 * @details @c Life Component 가 구현. @c Weapon::DoAttack 의 타깃 타입(@c IDamageable&).
	 */
	class IDamageable
	{
	  protected:
		IDamageable() = default;

	  public:
		virtual ~IDamageable() = default;
		IDamageable(const IDamageable &) = delete;
		IDamageable operator=(const IDamageable &) = delete;
		IDamageable(IDamageable &&) = delete;
		IDamageable operator=(IDamageable &) = delete;
		/// @brief 데미지 수신 - HP 차감 + 무적 중이면 무시 + HP 0 시 사망 연결.
		/// @param damage 적용할 데미지 양.
		virtual void DoDamaged(int damage) = 0;
	};

	/**
	 * @brief 공격 능력 - 타깃에게 데미지를 가하는 주체.
	 * @details @c Weapon 등이 구현. @c DoAttack 의 인자가 @c IDamageable& 라 능력끼리 직접 연결.
	 */
	class IAttackable
	{
	  protected:
		IAttackable() = default;

	  public:
		virtual ~IAttackable() = default;
		IAttackable(const IAttackable &) = delete;
		IAttackable operator=(const IAttackable &) = delete;
		IAttackable(IAttackable &&) = delete;
		IAttackable operator=(IAttackable &) = delete;

		/// @brief 지정 타깃에게 공격 실행(데미지 적용은 target 의 DoDamaged 위임).
		/// @param target 데미지를 받을 대상.
		virtual void DoAttack(IDamageable &target) = 0;
		/// @brief 기본 공격력.
		/// @return 평타 데미지 값.
		virtual int GetNormalAtk() const = 0;
		// virtual string GetAttackName() const = 0;
	};

	/**
	 * @brief 지속 이동 능력 - 단위 방향 + dt 로 프레임 변위 계산.
	 * @details @c Movement Component 가 구현(units/sec * dt = fps-independent 변위).
	 *          순간 가속(Dash/Knockback)은 별도 @c IImpulsable 사용.
	 */
	class IMovable
	{
	  protected:
		IMovable() = default;

	  public:
		virtual ~IMovable() = default;
		IMovable(const IMovable &) = delete;
		IMovable operator=(const IMovable &) = delete;
		IMovable(IMovable &&) = delete;
		IMovable operator=(IMovable &) = delete;

		/// @brief 단위 방향 @p dir 로 @p dt 초만큼 이동. 구현체가 units/sec 단위 속도 보유 가정.
		/// @param dir 이동 방향(정규화는 구현체 책임).
		/// @param dt  경과 시간(초). units/sec 속도와 곱해 프레임 변위 산출(fps-independent).
		virtual void DoForward(glm::vec2 dir, float dt) = 0;
	};

	enum class EFacing : int { Front = 0, ///< 정면(z 양수 방향).
	                           Back,      ///< 후면(z 음수 방향).
	                           Left,      ///< 좌측(x 음수 방향).
	                           Right };   ///< 우측(x 양수 방향).
	enum class EPose   : int { Idle  = 0, ///< 정지 포즈.
	                           Move };    ///< 이동 포즈.

	/// @brief XZ 방향 벡터(aim/velocity)를 4방향으로 양자화. |x| > |z| 이면 좌우, 아니면 전후.
	/// @details 부호 규약: x>0=Right, z>0=Front (탑다운 W=-Z 기준, Task6 실행 검증).
	/// @param v 양자화할 방향 벡터(보통 조준/속도).
	/// @return @c EFacing 4방향 중 우세 축.
	inline EFacing Quantize4(glm::vec2 v)
	{
		if (std::abs(v[0]) > std::abs(v[1]))
			return v[0] > 0.0f ? EFacing::Right : EFacing::Left;
		return v[1] > 0.0f ? EFacing::Front : EFacing::Back;
	}

	/**
	 * @brief 게임플레이 베이스에서 연출 sink 로 forward 하는 인터페이스(Template-Method 대상). RD1 OPT-1.
	 * @details
	 *  IContactable 식으로 전부 defaulted no-op - director 가 실제로 쓰는 verb 만 override 한다.
	 *  스프라이트/FMOD/Effekseer 타입을 0개 노출(의존성 inward) - 게임플레이 레이어가 연출 구현을
	 *  알 필요 없게 하는 경계. @c Life 등은 이 인터페이스 포인터(@c mSink)만 캐시해 호출한다.
	 */
	class IActorPresentation
	{
	  protected:
		IActorPresentation() = default;

	  public:
		virtual ~IActorPresentation() = default;
		IActorPresentation(const IActorPresentation &) = delete;
		IActorPresentation &operator=(const IActorPresentation &) = delete;
		IActorPresentation(IActorPresentation &&) = delete;
		IActorPresentation &operator=(IActorPresentation &&) = delete;

		/// @brief 피격 연출 반응(데미지 수신 시점). @param dmg 받은 데미지 양.
		virtual void ReactDamaged(int /*dmg*/) {}
		/// @brief 사망 연출 반응(디졸브 등 시작). @param pos 사망 발생 월드 위치.
		virtual void ReactDied(glm::vec3 /*pos*/) {}
		/// @brief 공격 연출 반응. @param aimDir 조준 방향.
		virtual void ReactAttack(glm::vec2 /*aimDir*/) {}
		/// @brief 조준 방향으로 표시 방향 정렬. @param aimDir 조준 방향.
		virtual void FaceAim(glm::vec2 /*aimDir*/) {}
		/// @brief 4방향 facing 강제 설정. @param facing 설정할 방향.
		virtual void SetFacing(EFacing /*facing*/) {}
		/// @brief 포즈(Idle/Move) 설정. @param pose 설정할 포즈.
		virtual void SetPose(EPose /*pose*/) {}
	};

	/**
	 * @brief 일회성 타임드 속도 버스트(Dash/Knockback) 능력 - 지속 이동 @c IMovable 의 대칭 파트너.
	 * @details
	 *  지속 이동(@c DoForward)과 의미가 다르므로 @c IMovable 오버로드 대신 별도 인터페이스로 분리
	 *  (SetMovableTarget 계약 보존). @c Physics::Impulse 등이 구현.
	 */
	class IImpulsable
	{
	  protected:
		IImpulsable() = default;

	  public:
		virtual ~IImpulsable() = default;
		IImpulsable(const IImpulsable &) = delete;
		IImpulsable &operator=(const IImpulsable &) = delete;
		IImpulsable(IImpulsable &&) = delete;
		IImpulsable &operator=(IImpulsable &&) = delete;

		/// @brief 지정 방향으로 순간 가속(버스트) 발동.
		/// @param dir 버스트 방향.
		virtual void DoImpulse(glm::vec2 dir) = 0;
	};

	// ====== 능력 시임 (2026-06-11 신설 - C1/C2 사이클 절단용 DIP) ======

	/**
	 * @brief 엔티티 중앙 타이머 컨테이너 접근 능력 - @c BaseEntity 가 구현.
	 * @details
	 *  형제 Component(Physics::Impulse, InputHandler 등)가 타이머 핸들을 @c Register/Unregister
	 *  위탁할 때 구상 @c BaseEntity 대신 본 인터페이스로 접근 -> Physics/InputHandler 의 Entity 역의존 절단(C1/C2).
	 *  반환 타입은 비-const(Register 가 mutate). 완전형 @c MultipleTimer 는 호출처 .cpp 가 포함.
	 */
	class ITimerOwner
	{
	  protected:
		ITimerOwner() = default;

	  public:
		virtual ~ITimerOwner() = default;
		ITimerOwner(const ITimerOwner &) = delete;
		ITimerOwner &operator=(const ITimerOwner &) = delete;
		ITimerOwner(ITimerOwner &&) = delete;
		ITimerOwner &operator=(ITimerOwner &&) = delete;

		/// @brief 엔티티 중앙 @c MultipleTimer 참조 - 형제가 핸들 Register/Unregister 위탁.
		/// @return 엔티티 timer 단일 보유처 레퍼런스.
		virtual SJH::Timer::MultipleTimer &Timers() = 0;
	};

	/**
	 * @brief 임펄스(대시/넉백) 활성 게이트 조회 능력 - @c BaseEntity 가 구현.
	 * @details InputHandler 가 *대시 중 일반 이동 suppress* 판정을 구상 @c BaseEntity 대신 본 인터페이스로 조회(C2 절단).
	 */
	class IImpulseState
	{
	  protected:
		IImpulseState() = default;

	  public:
		virtual ~IImpulseState() = default;
		IImpulseState(const IImpulseState &) = delete;
		IImpulseState &operator=(const IImpulseState &) = delete;
		IImpulseState(IImpulseState &&) = delete;
		IImpulseState &operator=(IImpulseState &&) = delete;

		/// @brief 임펄스(버스트) 활성 창인가 - true 면 일반 이동 suppress 게이트.
		/// @return 임펄스 활성 중이면 true.
		virtual bool IsImpulseActive() const = 0;
	};

	/**
	 * @brief 플레이어 액션 verb(대시/무기 사용) 능력 - @c PlayerEntity 가 구현.
	 * @details
	 *  InputHandler 가 구상 @c PlayerEntity 대신 본 인터페이스로 대시/발사 verb 를 구동(C2 절단, D4-a).
	 *  Dash/UseWeapon 은 같은 구현체(PlayerEntity)·같은 소비처(InputHandler)라 한 인터페이스로 묶음.
	 */
	class IPlayerCommand
	{
	  protected:
		IPlayerCommand() = default;

	  public:
		virtual ~IPlayerCommand() = default;
		IPlayerCommand(const IPlayerCommand &) = delete;
		IPlayerCommand &operator=(const IPlayerCommand &) = delete;
		IPlayerCommand(IPlayerCommand &&) = delete;
		IPlayerCommand &operator=(IPlayerCommand &&) = delete;

		/// @brief 대시 - 지정 방향으로 무적+임펄스+연출 발동. @param dir 대시 방향.
		virtual void Dash(glm::vec2 dir) = 0;
		/// @brief 무기 사용(발사) - 조준 방향으로 Weapon 위임. @param aim 조준 방향.
		virtual void UseWeapon(glm::vec2 aim) = 0;
	};

	/**
	 * @brief 발사 핀치 통지 능력 - @c PlayerHands 가 구현.
	 * @details InputHandler 좌클릭 바인딩이 구상 @c PlayerHands 대신 본 인터페이스로 통지(C2 절단). 실제 좁힘/복귀는 PlayerHands 소유.
	 */
	class IFireTrigger
	{
	  protected:
		IFireTrigger() = default;

	  public:
		virtual ~IFireTrigger() = default;
		IFireTrigger(const IFireTrigger &) = delete;
		IFireTrigger &operator=(const IFireTrigger &) = delete;
		IFireTrigger(IFireTrigger &&) = delete;
		IFireTrigger &operator=(IFireTrigger &&) = delete;

		/// @brief 발사 핀치(손 좁힘) 통지 - 실제 좁힘/복귀 로직은 PlayerHands 소유.
		virtual void TriggerFire() = 0;
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_CONTRACTS_ENTITYCONTRACTS_H__
