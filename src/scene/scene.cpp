/**
 * @file scene.cpp
 * @brief @c Director 싱글톤 구현 - Meyer's singleton + 컴파일 타임 복사/이동 금지 검증.
 *
 * @details
 *  ### 책임
 *  - @c Director::Get() - Meyer's 싱글톤 인스턴스 반환 (thread-safe C++11 static local).
 *
 *  ### 비-책임
 *  - [X] 씬 트리 / @c SceneContext 구현 - 각 담당 파일 참조.
 *
 * @note 파일 최상단 @c static_assert 2종으로 @c Director 비복사/비이동을 컴파일 타임 검증.
 */
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
