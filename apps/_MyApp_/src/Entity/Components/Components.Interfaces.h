#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_INTERFACES__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_INTERFACES__

#include <vmath.h>
#include <cmath>   // Quantize4 의 std::abs
namespace TopdownShooter::Entity
{
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

		virtual bool IsAlive() const = 0;
		virtual int GetHp() const = 0;
		virtual int GetMaxHp() const = 0;
		// virtual ostream &GetHpState(ostream &) const = 0;
	};
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
		virtual void DoDie() = 0;
	};
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
		virtual void DoDamaged(int damage) = 0;
	};

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

		virtual void DoAttack(IDamageable &target) = 0;
		virtual int GetNormalAtk() const = 0;
		// virtual string GetAttackName() const = 0;
	};

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

		/// @brief 단위 방향 dir 로 dt 초만큼 이동. 구현체가 *units/sec* 단위 속도 보유 가정.
		virtual void DoForward(vmath::vec2 dir, float dt) = 0;
	};

	enum class EFacing : int { Front = 0, Back, Left, Right };
	enum class EPose   : int { Idle  = 0, Move };

	/// @brief XZ 방향 벡터(aim/velocity)를 4방향으로 양자화. |x|>|z| 이면 좌우, 아니면 전후.
	///        부호 규약: x>0=Right, z>0=Front (탑다운 W=-Z 기준 — Task6 실행 검증).
	inline EFacing Quantize4(vmath::vec2 v)
	{
		if (std::abs(v[0]) > std::abs(v[1]))
			return v[0] > 0.0f ? EFacing::Right : EFacing::Left;
		return v[1] > 0.0f ? EFacing::Front : EFacing::Back;
	}

	/// @brief 게임플레이 베이스 -> 연출 sink (Template-Method forward 대상). RD1 OPT-1.
	///        IContactable 式 defaulted no-op — director 가 쓰는 verb 만 override.
	///        스프라이트/FMOD/Effekseer 타입 0개 (deps inward).
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

		virtual void ReactDamaged(int /*dmg*/) {}
		virtual void ReactDied(vmath::vec3 /*pos*/) {}
		virtual void ReactAttack(vmath::vec2 /*aimDir*/) {}
		virtual void FaceAim(vmath::vec2 /*aimDir*/) {}
		virtual void SetFacing(EFacing /*facing*/) {}
		virtual void SetPose(EPose /*pose*/) {}
	};

	/// @brief 일회성 타임드 속도 버스트 (Dash/Knockback). DoForward(지속)의 대칭 파트너.
	///        IMovable 오버로드 금지 (SetMovableTarget 계약) -> 신규 인터페이스.
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

		virtual void DoImpulse(vmath::vec2 dir) = 0;
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__