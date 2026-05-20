#ifndef __SJH_SCENE_H__
#define __SJH_SCENE_H__

#include "scene/actor.h"

namespace SJH::Scene
{
    /// @brief Cocos cc::Director 정통 — root Actor 보유 싱글톤. SP2 RenderContext::Get() 패턴과 일관.
    /// @details `namespace SJH::Scene` 의 nested 싱글톤 — 사용: `SJH::Scene::Director::Get().Root()`.
    class Director
    {
    public:
        static Director& Get();

        Actor&       Root()       { return mRoot; }
        const Actor& Root() const { return mRoot; }

        void Enter()             { mRoot.OnEnter(); }
        void Exit()              { mRoot.OnExit(); }
        void Update(float dt)    { mRoot.Update(dt); }

        Director(const Director&)            = delete;
        Director& operator=(const Director&) = delete;
        Director(Director&&)                 = delete;
        Director& operator=(Director&&)      = delete;

    private:
        Director() : mRoot("WorldRoot") {}
        ~Director() = default;

        Actor mRoot;
    };
}

#endif // __SJH_SCENE_H__
