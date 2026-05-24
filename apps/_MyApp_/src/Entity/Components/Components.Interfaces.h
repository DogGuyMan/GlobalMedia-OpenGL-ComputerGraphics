#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_INTERFACES__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_INTERFACES__

#include <vmath.h>
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

		virtual void DoAttack(IAttackable &target) = 0;
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

		virtual void DoForward(vmath::vec2 dir) = 0;
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_LIFE__