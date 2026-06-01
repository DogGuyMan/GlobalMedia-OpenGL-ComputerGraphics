#ifndef __MYAPP_PHYSICS_CONTACT_LISTENER_H__
#define __MYAPP_PHYSICS_CONTACT_LISTENER_H__

#include <box2d/box2d.h>

namespace TopdownShooter::Physics
{
    /// @brief b2World::SetContactListener 대상.
    /// @details BeginContact/EndContact 받고 IsSensor 분기 
    ///          양쪽 Actor 의 IContactable Component 에 콜백 전달.
    class PhysicsContactListener : public b2ContactListener
    {
    public:
        void BeginContact(b2Contact* contact) override;
        void EndContact  (b2Contact* contact) override;
    };
}

#endif // __MYAPP_PHYSICS_CONTACT_LISTENER_H__
