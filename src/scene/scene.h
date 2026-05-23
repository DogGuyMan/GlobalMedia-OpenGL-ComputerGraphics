#ifndef __SJH_SCENE_H__
#define __SJH_SCENE_H__

#include "scene/actor.h"

namespace SJH::Scene
{
    class Camera;   // SetActiveCamera 인자 forward decl — scene/camera.h 전체 include 회피.

    /// @brief Cocos cc::Director 정통 — root Actor 보유 싱글톤. SP2 DeviceContext::Get() 패턴과 일관.
    /// @details `namespace SJH::Scene` 의 nested 싱글톤 — 사용: `SJH::Scene::Director::Get().Root()`.
    ///          SP3.5 — 활성 Camera 슬롯 추가 (Unity Camera.main 정통). SceneRenderer::Render()
    ///          가 인자 없는 overload 호출 시 이 슬롯을 조회.
    class Director
    {
    public:
        static Director& Get();

        Actor&       Root()       { return mRoot; }
        const Actor& Root() const { return mRoot; }

        void Enter()             { mRoot.OnEnter(); }
        void Exit()              { mRoot.OnExit(); }
        void Update(float dt)    { mRoot.Update(dt); }

        /// @brief 활성 Camera 지정 — 비소유 관찰자 포인터.
        /// @details Camera 의 owner 는 Actor (Component lifetime). Director 는 단순 슬롯.
        ///          nullptr 설정으로 비활성화 가능.
        void    SetActiveCamera(Camera* cam) { mActiveCamera = cam; }

        /// @brief 현재 활성 Camera 조회. 미지정 시 nullptr.
        Camera* GetActiveCamera() const      { return mActiveCamera; }

        Director(const Director&)            = delete;
        Director& operator=(const Director&) = delete;
        Director(Director&&)                 = delete;
        Director& operator=(Director&&)      = delete;

    private:
        Director() : mRoot("WorldRoot") {}
        ~Director() = default;

        Actor   mRoot;
        Camera* mActiveCamera = nullptr;   // 비소유 — owner 는 Actor 의 Component map.
    };
}

#endif // __SJH_SCENE_H__
