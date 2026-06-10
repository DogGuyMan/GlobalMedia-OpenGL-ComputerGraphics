/**
 * @file PhysicsComponent.h
 * @brief Box2D b2Body 를 감싸는 Physics Component abstract base + FindPhysics 헬퍼.
 *
 * @details
 *  ### 책임
 *  - @c b2Body* non-owning 보유 + 라이프사이클 관리 (OnEnter: userdata 등록, OnExit: DestroyBody).
 *  - @c IContactable 인터페이스 구현 게이트 제공 (BeginContact/EndContact).
 *  - @c BodyConfig PoD 구조체로 body + fixture 공통 생성 파라미터 캡슐화.
 *  - @c FindPhysics(Actor*) 헬퍼: Actor 컴포넌트 목록을 순회해 Physics 파생 타입 polymorphic 검색.
 *
 *  ### 비-책임
 *  - [X] b2World 소유 및 Step 구동 -- @c PhysicsSystem 담당.
 *  - [X] 구체 shape (Circle/Box) 생성 -- @c CircleBody / @c BoxBody (PhysicsComponent.Imp.h) 담당.
 *  - [X] ContactListener 등록 -- @c PhysicsSystem::Init 에서 설치.
 *
 *  ### 정통 매핑
 *  - Unity @c Collider + @c Rigidbody2D 의 결합체 (isTrigger/isSensor, SetEnabled).
 *
 * @note b2Body::SetEnabled / DestroyBody / CreateBody 는 b2World::Step 잠금 중(ContactListener
 *       콜백 포함) 호출 금지 -> abort. 콜백에서는 enqueue 만 수행하고 Step 종료 후 sweep 에서 실행.
 *       (doc/Box2DAPI.md sec.8 참조)
 */
#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENT__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENT__
#include "Physics/Components.Interfaces.h"
#include "scene/actor.h"
#include <box2d/box2d.h>
#include <cstdint>
#include <vmath.h>

namespace TopdownShooter::Physics::Components
{
	/**
	 * @brief body+fixture 생성 파라미터 (b2BodyDef + fixture 공통부). shape 는 subtype 책임.
	 * @details 5 call-site 모두 커버 -- dynamic(Player/Enemy) / static(Wall/Pickup) / kinematic+velocity(Bullet).
	 *  @c b2World::CreateBody + @c b2Body::CreateFixture 에 전달되는 설정을 한 곳에 모아
	 *  factory(CircleBody/BoxBody) 에 주입한다.
	 */
	struct BodyConfig
	{
		b2World*    world          = nullptr;       ///< 생성 대상 b2World (nullptr 이면 MakeBody 가 nullptr 반환).
		b2BodyType  bodyType       = b2_dynamicBody;             ///< Wall/Pickup=static, Bullet=kinematic.
		vmath::vec2 startPosition  = vmath::vec2(0.0f, 0.0f);   ///< 물리 2D 평면 초기 위치.
		vmath::vec2 linearVelocity = vmath::vec2(0.0f, 0.0f);   ///< Bullet 초기 속도 (그 외 0).
		float       linearDamping  = 0.0f;          ///< 공기 저항 계수 (0 = 감쇠 없음).
		float       density        = 1.0f;          ///< 단위 면적당 질량 (b2FixtureDef.density).
		float       friction       = 0.2f;          ///< b2FixtureDef 기본값 -- friction 미설정 site(벽/픽업/총알) 보존.
		bool        isSensor       = false;         ///< true 면 물리 충돌 없이 이벤트만 (Unity Collider.isTrigger).
		uint16_t    categoryBits   = 0;             ///< 이 body 가 속한 레이어 비트 (PhysicsLayer -> ToBits 변환).
		uint16_t    maskBits       = 0;             ///< 충돌 대상 레이어 비트 (이 body 가 맞을 상대 레이어 조합).
		float       heightOffset   = 0.0f;          ///< 렌더 Y 오프셋 (물리 2D -> 렌더 3D XZ 변환 시 tr.y 에 사용).
	};

