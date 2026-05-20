#include "scene/scene.h"
#include <type_traits>

static_assert(!std::is_copy_constructible_v<SJH::Scene::Director>,
              "SJH::Scene::Director must be non-copy-constructible (singleton)");
static_assert(!std::is_move_constructible_v<SJH::Scene::Director>,
              "SJH::Scene::Director must be non-move-constructible (singleton)");

namespace SJH::Scene
{
    Director& Director::Get()
    {
        static Director instance;   // Meyer's singleton (thread-safe in C++11+)
        return instance;
    }
}
