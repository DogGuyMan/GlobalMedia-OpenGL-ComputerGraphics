/**
 * @file ContactListener.h
 * @brief Box2D 접촉 이벤트를 Actor 의 IContactable Component 로 중계하는 b2ContactListener 구현.
 *
 * @details
 *  ### 책임
 *  - b2World 에 1개만 설치되는 전역 접촉 리스너 (PhysicsSystem::Init 에서 SetContactListener).
 *  - 두 fixture 의 body userdata 에서 owner Actor* 를 복원하고, IsSensor 여부로
 *    Trigger/Collision 분기 후 양쪽 Actor 의 IContactable 콜백을 호출.
 *
 *  ### 비-책임
 *  - [X] 데미지/사망 등 게임 로직 - IContactable 구현체(게임 Component)의 책임.
 *
 *  ### 정통 매핑
 *  - Unity 의 물리 콜백 디스패치 계층 (Collider.isTrigger 분기).
 *
 * @warning 콜백은 b2World::Step 잠금 중에 호출된다. 여기서 호출되는 IContactable 구현체는
 *          body 구조 변경(SetEnabled/Destroy/Create)을 하면 IsLocked assert 로 abort 한다.
 *          마킹/큐 등록만 하고 실제 변경은 Step 밖 deferred sweep 에서 (doc/Box2DAPI.md sec 8).
 */
#ifndef __MYAPP_PHYSICS_CONTACT_LISTENER_H__
#define __MYAPP_PHYSICS_CONTACT_LISTENER_H__

#include <box2d/box2d.h>

namespace TopdownShooter::Physics
{
    /// @brief b2World::SetContactListener 대상 - 접촉을 IContactable 콜백으로 중계.
    /// @details BeginContact/EndContact 를 받아 IsSensor 분기로
    ///          양쪽 Actor 의 IContactable Component 에 콜백을 전달한다.
    class PhysicsContactListener : public b2ContactListener
    {
    public:
        /// @brief 접촉 시작 - 하나라도 sensor 면 양쪽에 OnTriggerEnter, 둘 다 solid 면 OnCollisionEnter.
        /// @param contact Box2D 가 전달하는 접촉 정보 (양쪽 fixture 보유). nullptr 가드 있음.
        void BeginContact(b2Contact* contact) override;
        /// @brief 접촉 종료 - sensor 분기에 따라 OnTriggerExit / OnCollisionExit 전달.
        /// @param contact Box2D 접촉 정보. nullptr 가드 있음.
        void EndContact  (b2Contact* contact) override;
    };
}

#endif // __MYAPP_PHYSICS_CONTACT_LISTENER_H__
