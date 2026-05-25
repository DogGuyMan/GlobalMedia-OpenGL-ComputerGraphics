#ifndef __MYAPP_PHYSICS_SYSTEM_H__
#define __MYAPP_PHYSICS_SYSTEM_H__

#include <box2d/box2d.h>
#include <memory>

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Physics
{
    class PhysicsContactListener;

    /// @brief b2World owner — Step + SyncToTransform + ContactListener 설치 책임.
    /// @details main.cpp 매 frame 호출 흐름:
    ///   1. Director.Update → Movement/PhysicsMovement 가 body velocity 갱신
    ///   2. physics.Step(dt) — b2World::Step. 이 단계에서 ContactListener 콜백 발생
    ///   3. physics.SyncToTransform(root) — body 좌표 → Actor.Transform 반영
    ///   4. SceneRenderer.Render
    class PhysicsSystem
    {
    public:
        PhysicsSystem();
        ~PhysicsSystem();

        PhysicsSystem(const PhysicsSystem&)            = delete;
        PhysicsSystem& operator=(const PhysicsSystem&) = delete;

        /// @brief b2World 생성 (gravity=0, top-down) + ContactListener 설치.
        void Init();
        void Shutdown();

        /// @brief b2World::Step(dt, velocityIters=8, positionIters=3).
        void Step(float dt);

        /// @brief root Actor 부터 재귀 순회 — PhysicsBodyComponent 부착 Actor 의 Transform 갱신.
        ///        물리 (x, y) → 렌더 (x, heightOffset, -y).
        void SyncToTransform(SJH::Scene::Actor& root);

        b2World& World() { return *mWorld; }

    private:
        std::unique_ptr<b2World>                mWorld;
        std::unique_ptr<PhysicsContactListener> mContactListener;
    };
}

#endif // __MYAPP_PHYSICS_SYSTEM_H__
