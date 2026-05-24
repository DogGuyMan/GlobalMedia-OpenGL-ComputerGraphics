#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__

#include "Components.Interfaces.h"
#include "scene/actor.h"
#include <string>

namespace TopdownShooter::Entity::Components
{
	class Weapon
	{
	  private:
		Weapon(int damage, const char *literal_str) : 
		        Damage(damage),
		        WeaponName(literal_str)
		{
		}

	  public:
		const int Damage;
		const std::string WeaponName;

		void ShowInfo() const;
	};
} // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__