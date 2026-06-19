/**
 * @file model_spawner.cpp
 * @brief @c ModelSpawner::SpawnEntities 구현 - @c Model RenderUnit -> 자식 Actor 변환.
 *
 * @details
 *  ### 책임
 *  - @c Model::GetRenderUnits() 순회 -> 인덱스 기반 이름(@c RenderUnit_N) + @c MeshRenderer.
 *  - 생성된 자식 Actor 를 @p parent.AddChild 로 편입.
 *
 *  ### 비-책임
 *  - [X] @c Model / @c Mesh / @c Material 소유권 - 모두 @c ResourceRegistry 보유.
 */
#include "render/model_spawner.h"
#include "scene/actor.h"
#include "render/mesh_renderer.h"
#include "object/model.h"

#include <memory>
#include <string>

namespace SJH::Scene::ModelSpawner
{
    std::vector<Actor*> SpawnEntities(Actor& parent, const Model& model)
    {
        std::vector<Actor*> spawned;
        const auto& units = model.GetRenderUnits();
        spawned.reserve(units.size());

        for (size_t i = 0; i < units.size(); ++i)
        {
            const auto& ru = units[i];
            auto child = std::make_unique<Actor>("RenderUnit_" + std::to_string(i));
            child->AddComponent<MeshRenderer>(ru.mesh.get(), ru.material);
            spawned.push_back(parent.AddChild(std::move(child)));
        }
        return spawned;
    }
}
