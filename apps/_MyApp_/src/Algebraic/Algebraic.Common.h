#ifndef _TOPDOWNSHOOTER_ALGEBRAIC_COMMON__
#define _TOPDOWNSHOOTER_ALGEBRAIC_COMMON__

namespace TopdownShooter::Algebraic
{
	enum class ENumericStatUseType : int
	{
		None = -1,
		Natural = 0,
		Ratio,
		Percentage
	};

	enum class ENumericStateCalcType : int
	{
		None = -1,
		Add = 0,
		Mul = 1
	};

	enum class ENumericStatType : int
	{
		None = -1,

		MaxHp = 0,
		Defence,
		Power,
		MoveSpeed,
		Accecerate,
		Tenacity,

		MaxStamina = 10,
		StaminaRestoreSpeed,
		DashForce,

		InstantiableDurateLifeTimeMultiplyRatio = 20,
		InstantiableSizeMultiplyRatio,
		InstantiableForwardingSpeedMultiplyRatio,

		PoolSize = 30,
		AttackSpeed,
		MeleeRatio,
		RangerRatio,
		TechRatio,

		EfficienceMultiplyer = 40,
		CoolDownSpeed,

		Luck = 50
	};
}; // namespace TopdownShooter::Algebraic::Numeric
#endif //_TOPDOWNSHOOTER_ALGEBRAIC_COMMON__