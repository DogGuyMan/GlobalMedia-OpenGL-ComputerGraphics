/**
 * @file Components.Interfaces.h
 * @brief 엔티티 Component 가 구현하는 능력(capability) 인터페이스 모음 + 방향 양자화 유틸.
 *
 * @details
 *  ### 책임
 *  - 게임플레이 능력을 잘게 나눈 순수 가상 인터페이스 선언:
 *    ILivable(생존/HP) / IDieable(사망) / IDamageable(피격) / IAttackable(공격) /
 *    IMovable(이동) / IImpulsable(순간 가속) / IActorPresentation(연출 sink).
 *  - 4방향 enum(@c EFacing) + 포즈 enum(@c EPose) + 방향 벡터 양자화 자유 함수(@c Quantize4).
 *
 *  ### 비-책임
 *  - [X] 상태/데이터 보유 - 각 인터페이스는 계약만 선언, 구현은 concrete Component 가 담당
 *    (Life/Movement/Weapon 등).
 *  - [X] 스프라이트/FMOD/Effekseer 등 외부 의존 - 의존성 inward, 본 헤더 타입 0개.
 *
 *  ### 정통 매핑
 *  - Unity `IDamageable` 식 잘게 쪼갠 인터페이스 - Actor 가 어떤 능력을 갖는지를 인터페이스 조합으로 표현.
 *  - @c BaseEntity 가 형제 Component 를 캐시해 이 인터페이스들로 위임(facade) - @c BaseEntity.h 참조.
 *
 * @note 모든 인터페이스는 복사/이동 금지(rule of five 의 일부만 = 가상 소멸자 + delete 5종).
 *       Component 는 Actor 가 소유하므로 값 복사 의미가 없다.
 */
#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_INTERFACES__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_INTERFACES__

#include <vmath.h>
#include <cmath>   // Quantize4 의 std::abs
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
		virtual void DoForward(vmath::vec2 dir, float dt) = 0;
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
	inline EFacing Quantize4(vmath::vec2 v)
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
		virtual void ReactDied(vmath::vec3 /*pos*/) {}
		/// @brief 공격 연출 반응. @param aimDir 조준 방향.
		virtual void ReactAttack(vmath::vec2 /*aimDir*/) {}
		/// @brief 조준 방향으로 표시 방향 정렬. @param aimDir 조준 방향.
		virtual void FaceAim(vmath::vec2 /*aimDir*/) {}
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
		virtual void DoImpulse(vmath::vec2 dir) = 0;
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__