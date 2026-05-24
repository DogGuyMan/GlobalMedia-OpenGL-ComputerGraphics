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
		float mCachedValue;
		bool mIsDirty;
		std::set<StatModifier> mModifiers;

		void CalculateWithUseAndCalcType(ENumericStatUseType use_type, StatModifier curModifier, float &adder, float &multiplier)
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

		void RecalculateStat()
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

	  public:
		const float BaseValue;
		const ENumericStatUseType UseType;
		const ENumericStatType NumericType;

		Stat(float base_value, ENumericStatUseType use_type, ENumericStatType numeric_type)
		    : BaseValue(base_value), UseType(use_type), NumericType(numeric_type)
		{
			mCachedValue = base_value;
			mIsDirty = false;
		}

		float GetValue() 
		{
			if (!mIsDirty)
				return mCachedValue;
			RecalculateStat();
			return mCachedValue;
		}

		void AddModifier(StatModifier modifier)
		{
			mModifiers.insert(modifier);
			mIsDirty = true;
		}

		void RemoveModifier(StatModifier modifier)
		{
			auto it = mModifiers.find(modifier);
			if (it != mModifiers.end())
			{
				mModifiers.erase(it);
				mIsDirty = true;
			}
		}

		void ResetModifiers()
		{
			mModifiers.clear();
			mIsDirty = true;
		}
	};
}; // namespace TopdownShooter::Algebraic::Numeric

#endif //_TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT__