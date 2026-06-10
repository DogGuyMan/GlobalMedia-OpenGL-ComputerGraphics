/**
 * @file Stat.h
 * @brief 기본값 + Modifier 집합을 합성해 최종 수치를 산출하는 게임 스탯 클래스.
 *
 * @details
 *  ### 책임
 *  - 기본값(@c BaseValue) 위에 @c StatModifier 집합을 합성해 최종 값 산출.
 *  - lazy-cache - @c GetValue() 호출 시 dirty 일 때만 재계산하고 결과를 캐시.
 *  - Modifier 추가/제거/초기화 시 dirty 플래그 set.
 *
 *  ### 비-책임
 *  - [X] Modifier 값 보관 형식 - @c StatModifier 담당.
 *  - [X] 스탯 종류/연산 분류 - @c Algebraic.Common.h 의 enum 담당.
 *
 *  ### 정통 매핑
 *  - Unity GAS 의 GameplayAttribute - BaseValue + Modifier 합성으로 CurrentValue 계산.
 *
 * @note @c BaseValue / @c UseType / @c NumericType 은 const - 생성 후 불변.
 *       lazy-cache 때문에 @c mCachedValue / @c mIsDirty 는 mutable (const 메서드 내 갱신).
 */
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
	/**
	 * @brief 기본값과 Modifier 집합을 합성해 최종 수치를 내는 스탯.
	 * @details @c GetValue() 가 dirty 일 때만 재계산하는 lazy-cache 패턴.
	 *          Modifier 는 @c Order 오름차순(@c std::set)으로 누적된다.
	 */
	class Stat
	{
	  private:
		// lazy-cache 패턴 - GetValue() const 안에서 RecalculateStat() 호출하므로 mutable.
		mutable float mCachedValue;            ///< 마지막 재계산 결과 캐시 (lazy-cache).
		mutable bool mIsDirty;                 ///< true 면 다음 GetValue() 에서 재계산 필요.
		std::set<StatModifier> mModifiers;     ///< Order 오름차순 정렬된 Modifier 집합.

		/// @brief 단일 Modifier 를 UseType/CalcType 에 따라 adder/multiplier 에 누적.
		/// @param use_type    스탯 해석 방식 (Natural/Ratio/Percentage).
		/// @param curModifier 누적할 Modifier.
		/// @param adder       [in,out] 덧셈 누적기.
		/// @param multiplier  [in,out] 곱셈 누적기.
		void CalculateWithUseAndCalcType(ENumericStatUseType use_type, StatModifier curModifier, float &adder, float &multiplier) const;

		/// @brief dirty 일 때 BaseValue + 전체 Modifier 를 합성해 mCachedValue 재계산.
		/// @details 재계산 후 @c mIsDirty 를 false 로 내린다. const 메서드지만 mutable 멤버를 갱신.
		void RecalculateStat() const;

	  public:
		const float BaseValue;                  ///< 기본값 (불변).
		const ENumericStatUseType UseType;      ///< 수치 해석 방식 (불변).
		const ENumericStatType NumericType;     ///< 스탯 종류 (불변).

		/// @brief 기본값/해석방식/종류를 받아 스탯 생성. 초기 캐시는 base_value, dirty=false.
		/// @param base_value   기본값.
		/// @param use_type     수치 해석 방식.
		/// @param numeric_type 스탯 종류.
		Stat(float base_value, ENumericStatUseType use_type, ENumericStatType numeric_type);

		/// @brief 최종 수치 조회 - dirty 면 재계산 후 캐시 반환.
		/// @return Modifier 합성이 반영된 최종 스탯 값.
		float GetValue() const;

		/// @brief Modifier 추가 후 dirty set.
		/// @param modifier 추가할 Modifier.
		void AddModifier(StatModifier modifier);

		/// @brief Modifier 제거 후 dirty set (존재할 때만).
		/// @param modifier 제거할 Modifier.
		void RemoveModifier(StatModifier modifier);

		/// @brief 모든 Modifier 제거 후 dirty set.
		void ResetModifiers();
	};
}; // namespace TopdownShooter::Algebraic::Numeric

#endif //_TOPDOWNSHOOTER_ALGEBRAIC_NUMERIC_STAT__
