/**
 * @file scene_node_id.h
 * @brief Context 씬의 노드 식별 enum — SceneGraph 의 배열 인덱스.
 */
#ifndef __SJH_SCENE_NODE_ID_H__
#define __SJH_SCENE_NODE_ID_H__

#include <cstddef>

namespace SJH
{
    /// @brief Context 씬 노드 식별자. 정수값이 SceneGraph 노드 배열 인덱스.
    enum class SceneNodeId : std::size_t
    {
        Root = 0,
        Plane,
        Box1,
        Box2,
        Outline,
        Windows,
        Window0,
        Window1,
        Window2,
        Camera,
        DirLight,
        PointLight0,
        PointLight1,
        SpotLight,
        Count
    };
}
#endif // __SJH_SCENE_NODE_ID_H__
