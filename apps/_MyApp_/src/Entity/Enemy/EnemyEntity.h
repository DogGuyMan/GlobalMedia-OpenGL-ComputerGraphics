#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_ENTITY_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_ENTITY_H__

#include "Entity/BaseEntity.h"

namespace TopdownShooter::Entity::Enemy
{
	/// @brief 적 Accessor-facade — BaseEntity(Life/Physics/Director/Impulse) 만으로 충분.
	///        (적 전용 능력 생기면 여기 확장.) header-only — OnEnter 는 BaseEntity 상속.
	class EnemyEntity : public TopdownShooter::Entity::BaseEntity
	{
	};
} // namespace TopdownShooter::Entity::Enemy

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_ENTITY_H__
