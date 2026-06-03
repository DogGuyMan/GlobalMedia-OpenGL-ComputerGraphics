#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENT__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENT__
#include "Physics/Components.Interfaces.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <cstdint>
#include <vmath.h>

namespace TopdownShooter::Physics::Components
{
	/// @brief body+fixture 생성 파라미터 (b2BodyDef + fixture 공통부). shape 는 subtype 책임.
	/// @details 5 call-site 모두 커버 — dynamic(Player/Enemy) / static(Wall/Pickup) / kinematic+velocity(Bullet).
	struct BodyConfig
	{
		b2World*    world          = nullptr;
		b2BodyType  bodyType       = b2_dynamicBody;             // Wall/Pickup=static, Bullet=kinematic
		vmath::vec2 startPosition  = vmath::vec2(0.0f, 0.0f);
		vmath::vec2 linearVelocity = vmath::vec2(0.0f, 0.0f);    // Bullet 초기 속도 (그 외 0)
		float       linearDamping  = 0.0f;
		float       density        = 1.0f;
		float       friction       = 0.2f;                       // b2FixtureDef 기본값 — friction 미설정 site(벽/픽업/총알) 보존
		bool        isSensor       = false;
		uint16_t    categoryBits   = 0;
		uint16_t    maskBits       = 0;
		float       heightOffset   = 0.0f;
	};

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

		/// @brief b2Body 시뮬레이션 참여 on/off (Box2D b2Body::SetEnabled). 사망 시 *충돌만* 정지(시각 디졸브는 유지)에 사용.
		Physics &SetBodyEnabled(bool e)
		{
			if (mBody) mBody->SetEnabled(e);
			return *this;
		}

		b2Body *GetBody()         const { return mBody; }
		float   GetHeightOffset() const { return mHeightOffset; }
		bool    IsSensor()        const { return mIsSensor; }   // isTrigger source-of-truth

		// ── body 생성 (subtype ctor 가 호출, eager). owner 무관 — 등록은 OnEnter. ──
	  protected:
		/// @brief b2BodyDef -> CreateBody (fixture 없음). subtype ctor 가 호출 후 shape fixture 추가.
		b2Body* MakeBody(const BodyConfig& cfg)
		{
			if (cfg.world == nullptr) return nullptr;
			b2BodyDef bd;
			bd.type          = cfg.bodyType;
			bd.position.Set(cfg.startPosition[0], cfg.startPosition[1]);
			bd.linearDamping = cfg.linearDamping;
			bd.linearVelocity.Set(cfg.linearVelocity[0], cfg.linearVelocity[1]);
			return cfg.world->CreateBody(&bd);
		}
		/// @brief 생성된 body 를 멤버에 등록(owner 제외 — OnEnter 가 담당). subtype ctor 말미 호출.
		void InitBody(b2Body* body, const BodyConfig& cfg)
		{
			mBody         = body;
			mIsSensor     = cfg.isSensor;
			mHeightOffset = cfg.heightOffset;
		}

	  public:
		/// @brief owner userdata 등록 (ctor 엔 GetOwner=null -> 여기서). contact 콜백은 런타임에만 읽음.
		void OnEnter() override
		{
			if (mBody != nullptr)
				mBody->GetUserData().pointer = reinterpret_cast<uintptr_t>(GetOwner());
		}
		/// @brief 액터 제거(despawn) 시 b2Body 완전 파괴 — b2World 가 소유하므로 명시 DestroyBody.
		///        (Update 가 pure 라 Physics 는 여전히 abstract. subtype 의 빈 OnExit override 는 제거됨.)
		void OnExit() override
		{
			if (mBody != nullptr)
			{
				mBody->GetWorld()->DestroyBody(mBody);
				mBody = nullptr;
			}
		}
		void Update(float dt) override = 0;
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
