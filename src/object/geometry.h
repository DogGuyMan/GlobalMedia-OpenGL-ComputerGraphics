#ifndef __OBJECT_GEOMETRY_H__
#define __OBJECT_GEOMETRY_H__

#include "object/vertex.h"
#include <GL/glcorearb.h>
#include <cstdint>
#include <vector>

// Assimp aiMesh — 헤더 의존을 cpp 로 격리하기 위해 전방선언만.
struct aiMesh;

namespace SJH
{
    /// @brief Geometry 생성기의 출력 — Mesh::Create 가 그대로 소비하는 정점/인덱스 쌍.
    struct MeshData
    {
        std::vector<Vertex>   vertices; ///< 정점 배열 (position + normal + texCoord)
        std::vector<uint32_t> indices;  ///< 인덱스 배열 (uint32_t ≡ GLuint)
    };

    /// @brief 절차적 도형 데이터 생성기 + Assimp 메시 변환기. 출력은 object-space 정규 형상.
    /// @note 배치(offset)는 노출하지 않음 — Transform 책임. winding 반전만 back_face 로 제어.
    namespace Geometry
    {
        /// @brief 원점 중심 단위 큐브 (24정점 / 36인덱스).
        MeshData Box(bool back_face = false);
        /// @brief 원점 중심 단위 XY quad, z=0 (4정점 / 6인덱스).
        MeshData Plane(bool back_face = false);
        /// @brief 사각뿔 — 4 옆면 + 바닥 quad.
        MeshData Cone(bool back_face = false);
        /// @brief 정사면체.
        MeshData Tetrahedron(bool back_face = false);
        /// @brief 정팔면체.
        MeshData Octahedron(bool back_face = false);

        /// @brief 원판/고리 파라메트릭 서피스 (XZ 평면).
        /// @param us,ue,uRes 각도 범위[rad]와 분할 수. @param vs,ve,vRes 반지름 비율(0~1)과 분할.
        MeshData Disk(double us, double ue, int uRes,
                      double vs, double ve, int vRes,
                      float radius = 1.0f, bool back_face = false);
        /// @brief 원기둥 옆면.
        /// @param us,ue,uRes 원주 각도와 분할. @param vs,ve,vRes 높이 비율과 분할.
        MeshData Cylinder(double us, double ue, int uRes,
                          double vs, double ve, int vRes,
                          float radius = 1.0f, float height = 1.0f,
                          bool back_face = false);
        /// @brief 반구 (북반구, +Y).
        /// @param us,ue,uRes 경도와 분할. @param vs,ve,vRes 위도 비율(0~1->PI/2)과 분할.
        MeshData HemiSphere(double us, double ue, int uRes,
                            double vs, double ve, int vRes,
                            float radius = 1.0f, bool back_face = false);

        /// @brief Assimp aiMesh -> MeshData 변환. position/normal/texCoord 채널을 그대로 복사하고
        ///        삼각형 face 인덱스를 펼침. (Triangulate 전처리 가정 — 모든 face 는 3 인덱스)
        MeshData FromAssimp(const aiMesh *mesh);

        /// @brief NDC clip-space 화면 가득 quad — SP4 post-processing 용.
        /// @details position 은 (-1,-1)~(1,1) NDC 좌표, UV 는 (0,0)~(1,1).
        ///          postprocess.vs 가 model/view/proj 곱셈 없이 gl_Position = vec4(aPos,1) 직접 사용.
        ///          normal 은 +Z (사용 안 함, Vertex 구조체 만족용).
        MeshData ScreenQuad();
    } // namespace Geometry
} // namespace SJH

#endif // __OBJECT_GEOMETRY_H__
