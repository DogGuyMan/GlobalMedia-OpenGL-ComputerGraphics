/**
 * @file EnemyEntity.h
 * @brief 적 엔티티 Accessor-facade - BaseEntity 의 공통 능력(Life/Physics/Director/Impulse) 을 그대로 노출.
 *
 * @details
 *  ### 책임
 *  - 적 Actor 에 부착되어 형제 Component 묶음(Life/Physics/Director/Impulse) 의 접근 진입점 역할.
 *  - 현재 적 전용 동작이 없으므로 BaseEntity 를 빈 파생으로 상속만 한다 (마커 타입).
 *
 *  ### 비-책임
 *  - [X] 추적 AI - @c SimplePursueAI 담당.
 *  - [X] Actor 조립/Component 부착 - @c EnemyFactory (CreateEnemyActor) 담당.
 *
 *  ### 정통 매핑
 *  - Unity 의 컴포넌트 묶음 위에 올리는 도메인 facade - 형제 Component 캐시 + verb 노출 패턴.
 *
 * @note header-only. @c OnEnter 등 hook 은 BaseEntity 상속분을 그대로 사용한다 (재정의 없음).
 */
#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_ENTITY_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_ENTITY_H__

#include "Entity/BaseEntity.h"

namespace TopdownShooter::Entity::Enemy
{
	/**
	 * @brief 적 Accessor-facade - BaseEntity(Life/Physics/Director/Impulse) 만으로 충분한 빈 파생.
	 * @details
	 *  적 전용 능력이 생기면 본 클래스에 멤버/verb 를 확장한다. 현재는 BaseEntity 가 제공하는
	 *  공통 facade(IsAlive / IsImpulseActive / Play 등) 로 충분하므로 추가 구현이 없다.
	 *  @c OnEnter 의 형제 Component 캐시는 BaseEntity 상속분으로 자동 수행된다.
	 */
	class EnemyEntity : public TopdownShooter::Entity::BaseEntity
	{
	};
} // namespace TopdownShooter::Entity::Enemy

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_ENTITY_H__
