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
	///   - 정적↔정적 충돌은 Box2D 가 이벤트 없음 (Unity "최소 한 쪽 Rigidbody" 규약과 동일)
	///
	///   ### isTrigger source-of-truth
	///   - `Components::Physics::IsSensor()` 가 유일 — 본 인터페이스에는 게터를 두지 않음.
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
