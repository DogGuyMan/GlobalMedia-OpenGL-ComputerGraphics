#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
#include "Physics/PhysicsComponent.h"

namespace TopdownShooter::Physics::Components
{
	/// @brief Circle shape 물리 Component — ctor 에서 b2CircleShape body 생성(eager).
	/// @details base Physics 가 b2Body 라이프사이클을 담당. owner userdata 등록은 base OnEnter.
	class CircleBody : public Physics
	{
	  public:
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

	/// @brief Box shape 물리 Component — ctor 에서 b2PolygonShape::SetAsBox body 생성(eager).
	/// @details 현재 Player / Wall / Pickup 모두 box 형태로 등록.
	class BoxBody : public Physics
	{
	  public:
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