	/**
	 * @brief b2Body* non-owning 보유 Physics Component abstract base.
	 * @details
	 *  @c b2Body 의 lifetime 은 @c b2World 가 소유한다. 본 클래스는 포인터만 보관하며,
	 *  @c OnExit 에서 @c b2World::DestroyBody 를 명시 호출해 body 를 해제한다.
	 *
	 *  ### 좌표계 (spec sec.4.4)
	 *  - 물리: b2Vec2(x, y) 2D XY 평면
	 *  - 렌더: Transform.Translate = vmath::vec3(x, heightOffset, -y) 3D XZ 평면 (top-down)
	 *
	 *  ### Sensor (Unity Collider.isTrigger 매핑)
	 *  - @c mIsSensor=true  -> @c b2Fixture::SetSensor(true). 물리 충돌 없음, 이벤트만.
	 *  - @c mIsSensor=false -> @c b2Fixture::SetSensor(false) (기본값). 물리 충돌 + 이벤트.
	 *
	 *  ### isTrigger 단일 source-of-truth
	 *  - @c mIsSensor 가 유일한 진실. 런타임 변경은 @c SetSensor(bool) 만 사용.
	 *
	 *  ### 형태별 구체 구현
	 *  - @c CircleBody / @c BoxBody (PhysicsComponent.Imp.h) -- 구체 shape 생성 담당.
	 *    본 base 가 b2Body 라이프사이클을 담당, shape 생성은 각 concrete factory 책임.
	 *
	 * @note b2Body::SetEnabled 는 b2World::Step 잠금 중 호출 금지(abort). ContactListener
	 *       콜백에서는 flag/queue enqueue 만 하고 Step 종료 후 sweep 에서 SetBodyEnabled 호출.
	 */
	class Physics : public SJH::Scene::Component,
	                public IContactable
	{
	  private:
		b2Body *mBody         = nullptr;  ///< b2World 소유 body 포인터 (비소유, OnExit 에서 DestroyBody 호출).
		float   mHeightOffset = 0.0f;     ///< 렌더 3D Y 좌표 오프셋 (물리 2D -> XZ 변환 시 tr.y 값).
		bool    mIsSensor     = false;    ///< isTrigger source-of-truth. true 면 이벤트만, 물리 반응 없음.

	  public:
		Physics() = default;

		/// @brief b2Body* 주입 - b2World::CreateBody 결과. user data 에 owner Actor* 자동 등록.
		Physics &SetBody(b2Body *body)
		{
			mBody = body;
			if (mBody)
				mBody->GetUserData().pointer = reinterpret_cast<uintptr_t>(GetOwner());
			return *this;
		}

		/// @brief 렌더 Y 오프셋 설정. SyncToTransform 에서 tr.y 에 적용됨.
		/// @param y 오프셋 값 (단위: 렌더 월드 유닛).
		/// @return *this (fluent builder).
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

		/// @brief 내부 b2Body 포인터 반환. OnExit 이후 nullptr.
		b2Body *GetBody()         const { return mBody; }
		/// @brief 렌더 Y 오프셋 반환.
		float   GetHeightOffset() const { return mHeightOffset; }
		/// @brief isTrigger source-of-truth. true 면 sensor(이벤트만), false 면 물리 충돌 활성.
		bool    IsSensor()        const { return mIsSensor; }

		// -- body 생성 (subtype ctor 가 호출, eager). owner 무관 -- 등록은 OnEnter. --
	  protected:
		/// @brief b2BodyDef -> CreateBody (fixture 없음). subtype ctor 가 호출 후 shape fixture 추가.
		/// @param cfg body 생성 파라미터. @c cfg.world 가 nullptr 이면 nullptr 반환.
		/// @return 생성된 @c b2Body* (실패 시 nullptr).
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
		/// @brief 생성된 body 를 멤버에 등록(owner userdata 제외 -- OnEnter 가 담당). subtype ctor 말미 호출.
		/// @param body  @c MakeBody 결과. nullptr 허용 (isSensor/heightOffset 은 여전히 기록됨).
		/// @param cfg   생성 파라미터 (isSensor, heightOffset 추출용).
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
		/// @brief 액터 제거(despawn) 시 b2Body 완전 파괴 - b2World 가 소유하므로 명시 DestroyBody.
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
	/// @details @c Actor::GetComponent<Physics> 도 polymorphic 조회 가능(게이트 완화).
	///          단 첫 매칭만 반환하므로, 다중 Physics 가능성 + 명시적 의도를 위해 본 헬퍼 유지.
	/// @param actor 검색 대상 Actor. nullptr 이면 nullptr 반환.
	/// @return 첫 번째 @c Physics 파생 Component 포인터. 없으면 nullptr.
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
