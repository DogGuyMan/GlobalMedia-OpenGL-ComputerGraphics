#include "scene/model_spawner.h"
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
