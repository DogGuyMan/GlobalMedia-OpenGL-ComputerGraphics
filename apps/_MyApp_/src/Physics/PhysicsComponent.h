#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENT__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENT__
#include "Physics/Components.Interfaces.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <cstdint>

namespace TopdownShooter::Physics::Components
{
	/// @brief b2Body* non-owning 보유 abstract base. PhysicsSystem 의 b2World 가 lifetime 소유.
	/// @details
	///   ### 좌표계 (spec §4.4)
	///   - 물리: b2Vec2(x, y) 2D XY 평면
	///   - 렌더: Transform.Translate = vmath::vec3(x, heightOffset, -y) 3D XZ 평면 (top-down)
	///
	///   ### Sensor (Unity Collider.isTrigger 매핑)
	///   - mIsSensor=true   b2Fixture::SetSensor(true). 물리 충돌 없음, 이벤트만.
	///   - mIsSensor=false  b2Fixture::SetSensor(false) (default). 물리 충돌 + 이벤트.
	///
	///   ### isTrigger 단일 source-of-truth
	///   - mIsSensor 가 유일한 진실. 런타임 변경은 SetSensor(bool) 만.
	///
	///   ### 형태별 구체 구현
	///   - CircleBody / BoxBody (PhysicsComponent.Imp.h) — 구체 형태 태그.
	///     본 base 가 b2Body 라이프사이클을 담당, shape 생성은 factory 책임.
	class Physics : public SJH::Scene::Component,
	                public IContactable
	{
	  private:
		b2Body *mBody         = nullptr;
		float   mHeightOffset = 0.0f;
		bool    mIsSensor     = false;

	  public:
		Physics() = default;

		/// @brief b2Body* 주입 — b2World::CreateBody 결과. user data 에 owner Actor* 자동 등록.
		Physics &SetBody(b2Body *body)
		{
			mBody = body;
			if (mBody)
				mBody->GetUserData().pointer = reinterpret_cast<uintptr_t>(GetOwner());
			return *this;
		}

		Physics &SetHeightOffset(float y)
		{
			mHeightOffset = y;
			return *this;
		}

		/// @brief Unity Collider.isTrigger 와 동일. 런타임 변경 시 모든 fixture 에 전파.
		Physics &SetSensor(bool s)
		{
			mIsSensor = s;
			if (mBody) {
				for (b2Fixture *f = mBody->GetFixtureList(); f; f = f->GetNext())
					f->SetSensor(s);
			}
			return *this;
		}

		b2Body *GetBody()         const { return mBody; }
		float   GetHeightOffset() const { return mHeightOffset; }
		bool    IsSensor()        const { return mIsSensor; }   // isTrigger source-of-truth

		virtual void OnEnter() = 0;
		virtual void OnExit() = 0;
		virtual void Update(float dt) = 0;
	};

	/// @brief Actor 의 Component 중 첫 번째 Physics-derived 를 polymorphic 으로 검색.
	/// @details Actor::GetComponent<Physics> 도 이제 polymorphic 조회 가능(게이트 완화).
	///          단 첫 매칭만 반환하므로, 다중 Physics 가능성 + 명시적 의도를 위해 본 헬퍼 유지.
	inline Physics *FindPhysics(SJH::Scene::Actor *actor)
	{
		if (!actor) return nullptr;
		Physics *found = nullptr;
		actor->ForEachComponent([&](SJH::Scene::Component *c) {
			if (!found) {
				if (auto *p = dynamic_cast<Physics *>(c))
					found = p;
			}
		});
		return found;
	}
}; // namespace TopdownShooter::Physics::Components

#endif //_TOPDOWNSHOOTER_PHYSICS_COMPONENT__
