#ifndef __OBJECT_VERTEX_H__
#define __OBJECT_VERTEX_H__

#include <vmath.h>

namespace SJH
{
    /// @brief 단일 정점 — 위치 + 법선 + UV 좌표.
    /// @note GL 로더(glad/gl3w) 비의존 — vmath 만 의존하므로 어느 TU 에서도 안전.
    struct Vertex
    {
        vmath::vec3 position; ///< 정점 위치 (object space)
        vmath::vec3 normal;   ///< 법선 벡터 (object space, 정규화 가정)
        vmath::vec2 texCoord; ///< UV 좌표 (0~1 범위 권장)
    };
}

#endif // __OBJECT_VERTEX_H__
