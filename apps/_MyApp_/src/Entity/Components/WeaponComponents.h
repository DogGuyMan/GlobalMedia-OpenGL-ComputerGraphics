#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__

#include "Algebraic/Stat.h"
#include "Components.Interfaces.h"
#include "scene/actor.h"
#include <string>

namespace TopdownShooter::Entity::Components
{
	class Weapon
	{
	  private:
		Weapon(int damage, const char *literal_str)
		    : Damage(static_cast<float>(damage), Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::Power),
		      WeaponName(literal_str)
		{
		}

	  public:
		// Damage 는 Stat 으로 통합 — modifier 시스템 적용 가능 (NumericType::Power, UseType::Natural).
		// 기존 `const int Damage` 의 const 는 제거 — Stat 자체 BaseValue 가 const 라 base 값은 여전히 불변,
		// 그러나 멤버는 modifier 추가/제거가 가능해야 하므로 non-const.
		Algebraic::Numeric::Stat Damage;
		const std::string WeaponName;

		void ShowInfo() const;
	};
} // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__
