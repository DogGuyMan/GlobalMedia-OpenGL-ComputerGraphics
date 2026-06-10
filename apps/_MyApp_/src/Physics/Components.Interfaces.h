/**
 * @file Components.Interfaces.h
 * @brief 물리 접촉 콜백 인터페이스 IContactable - Unity MonoBehaviour 의 OnTrigger / OnCollision 콜백 매핑.
 *
 * @details
 *  ### 책임
 *  - Component 가 본 인터페이스를 구현하면 PhysicsContactListener 가 접촉 이벤트를 전달받는 진입점이 된다.
 *  - 4종 콜백(TriggerEnter/Exit, CollisionEnter/Exit) 의 의미와 Box2D 디스패치 규약 정의.
 *
 *  ### 비-책임
 *  - [X] isTrigger 여부 보유 - Components::Physics::IsSensor() 가 유일 source-of-truth (본 인터페이스에 게터 없음).
 *  - [X] 접촉 판정/디스패치 - PhysicsContactListener(.cpp) 가 b2ContactListener 콜백에서 수행.
 *
 *  ### 정통 매핑
 *  - Unity MonoBehaviour::OnTriggerEnter / OnCollisionEnter.
 *
 * @note 본 인터페이스의 콜백은 b2World::Step 잠금 중에 호출된다 (ContactListener 경유).
 *       따라서 구현체는 콜백 안에서 body 구조 변경(SetEnabled/Destroy/Create)을 하면 안 되고,
 *       마킹/큐 등록만 한 뒤 Step 밖 deferred sweep 에서 처리해야 한다 (doc/Box2DAPI.md sec 8 참고).
 */
#ifndef _TOPDOWNSHOOTER_PHYSICS_COMPONENTS_INTERFACES__
#define _TOPDOWNSHOOTER_PHYSICS_COMPONENTS_INTERFACES__

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Physics
{
	/// @brief Unity MonoBehaviour::OnTriggerEnter / OnCollisionEnter 정통 매핑.
	/// @details Component 가 이 인터페이스를 구현하면 PhysicsContactListener 가
	///          BeginContact/EndContact 시 IsSensor 분기로 콜백을 전달.
	///
	///   ### 4-콜백 의미
	///   - OnTriggerEnter(other)   : isSensor=true 영역에 진입 (관통 시작)
	///   - OnTriggerExit (other)   : isSensor=true 영역에서 이탈
	///   - OnCollisionEnter(other) : solid 충돌 발생 (벽 부딪힘 등)
	///   - OnCollisionExit (other) : solid 충돌 종료
	///
	///   ### Box2D 디스패치 규약
	///   - 하나라도 sensor  양쪽 모두 OnTriggerEnter
	///   - 둘 다 solid     양쪽 모두 OnCollisionEnter
	///   - 정적<->정적 충돌은 Box2D 가 이벤트 없음 (Unity "최소 한 쪽 Rigidbody" 규약과 동일)
	///
	///   ### isTrigger source-of-truth
	///   - `Components::Physics::IsSensor()` 가 유일 - 본 인터페이스에는 게터를 두지 않음.
	class IContactable
	{
	  protected:
		IContactable() = default;

	  public:
		virtual ~IContactable() = default;
		IContactable(const IContactable &)             = delete;
		IContactable &operator=(const IContactable &)  = delete;
		IContactable(IContactable &&)                  = delete;
		IContactable &operator=(IContactable &&)       = delete;

		virtual void OnTriggerEnter  (SJH::Scene::Actor * /*other*/) {}
		virtual void OnTriggerExit   (SJH::Scene::Actor * /*other*/) {}
		virtual void OnCollisionEnter(SJH::Scene::Actor * /*other*/) {}
		virtual void OnCollisionExit (SJH::Scene::Actor * /*other*/) {}
	};
}; // namespace TopdownShooter::Physics

#endif //_TOPDOWNSHOOTER_PHYSICS_COMPONENTS_INTERFACES__
