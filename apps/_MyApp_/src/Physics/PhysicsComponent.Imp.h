#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
#include "Physics/PhysicsComponent.h"

namespace TopdownShooter::Physics::Components
{
	/// @brief Circle shape 물리 Component — b2CircleShape 형태 태그.
	/// @details base Physics 가 b2Body 라이프사이클을 담당. shape 생성/등록은 factory 책임.
	///          IContactable 의 4 콜백은 default empty {} 이므로 필요시에만 override.
	class CircleBody : public Physics
	{
	  public:
		void OnEnter() override {}
		void OnExit()  override {}
		void Update(float /*dt*/) override {}
	};

	/// @brief Box shape 물리 Component — b2PolygonShape::SetAsBox 형태 태그.
	/// @details 현재 Player / Wall / Pickup 모두 box 형태로 등록.
	class BoxBody : public Physics
	{
	  public:
		void OnEnter() override {}
		void OnExit()  override {}
		void Update(float /*dt*/) override {}
	};
}; // namespace TopdownShooter::Physics::Components

#endif //_TOPDOWNSHOOTER_PHYSICS_COMPONENT_IMP__
