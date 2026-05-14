#include "engine/scene_graph.h"

#include <utility>

namespace Engine::SceneGraph
{
	Transform::Transform *SceneGraph::CreateRootTransform(const char *root_name)
	{
		auto root = std::make_unique<Transform::Transform>();
		root->Name = root_name;
		hierarchies.insert(std::make_pair(root_name, std::move(root)));
		return hierarchies[root_name].get();
	}

	void SceneGraph::Clear()
	{
		hierarchies.clear();
	}
} // namespace Engine::SceneGraph
