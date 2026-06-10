/**
 * @file PhysicsSystem.h
 * @brief b2World 소유자 + Step / SyncToTransform / ContactListener 설치 책임.
 *
 * @details
 *  ### 책임
 *  - @c b2World (gravity=0, top-down) 를 유일하게 소유 (@c unique_ptr).
 *  - @c Init(): b2World 생성 + @c PhysicsContactListener 설치.
 *  - @c Step(dt): @c b2World::Step(dt, 8, 3) 구동. 이 단계에서 ContactListener 콜백 발생.
 *  - @c SyncToTransform(root): 물리 body 위치 (x, y) -> 렌더 Transform (x, heightOffset, -y).
 *
 *  ### 비-책임
 *  - [X] b2Body 생성/파괴 -- @c CircleBody / @c BoxBody ctor / @c Physics::OnExit 담당.
 *  - [X] 충돌 이벤트 게임 로직 -- @c PhysicsContactListener + @c IContactable 담당.
 *
 *  ### per-frame 호출 순서 (main.cpp)
 *  1. @c Director.Update -- Movement/PhysicsMovement 가 body velocity 갱신.
 *  2. @c PhysicsSystem::Step(dt) -- b2World::Step (ContactListener 콜백 발생 -> Step 잠금).
 *  3. @c WaveController::SweepDespawned() -- Step 종료 후 deferred body 변경 sweep.
 *  4. @c PhysicsSystem::SyncToTransform(root) -- body 좌표 -> Actor.Transform 반영.
 *  5. @c SceneRenderer::Render.
 *
 * @note b2World::Step 잠금 중(ContactListener 콜백 포함) body 구조 변경
 *       (SetEnabled/DestroyBody/CreateBody) 은 abort 를 유발한다.
 *       콜백에서는 enqueue 만 수행하고, Step 종료 후 sweep 에서 처리.
 *       (doc/Box2DAPI.md sec.8 참조)
 */
#ifndef __MYAPP_PHYSICS_SYSTEM_H__
#define __MYAPP_PHYSICS_SYSTEM_H__

#include <box2d/box2d.h>
#include <memory>

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Physics
{
    class PhysicsContactListener;

    /**
     * @brief b2World 소유 + Step / SyncToTransform / ContactListener 설치 책임 시스템 클래스.
     * @details
     *  복사 금지 (unique_ptr 소유, b2World 는 비복사 타입).
     *  @c Init() 으로 초기화, @c Shutdown() (또는 소멸자) 으로 b2World 해제.
     *
     *  per-frame 호출 순서:
     *  1. @c Director.Update -- @c PhysicsMovement 가 body velocity 갱신.
     *  2. @c Step(dt) -- @c b2World::Step (ContactListener 콜백 발생, Step 잠금 시작->해제).
     *  3. deferred sweep -- Step 종료 후 @c WaveController::SweepDespawned 등 body 변경.
     *  4. @c SyncToTransform(root) -- body 좌표 -> Actor.Transform 반영.
     *  5. @c SceneRenderer::Render.
     */
    class PhysicsSystem
    {
    public:
        PhysicsSystem();
        ~PhysicsSystem();

        PhysicsSystem(const PhysicsSystem&)            = delete;
        PhysicsSystem& operator=(const PhysicsSystem&) = delete;

        /// @brief b2World 생성 (gravity=0, top-down 슈터) + @c PhysicsContactListener 설치.
        /// @note 중복 호출 시 warn 후 무시 (멱등 guard).
        void Init();
        /// @brief b2World 해제. 소멸자에서도 자동 호출.
        void Shutdown();

        /// @brief @c b2World::Step(dt, velocityIters=8, positionIters=3) 구동.
        /// @details 이 호출 동안 ContactListener 콜백이 발생하며 b2World 는 잠금 상태가 된다.
        ///          잠금 중 body 구조 변경(SetEnabled/Destroy/CreateBody) 은 abort 유발 -- Step 후 sweep 에서 처리.
        /// @param dt 프레임 delta time (초 단위).
        void Step(float dt);

        /// @brief @p root 부터 재귀 DFS -- @c Components::Physics 부착 Actor 의 Transform 을 body 위치로 갱신.
        /// @details 물리 XY -> 렌더 XZ 변환: tr = (p.x, heightOffset, -p.y).
        /// @param root DFS 시작 Actor (씬 루트 또는 특정 서브트리).
        void SyncToTransform(SJH::Scene::Actor& root);

        /// @brief 내부 @c b2World 참조 반환. @c Raycast / @c CreateBody 직접 호출용.
        b2World& World() { return *mWorld; }

    private:
        std::unique_ptr<b2World>                mWorld;           ///< top-down 물리 세계 (gravity=0). 유일 소유자.
        std::unique_ptr<PhysicsContactListener> mContactListener; ///< 충돌 이벤트 -> IContactable 디스패치.
    };
}

#endif // __MYAPP_PHYSICS_SYSTEM_H__
