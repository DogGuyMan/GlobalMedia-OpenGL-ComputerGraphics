#ifndef _TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT_MODIFIER__
#define _TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT_MODIFIER__
#include "Algebraic.Common.h"

namespace TopdownShooter::Algebraic::Numeric
{
	struct StatModifier
	{
		const ENumericStateCalcType CalcType = ENumericStateCalcType::None;
		const ENumericStatType StatType = ENumericStatType::None;
		const float Value = 0;
		const int Order = 0;

		StatModifier(ENumericStateCalcType calc_type, ENumericStatType stat_type, float value, int order) : CalcType(calc_type),
		                                                                                                    StatType(stat_type),
		                                                                                                    Value(value),
		                                                                                                    Order(order)
		{
		}

		bool operator<(const StatModifier &other) const
		{
			return Order < other.Order;
		}
	};
}; // namespace TopdownShooter::Algebraic::Numeric

#endif //_TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT_MODIFIER__