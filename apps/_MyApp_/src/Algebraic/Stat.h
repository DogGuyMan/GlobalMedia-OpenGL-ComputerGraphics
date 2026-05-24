#ifndef _TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT__
#define _TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT__
#include "Algebraic.Common.h"
#include "StatModifier.h"
#include "scene/actor.h"
#include <functional>
#include <set>
#include <string>

namespace TopdownShooter::Algebraic::Numeric
{
	class Stat
	{
	  private:
		// lazy-cache 패턴 — GetValue() const 안에서 RecalculateStat() 호출하므로 mutable.
		mutable float mCachedValue;
		mutable bool mIsDirty;
		std::set<StatModifier> mModifiers;

		void CalculateWithUseAndCalcType(ENumericStatUseType use_type, StatModifier curModifier, float &adder, float &multiplier) const;

		void RecalculateStat() const;

	  public:
		const float BaseValue;
		const ENumericStatUseType UseType;
		const ENumericStatType NumericType;

		Stat(float base_value, ENumericStatUseType use_type, ENumericStatType numeric_type);

		float GetValue() const;

		void AddModifier(StatModifier modifier);

		void RemoveModifier(StatModifier modifier);

		void ResetModifiers();
	};
}; // namespace TopdownShooter::Algebraic::Numeric

#endif //_TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT__
