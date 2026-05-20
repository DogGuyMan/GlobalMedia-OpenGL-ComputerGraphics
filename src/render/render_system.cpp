#include "render/render_system.h"
#include "render/render_context.h"
#include "scene/scene.h"
#include "scene/actor.h"
#include "scene/components.h"
#include "material/material.h"

namespace SJH
{
    void RenderSystem::Render(const vmath::mat4& viewMat, const vmath::mat4& projMat)
    {
        auto& rc = RenderContext::Get();
        mQueue.Clear();
        CollectFromActor(Scene::Director::Get().Root(), viewMat);
        mQueue.SortMultiStage();
        mQueue.Flush(rc, viewMat, projMat);
    }

    void RenderSystem::CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat)
    {
        if (!actor.IsActive()) return;

        if (auto* mr = actor.GetComponent<Scene::MeshRenderer>())
        {
            if (mr->IsEnabled() && mr->Visible && mr->Mesh && mr->Material)
            {
                const vmath::mat4 model    = actor.GetWorldMatrix();
                // view-space origin z: (viewMat * model) 의 4번째 열(translation) z 성분.
                // vmath 는 mat*vec 오버로드 미제공 → mat4 직접 인덱싱으로 depth 추출.
                const float       depthZ   = (viewMat * model)[3][2];
                mQueue.Submit({
                    mr->Material->GetProgram(),
                    mr->Mesh,
                    mr->Material,
                    model,
                    mr->QueueLayer,
                    &actor,
                    depthZ
                });
            }
        }

        for (const auto& child : actor.GetChildren())
            CollectFromActor(*child, viewMat);
    }
}
