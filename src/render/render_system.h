#ifndef __SJH_RENDER_SYSTEM_H__
#define __SJH_RENDER_SYSTEM_H__

#include "render/render_queue.h"
#include <vmath.h>

namespace SJH::Scene { class Actor; }
namespace SJH
{
    /// @brief Actor 트리 traverse → MeshRenderer 수집 → DrawCommand → Queue Flush.
    /// @details per-frame 호출:
    ///   1. Scene::Root() 부터 DFS — MeshRenderer 컴포넌트 수집
    ///   2. Actor::GetWorldMatrix() = model, depth = (view × model)[3].z
    ///   3. Queue 정렬 (Multi-stage)
    ///   4. Queue Flush → RenderContext
    ///
    ///   SP3.5 의 CameraComponent 도입 시 view/proj 인자 없는 overload 추가.
    class RenderSystem
    {
    public:
        void Render(const vmath::mat4& viewMat, const vmath::mat4& projMat);

    private:
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat);

        RenderQueue mQueue;
    };
}

#endif // __SJH_RENDER_SYSTEM_H__
