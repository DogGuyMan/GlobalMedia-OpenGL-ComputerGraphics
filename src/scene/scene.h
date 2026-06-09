/**
 * @file scene.h
 * @brief @c Director 싱글톤 - root @c Actor 트리 + @c SceneContext Aggregate Root 보유.
 *
 * @details
 *  ### 책임
 *  - Meyer's 싱글톤 @c Director::Get() 으로 전역 씬 진입점 제공.
 *  - @c Root() - 전체 씬 트리의 최상위 Actor (@c "WorldRoot").
 *  - @c Enter() / @c Exit() / @c Update(dt) - 씬 전체 lifecycle 일괄 위임.
 *  - @c GetContext() - Camera/Light 비소유 컬렉션 @c SceneContext 접근.
 *
 *  ### 비-책임
 *  - [X] 렌더링 - @c SceneRenderer 가 담당.
 *  - [X] 자원 관리 - @c ResourceRegistry 가 담당.
 *
 * @note SP-SceneContext+ProgramRegistry (2026-05-26) - @c mActiveCamera 단일 슬롯 폐기.
 *       Camera/Light 컬렉션은 @c SceneContext 가 보유 (Cocos2D @c Scene::_cameras 정통).
 */

#ifndef __SJH_SCENE_H__
#define __SJH_SCENE_H__

#include "scene/actor.h"
#include "scene/scene_context.h"

namespace SJH::Scene
{
    /**
     * @brief Cocos @c cc::Director 정통 - root @c Actor + @c SceneContext Aggregate Root 보유 싱글톤.
     * @details
     *  @c Scene::Director::Get().Root() 로 씬 트리 편집,
     *  @c Scene::Director::Get().GetContext() 로 Camera/Light 컬렉션 접근.
     *
     *  SP-SceneContext+ProgramRegistry (2026-05-26) - @c mActiveCamera 슬롯 폐기,
     *  @c SceneContext 가 Camera/Light 컬렉션 보유 (Cocos2D @c Scene::_cameras 정통).
     */
    class Director
    {
    public:
        /// @brief Meyer's 싱글톤 - thread-safe (C++11 static local). 첫 호출에 lazy 초기화.
        static Director& Get();

        /// @brief 씬 트리 최상위 Actor ("WorldRoot") 참조.
        Actor&       Root()       { return mRoot; }
        /// @brief 씬 트리 최상위 Actor (읽기 전용).
        const Actor& Root() const { return mRoot; }

        /// @brief 씬 전체 OnEnter 위임 - @c mRoot.OnEnter() 전파.
        void Enter()             { mRoot.OnEnter(); }
        /// @brief 씬 전체 OnExit 위임 - @c mRoot.OnExit() 전파.
        void Exit()              { mRoot.OnExit(); }
        /// @brief 씬 전체 Update 위임 - @c mRoot.Update(dt) 재귀 전파.
        /// @param dt 프레임 델타 타임 (초).
        void Update(float dt)    { mRoot.Update(dt); }

        /// @brief Camera / Light 비소유 컬렉션 @c SceneContext 참조.
        SceneContext&       GetContext()       { return mContext; }
        /// @brief @c SceneContext 읽기 전용.
        const SceneContext& GetContext() const { return mContext; }

        // 싱글톤 - 복사/이동 금지.
        Director(const Director&)            = delete;
        Director& operator=(const Director&) = delete;
        Director(Director&&)                 = delete;
        Director& operator=(Director&&)      = delete;

    private:
        Director() : mRoot("WorldRoot") {}
        ~Director() = default;

        Actor        mRoot;    ///< 씬 트리 루트 노드.
        SceneContext mContext; ///< Camera/Light 비소유 컬렉션.
    };
}

#endif // __SJH_SCENE_H__
