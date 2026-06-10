/**
 * @file Algebraic.Common.h
 * @brief 게임 수치(Stat) 도메인의 공용 열거형 정의 - 수치 해석/연산/종류 분류.
 *
 * @details
 *  ### 책임
 *  - 수치 해석 방식(@c ENumericStatUseType) - 값을 자연수/비율/백분율 중 무엇으로 볼지.
 *  - 수치 연산 방식(@c ENumericStateCalcType) - Modifier 를 합산할지 곱셈으로 누적할지.
 *  - 수치 종류(@c ENumericStatType) - MaxHp/Power/MoveSpeed 등 스탯 식별자 도메인.
 *
 *  ### 비-책임
 *  - [X] 실제 수치 계산/캐시 - @c Stat 담당.
 *  - [X] Modifier 값 보관 - @c StatModifier 담당.
 *
 *  ### 정통 매핑
 *  - Unity GAS(Gameplay Ability System) 의 AttributeType / ModifierOp 분류와 대응.
 *
 * @note 모든 enum 의 @c None = -1 은 미설정/무효 sentinel.
 *       @c ENumericStatType 의 값에 매겨진 구간(0/10/20/30/40/50)은 스탯 카테고리 그룹 경계.
 */
#ifndef _TOPDOWNSHOOTER_ALGEBRAIC_COMMON__
#define _TOPDOWNSHOOTER_ALGEBRAIC_COMMON__

namespace TopdownShooter::Algebraic
{
	/// @brief 스탯 값의 해석 방식 - 자연수/비율/백분율 중 어떻게 다룰지 구분.
	enum class ENumericStatUseType : int
	{
		None = -1,    ///< 미설정/무효 sentinel.
		Natural = 0,  ///< 자연수 값으로 해석 (예: HP 100).
		Ratio,        ///< 비율 값으로 해석 (예: 1.5 배).
		Percentage    ///< 백분율 값으로 해석 (예: 50%).
	};

	/// @brief Modifier 누적 시 연산 방식 - 덧셈으로 모을지 곱셈으로 모을지.
	enum class ENumericStateCalcType : int
	{
		None = -1,  ///< 미설정/무효 sentinel.
		Add = 0,    ///< 덧셈 누적 (adder 에 합산).
		Mul = 1     ///< 곱셈 누적 (multiplier 에 합산).
	};

	/// @brief 스탯 종류 식별자 - 카테고리별 구간(0/10/20/30/40/50)으로 묶인 도메인.
	enum class ENumericStatType : int
	{
		None = -1,  ///< 미설정/무효 sentinel.

		// -- 기본 전투/이동 스탯 (0번대) --
		MaxHp = 0,   ///< 최대 체력.
		Defence,     ///< 방어력.
		Power,       ///< 공격력.
		MoveSpeed,   ///< 이동 속도.
		Accecerate,  ///< 가속도.
		Tenacity,    ///< 강인함(경직 저항).

		// -- 스태미나 계열 (10번대) --
		MaxStamina = 10,      ///< 최대 스태미나.
		StaminaRestoreSpeed,  ///< 스태미나 회복 속도.
		DashForce,            ///< 대시 추진력.

		// -- 투사체/소환체 배율 (20번대) --
		InstantiableDurateLifeTimeMultiplyRatio = 20,  ///< 소환체 지속 시간 배율.
		InstantiableSizeMultiplyRatio,                 ///< 소환체 크기 배율.
		InstantiableForwardingSpeedMultiplyRatio,      ///< 소환체 전진 속도 배율.

		// -- 공격/풀링 계열 (30번대) --
		PoolSize = 30,  ///< 오브젝트 풀 크기.
		AttackSpeed,    ///< 공격 속도.
		MeleeRatio,     ///< 근접 계수.
		RangerRatio,    ///< 원거리 계수.
		TechRatio,      ///< 기술 계수.

		// -- 효율/쿨다운 계열 (40번대) --
		EfficienceMultiplyer = 40,  ///< 효율 배율.
		CoolDownSpeed,              ///< 쿨다운 속도.

		// -- 기타 (50번대) --
		Luck = 50  ///< 행운.
	};
}; // namespace TopdownShooter::Algebraic::Numeric
#endif //_TOPDOWNSHOOTER_ALGEBRAIC_COMMON__