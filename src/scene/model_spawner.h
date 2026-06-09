/**
 * @file model_spawner.h
 * @brief Assimp @c Model 의 @c RenderUnit 을 Actor 트리로 변환하는 유틸리티 네임스페이스.
 *
 * @details
 *  ### 책임
 *  - @c Model::GetRenderUnits() 의 각 @c RenderUnit 을 독립 자식 @c Actor 로 생성.
 *  - 각 자식 Actor 에 @c MeshRenderer Component 를 부착하고 @p parent 에 추가.
 *
 *  ### 비-책임
 *  - [X] @c Model 로딩 - @c ResourceRegistry::CreateModel 이 담당.
 *  - [X] Transform 계층 - @c Model 이 계층 정보를 가지지 않으므로 flat 자식 구조만 생성.
 *
 * @note 결정 Q5-1 - 1 RenderUnit = 1 Actor. 부모-자식 Transform 계층을 자연스럽게 흡수.
 */

#ifndef __SJH_SCENE_MODEL_SPAWNER_H__
#define __SJH_SCENE_MODEL_SPAWNER_H__

#include <vector>

namespace SJH { class Model; }
namespace SJH::Scene
{
    class Actor;

    /// @brief Assimp Model 의 N RenderUnit -> N 자식 Actor (각자 MeshRenderer 보유).
    /// @details Q5-1 결정 - 1 RenderUnit = 1 Actor. 부모-자식 Transform 계층 자연 흡수.
    namespace ModelSpawner
    {
        /// @brief @c parent 의 자식들로 Model 의 모든 RenderUnit 을 Actor 화.
        /// @return 생성된 자식 Actor 목록 - 비소유 관찰자. 수명은 @c parent 가 보장.
        ///         @c parent 가 소멸하거나 자식을 제거하면 즉시 dangling - 호출자 보관 금지.
        /// @note  부분 결과 가능 - std::vector::push_back 또는 unique_ptr emplace 가 throw 시
        ///        지금까지 생성된 일부 Actor 만 부착된 채 예외 전파. parent 의 자식 일관성은 유지.
        std::vector<Actor*> SpawnEntities(Actor& parent, const Model& model);
    }
}

#endif // __SJH_SCENE_MODEL_SPAWNER_H__
