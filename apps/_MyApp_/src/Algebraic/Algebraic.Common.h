#ifndef _TOPDOWNSHOOTER_ALGEBRAIC_COMMON__
#define _TOPDOWNSHOOTER_ALGEBRAIC_COMMON__

namespace TopdownShooter::Algebraic
{
	enum class ENumericStatUseType : int
	{
		None = 0,
		Natural = 1,
		Ratio,
		Percentage
	};

	enum class ENumericStateCalcType : int
	{
		None = 0,
		Add = 1,
		Mul = 2
	};

	enum class ENumericStatType : int
	{
		None = 0,

		MaxHp = 1,
		Defence,
		Power,
		MoveSpeed,
		Accecerate,
		Tenacity,

		MaxStamina = 11,
		StaminaRestoreSpeed,
		DashForce,

		InstantiableDurateLifeTimeMultiplyRatio = 21,
		InstantiableSizeMultiplyRatio,
		InstantiableForwardingSpeedMultiplyRatio,

		PoolSize = 31,
		AttackSpeed,
		MeleeRatio,
		RangerRatio,
		TechRatio,

		EfficienceMultiplyer = 41,
		CoolDownSpeed,

		Luck = 51
	};
}; // namespace TopdownShooter::Algebraic::Numeric
#endif //_TOPDOWNSHOOTER_ALGEBRAIC_COMMON__