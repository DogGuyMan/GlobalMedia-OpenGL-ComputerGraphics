/**
 * @file StatModifier.h
 * @brief Stat 에 적용되는 단일 수치 변경자(Modifier) 불변 값 객체.
 *
 * @details
 *  ### 책임
 *  - 어떤 스탯(@c StatType)에 / 어떤 연산(@c CalcType)으로 / 얼마(@c Value)를 / 어떤 순서(@c Order)로
 *    적용할지를 담는 불변 PoD.
 *  - @c std::set<StatModifier> 정렬 키 제공 (@c operator< 가 @c Order 기준).
 *
 *  ### 비-책임
 *  - [X] 실제 합산/캐시 - @c Stat 담당.
 *
 *  ### 정통 매핑
 *  - Unity GAS 의 GameplayModifierInfo / AttributeModifier 와 대응.
 *
 * @note 모든 멤버가 @c const - 생성 후 변경 불가(불변 값 객체).
 *       Buff/Debuff 한 줄을 그대로 나타낸다.
 */
#ifndef _TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT_MODIFIER__
#define _TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT_MODIFIER__
#include "Algebraic.Common.h"

namespace TopdownShooter::Algebraic::Numeric
{
	/**
	 * @brief Stat 에 적용되는 단일 수치 변경자 - 불변 값 객체.
	 * @details @c Order 오름차순으로 정렬되어 @c Stat 의 재계산 루프에서 순서대로 합산된다.
	 */
	struct StatModifier
	{
		const ENumericStateCalcType CalcType = ENumericStateCalcType::None;  ///< 연산 방식 (Add/Mul).
		const ENumericStatType StatType = ENumericStatType::None;            ///< 대상 스탯 종류.
		const float Value = 0;                                              ///< 변경 값.
		const int Order = 0;                                               ///< 적용 순서 (정렬 키).

		/// @brief 모든 필드를 받는 생성자 - 모든 멤버가 const 라 이후 변경 불가.
		/// @param calc_type  연산 방식 (Add/Mul).
		/// @param stat_type  대상 스탯 종류.
		/// @param value      변경 값.
		/// @param order      적용 순서 (std::set 정렬 키).
		StatModifier(ENumericStateCalcType calc_type, ENumericStatType stat_type, float value, int order) : CalcType(calc_type),
		                                                                                                    StatType(stat_type),
		                                                                                                    Value(value),
		                                                                                                    Order(order)
		{
		}

		/// @brief Order 기준 미만 비교 - std::set 정렬용.
		/// @param other 비교 대상 Modifier.
		/// @return 본 객체의 @c Order 가 @p other 보다 작으면 true.
		bool operator<(const StatModifier &other) const
		{
			return Order < other.Order;
		}
	};
}; // namespace TopdownShooter::Algebraic::Numeric

#endif //_TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT_MODIFIER__