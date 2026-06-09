/**
 * @file vertex.h
 * @brief 단일 정점 레이아웃 정의 - position + normal + texCoord (interleaved VBO 단위).
 *
 * @details
 *  ### 책임
 *  - @c SJH::Vertex - Mesh VBO 의 한 원소. @c Geometry 빌더 출력과 @c Mesh::Init attrib 설정의 공통 단위.
 *
 *  ### 비-책임
 *  - [X] GPU 업로드 / attrib 설정 - @c Mesh::Init 이 @c VertexLayout::TrySetAttrib 로 담당.
 *  - [X] 탄젠트/바이탄젠트 / 스키닝 가중치 - 현재 미지원. 향후 확장 시 별도 Vertex 변형 도입.
 *
 * @note GL 로더(gl3w) 비의존 - @c vmath 만 의존하므로 어느 번역 단위에서도 안전하게 포함 가능.
 *       `offsetof(Vertex, position/normal/texCoord)` 로 VBO stride/offset 를 안전하게 계산.
 */

#ifndef __OBJECT_VERTEX_H__
#define __OBJECT_VERTEX_H__

#include <vmath.h>

namespace SJH
{
    /**
     * @brief 단일 정점 - position + normal + texCoord 인터리브 레이아웃.
     * @details
     *  @c Mesh::Init 의 @c TrySetAttrib 가 이 구조체의 @c offsetof 를 그대로 사용:
     *  - attrib 0 : @c position (vec3)
     *  - attrib 1 : @c normal   (vec3)
     *  - attrib 2 : @c texCoord (vec2)
     */
    struct Vertex
    {
        vmath::vec3 position; ///< 정점 위치 (object space).
        vmath::vec3 normal;   ///< 법선 벡터 (object space, 단위 벡터 가정).
        vmath::vec2 texCoord; ///< UV 좌표 (0~1 범위 권장).
    };
}

#endif // __OBJECT_VERTEX_H__
