#ifndef __MYAPP_PHYSICS_CONTACT_INTERFACE_H__
#define __MYAPP_PHYSICS_CONTACT_INTERFACE_H__

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
    ///   - 하나라도 sensor → 양쪽 모두 OnTriggerEnter
    ///   - 둘 다 solid    → 양쪽 모두 OnCollisionEnter
    ///   - 정적↔정적 충돌은 Box2D 가 이벤트 없음 (Unity "최소 한 쪽 Rigidbody" 규약과 동일)
    class IPhysicsContactListener
    {
    protected:
        IPhysicsContactListener() = default;

    public:
        virtual ~IPhysicsContactListener() = default;
        IPhysicsContactListener(const IPhysicsContactListener&)            = delete;
        IPhysicsContactListener& operator=(const IPhysicsContactListener&) = delete;
        IPhysicsContactListener(IPhysicsContactListener&&)                 = delete;
        IPhysicsContactListener& operator=(IPhysicsContactListener&&)      = delete;

        virtual void OnTriggerEnter   (SJH::Scene::Actor* /*other*/) {}
        virtual void OnTriggerExit    (SJH::Scene::Actor* /*other*/) {}
        virtual void OnCollisionEnter (SJH::Scene::Actor* /*other*/) {}
        virtual void OnCollisionExit  (SJH::Scene::Actor* /*other*/) {}
    };
}

#endif // __MYAPP_PHYSICS_CONTACT_INTERFACE_H__
