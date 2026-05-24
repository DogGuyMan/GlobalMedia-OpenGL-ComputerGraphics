#include "Stat.h"
#include <cstdlib>

namespace TopdownShooter::Algebraic::Numeric
{
	Stat::Stat(float base_value, ENumericStatUseType use_type, ENumericStatType numeric_type)
	    : BaseValue(base_value), UseType(use_type), NumericType(numeric_type)
	{
		mCachedValue = base_value;
		mIsDirty = false;
	}

	void Stat::CalculateWithUseAndCalcType(ENumericStatUseType use_type, StatModifier curModifier, float &adder, float &multiplier) const
	{
		switch (use_type)
		{
		case ENumericStatUseType::Natural: {
			if (curModifier.CalcType == ENumericStateCalcType::Add)
			{
				adder += curModifier.Value;
				return;
			}
			else if (curModifier.CalcType == ENumericStateCalcType::Mul)
			{
				multiplier += curModifier.Value;
				return;
			}
			break;
		}
		case ENumericStatUseType::Ratio: {
			if (curModifier.CalcType == ENumericStateCalcType::Add)
			{
				adder += curModifier.Value;
				return;
			}
			else if (curModifier.CalcType == ENumericStateCalcType::Mul)
			{
				multiplier += curModifier.Value;
				return;
			}
			break;
		}
		case ENumericStatUseType::Percentage: {
			if (curModifier.CalcType == ENumericStateCalcType::Add)
			{
				adder += curModifier.Value;
				return;
			}
			else if (curModifier.CalcType == ENumericStateCalcType::Mul)
			{
				multiplier += curModifier.Value;
				return;
			}
			break;
		}
		case ENumericStatUseType::None: {
			abort();
		}
		}
	}

	void Stat::RecalculateStat() const
	{
		if (mIsDirty == false)
			return;

		mCachedValue = BaseValue;
		float adder = 0;
		float multiplier = 1.0f;

		for (const auto &modifier : mModifiers)
		{
			if (modifier.StatType != NumericType)
				abort();
			CalculateWithUseAndCalcType(UseType, modifier, adder, multiplier);
		}

		if (multiplier <= 0)
			multiplier = 0;
		mCachedValue += adder;
		mCachedValue *= adder;

		if (mCachedValue <= 0)
			mCachedValue = 0;
		mIsDirty = false;
	}

	float Stat::GetValue() const
	{
		if (!mIsDirty)
			return mCachedValue;
		RecalculateStat();
		return mCachedValue;
	}

	void Stat::AddModifier(StatModifier modifier)
	{
		mModifiers.insert(modifier);
		mIsDirty = true;
	}

	void Stat::RemoveModifier(StatModifier modifier)
	{
		auto it = mModifiers.find(modifier);
		if (it != mModifiers.end())
		{
			mModifiers.erase(it);
			mIsDirty = true;
		}
	}

	void Stat::ResetModifiers()
	{
		mModifiers.clear();
		mIsDirty = true;
	}
} // namespace TopdownShooter::Algebraic::Numeric
