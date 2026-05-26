#ifndef __SJH_SCENE_H__
#define __SJH_SCENE_H__

#include "scene/actor.h"
#include "scene/scene_context.h"

namespace SJH::Scene
{
    /// @brief Cocos cc::Director 정통 — root Actor + SceneContext Aggregate Root 보유 싱글톤.
    /// @details `Scene::Director::Get().Root()` / `Scene::Director::Get().GetContext()`.
    ///          SP-SceneContext+ProgramRegistry (2026-05-26) — mActiveCamera 슬롯 폐기,
    ///          SceneContext 가 Camera/Light 컬렉션 보유 (Cocos2D `Scene::_cameras` 정통).
    class Director
    {
    public:
        static Director& Get();

        Actor&       Root()       { return mRoot; }
        const Actor& Root() const { return mRoot; }

        void Enter()             { mRoot.OnEnter(); }
        void Exit()              { mRoot.OnExit(); }
        void Update(float dt)    { mRoot.Update(dt); }

        SceneContext&       GetContext()       { return mContext; }
        const SceneContext& GetContext() const { return mContext; }

        Director(const Director&)            = delete;
        Director& operator=(const Director&) = delete;
        Director(Director&&)                 = delete;
        Director& operator=(Director&&)      = delete;

    private:
        Director() : mRoot("WorldRoot") {}
        ~Director() = default;

        Actor        mRoot;
        SceneContext mContext;
    };
}

#endif // __SJH_SCENE_H__
