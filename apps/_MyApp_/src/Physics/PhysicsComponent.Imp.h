/**
 * @file PhysicsComponent.Imp.h
 * @brief Box2D 구체 shape Component 구현 -- CircleBody / BoxBody.
 *
 * @details
 *  ### 책임
 *  - @c Physics abstract base 를 상속해 각 shape(circle/box) 에 맞는 b2Fixture 를 생성.
 *  - ctor 에서 @c MakeBody + fixture 부착 + @c InitBody 를 즉시(eager) 수행 ->
 *    AddComponent 시점에 body 가 이미 생성된 상태.
 *  - @c Update 는 빈 구현 (body 동기화는 @c PhysicsSystem::SyncToTransform 이 담당).
 *
 *  ### 비-책임
 *  - [X] b2World 소유 및 Step -- @c PhysicsSystem 담당.
 *  - [X] Transform 동기화 -- @c PhysicsSystem::SyncToTransform 담당.
 *  - [X] owner userdata 등록 -- base @c Physics::OnEnter 담당 (ctor 시점에는 GetOwner=null).
 *
 *  ### 정통 매핑
 *  - Unity @c CircleCollider2D / @c BoxCollider2D + @c Rigidbody2D 결합.
 *
 * @note ctor 에서 body 를 즉시 생성하므로 @c BodyConfig::world 가 nullptr 이면
 *       body=nullptr 상태로 남는다. @c Physics::OnExit 에서 DestroyBody 가 nullptr 체크를 수행.
 */
#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
#include "Physics/PhysicsComponent.h"

namespace TopdownShooter::Physics::Components
{
	/**
	 * @brief Circle shape 물리 Component -- ctor 에서 b2CircleShape body 를 즉시(eager) 생성.
	 * @details base @c Physics 가 b2Body 라이프사이클을 담당. owner userdata 등록은 base @c OnEnter.
	 *  현재 사용처: 총알(Bullet) 및 원형 pickup 등.
	 */
	class CircleBody : public Physics
	{
	  public:
		/// @brief b2CircleShape + BodyConfig 로 body 즉시 생성.
		/// @param cfg    body 공통 설정 (world, bodyType, position, sensor, filter 등).
		/// @param radius 원 반지름 (물리 단위, b2CircleShape::m_radius 에 직접 대입).
		CircleBody(const BodyConfig& cfg, float radius)
		{
			b2Body* body = MakeBody(cfg);
			if (body != nullptr)
			{
				b2CircleShape circle;
				circle.m_radius = radius;
				b2FixtureDef fd;
				fd.shape               = &circle;
				fd.density             = cfg.density;
				fd.friction            = cfg.friction;
				fd.isSensor            = cfg.isSensor;
				fd.filter.categoryBits = cfg.categoryBits;
				fd.filter.maskBits     = cfg.maskBits;
				body->CreateFixture(&fd);
			}
			InitBody(body, cfg);
		}
		void Update(float /*dt*/) override {}
	};

	/**
	 * @brief Box shape 물리 Component -- ctor 에서 b2PolygonShape::SetAsBox body 를 즉시(eager) 생성.
	 * @details 현재 Player / Enemy / Wall / Pickup 모두 box 형태로 등록.
	 *  @c SetAsBox(size[0]*0.5, size[1]*0.5) 로 halfExtents 를 설정한다.
	 */
	class BoxBody : public Physics
	{
	  public:
		/// @brief b2PolygonShape(SetAsBox) + BodyConfig 로 body 즉시 생성.
		/// @param cfg  body 공통 설정 (world, bodyType, position, sensor, filter 등).
		/// @param size 박스 전체 크기 (반폭 = size[0]*0.5, 반높이 = size[1]*0.5 로 변환됨).
		BoxBody(const BodyConfig& cfg, vmath::vec2 size)
		{
			b2Body* body = MakeBody(cfg);
			if (body != nullptr)
			{
				b2PolygonShape box;
				box.SetAsBox(size[0] * 0.5f, size[1] * 0.5f);
				b2FixtureDef fd;
				fd.shape               = &box;
				fd.density             = cfg.density;
				fd.friction            = cfg.friction;
				fd.isSensor            = cfg.isSensor;
				fd.filter.categoryBits = cfg.categoryBits;
				fd.filter.maskBits     = cfg.maskBits;
				body->CreateFixture(&fd);
			}
			InitBody(body, cfg);
		}
		void Update(float /*dt*/) override {}
	};
}; // namespace TopdownShooter::Physics::Components

#endif //_TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
