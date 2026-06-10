/**
 * @file Stat.cpp
 * @brief Stat 의 lazy-cache 재계산 로직과 Modifier 관리 구현.
 *
 * @details
 *  ### 책임
 *  - 생성자에서 캐시를 BaseValue 로 초기화.
 *  - @c RecalculateStat 의 adder/multiplier 합성.
 *  - Modifier add/remove/reset 시 dirty 전이.
 *
 * @note 잘못된 입력(UseType=None, 다른 StatType Modifier 혼입)은 @c abort() 로 즉시 중단 -
 *       프로그래밍 계약 위반을 조용히 넘기지 않는다.
 */
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

	// 단일 Modifier 를 UseType/CalcType 분기에 따라 adder 또는 multiplier 에 누적한다.
	// 현재 세 UseType(Natural/Ratio/Percentage) 의 누적 규칙은 동일하며, None 은 계약 위반으로 abort.
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
