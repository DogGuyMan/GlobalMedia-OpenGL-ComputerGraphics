/**
 * @file geometry.h
 * @brief 절차적 도형 데이터 생성기(Box/Plane/Cone/Sphere 등) + Assimp 메시 변환기.
 *
 * @details
 *  ### 책임
 *  - `Geometry::Box / Plane / Cone / Tetrahedron / Octahedron` - object-space 정규 형상 데이터 생성.
 *  - `Geometry::Disk / Cylinder / HemiSphere / Sphere` - 파라메트릭 서피스 (각도/분할 인자 제어).
 *  - `Geometry::FromAssimp` - Assimp `aiMesh` -> `MeshData` 변환 (position/normal/texCoord 채널 복사).
 *  - `Geometry::ScreenQuad` - NDC clip-space 화면 가득 quad (SP4 post-processing 용).
 *  - 출력 `MeshData` 는 `Mesh::Create` 가 직접 소비하는 정점/인덱스 쌍.
 *
 *  ### 비-책임
 *  - [X] GPU 업로드 / VAO 생성 - @c Mesh::Create 가 담당.
 *  - [X] 배치(offset) 변환 - @c Transform 책임. @c back_face 파라미터로 winding 반전만 지원.
 *  - [X] 텍스처 로드 / 머티리얼 바인딩 - 호출자 책임.
 *
 * @note 내부 빌더는 13-float interleaved(pos4+color4+normal3+uv2) 형식으로 조립 후
 *       `FromInterleaved` 가 @c SJH::Vertex(pos3+normal3+uv2) 로 변환한다.
 *       color 4 float 과 pos.w 는 폐기.
 */

#ifndef __OBJECT_GEOMETRY_H__
#define __OBJECT_GEOMETRY_H__

#include "object/vertex.h"
#include <GL/glcorearb.h>
#include <cstdint>
#include <vector>

// Assimp aiMesh - 헤더 의존을 cpp 로 격리하기 위해 전방선언만.
struct aiMesh;

namespace SJH
{
    /**
     * @brief Geometry 생성기의 출력 - @c Mesh::Create 가 그대로 소비하는 정점/인덱스 쌍.
     * @details CPU 측 정점/인덱스 버퍼 컨테이너. GPU 업로드 전까지 임시 보유 후 이동(move) 소비된다.
     */
    struct MeshData
    {
        std::vector<Vertex>   vertices; ///< 정점 배열 (position + normal + texCoord)
        std::vector<uint32_t> indices;  ///< 인덱스 배열 (@c uint32_t == @c GLuint)
    };

    /**
     * @brief 절차적 도형 데이터 생성기 + Assimp 메시 변환기.
     * @details
     *  출력은 모두 object-space 정규 형상 (@c MeshData). 배치(offset)는 노출하지 않으며
     *  winding 반전은 @c back_face 파라미터로 제어한다 - 위치/회전/스케일은 @c Transform 책임.
     *
     *  파라메트릭 함수(@c Disk / @c Cylinder / @c HemiSphere / @c Sphere)의 공통 인자 규약:
     *  - @c us / @c ue / @c uRes - 경도(또는 원주) 각도 범위[rad]와 분할 수.
     *  - @c vs / @c ve / @c vRes - 위도(또는 반지름/높이) 비율과 분할 수.
     */
    namespace Geometry
    {
        /// @brief 원점 중심 단위 큐브 (24 정점 / 36 인덱스). 각 면에 독립 법선.
        /// @param back_face @c true 이면 winding 반전 - 안쪽 면 렌더용.
        MeshData Box(bool back_face = false);

        /// @brief 원점 중심 단위 XY quad, z=0 (4 정점 / 6 인덱스).
        /// @param back_face @c true 이면 winding 반전.
        MeshData Plane(bool back_face = false);

        /// @brief 사각뿔 - 4 삼각형 옆면 + 바닥 quad.
        /// @param back_face @c true 이면 winding 반전.
        MeshData Cone(bool back_face = false);

        /// @brief 정사면체 - 4 삼각형 면.
        /// @param back_face @c true 이면 winding 반전.
        MeshData Tetrahedron(bool back_face = false);

        /// @brief 정팔면체 - 위/아래 사각뿔 합성.
        /// @param back_face @c true 이면 winding 반전.
        MeshData Octahedron(bool back_face = false);

        /// @brief 원판/고리 파라메트릭 서피스 (XZ 평면, 법선 +/-Y).
        /// @param us     원주 시작 각도 [rad].
        /// @param ue     원주 끝 각도 [rad].
        /// @param uRes   원주 방향 분할 수.
        /// @param vs     반지름 시작 비율 (0 = 중심, @c >0 = ring 내경).
        /// @param ve     반지름 끝 비율 (0~1, @c radius 곱셈 기준).
        /// @param vRes   반지름 방향 분할 수.
        /// @param radius 최대 반지름 (world unit).
        /// @param back_face @c true 이면 winding 반전 (법선 -Y).
        MeshData Disk(double us, double ue, int uRes,
                      double vs, double ve, int vRes,
                      float radius = 1.0f, bool back_face = false);

        /// @brief 원기둥 옆면 (XZ 저면, +Y 축, 법선 = XZ 라디알 방향).
        /// @param us     원주 시작 각도 [rad].
        /// @param ue     원주 끝 각도 [rad].
        /// @param uRes   원주 방향 분할 수.
        /// @param vs     높이 시작 비율 (0~1, @c height 곱셈 기준).
        /// @param ve     높이 끝 비율.
        /// @param vRes   높이 방향 분할 수.
        /// @param radius 반지름.
        /// @param height 전체 높이.
        /// @param back_face @c true 이면 법선 안쪽(내면) 반전.
        MeshData Cylinder(double us, double ue, int uRes,
                          double vs, double ve, int vRes,
                          float radius = 1.0f, float height = 1.0f,
                          bool back_face = false);

        /// @brief 북반구 (+Y, 중심 원점). 법선 = 구면 (중심->정점 방향).
        /// @param us     경도 시작 각도 [rad].
        /// @param ue     경도 끝 각도 [rad].
        /// @param uRes   경도 방향 분할 수.
        /// @param vs     위도 비율 시작 (0 = 적도, 1 = 북극, @c *PI/2 변환).
        /// @param ve     위도 비율 끝.
        /// @param vRes   위도 방향 분할 수.
        /// @param radius 반지름.
        /// @param back_face @c true 이면 법선 내향 반전.
        MeshData HemiSphere(double us, double ue, int uRes,
                            double vs, double ve, int vRes,
                            float radius = 1.0f, bool back_face = false);

        /// @brief 완전구 (중심 원점). 법선 = 구면 (중심->정점 방향).
        /// @param us     경도 시작 각도 [rad].
        /// @param ue     경도 끝 각도 [rad].
        /// @param uRes   경도 방향 분할 수.
        /// @param vs     위도 비율 시작 (0 = 남극, 1 = 북극, @c *PI-PI/2 변환).
        /// @param ve     위도 비율 끝.
        /// @param vRes   위도 방향 분할 수.
        /// @param radius 반지름.
        /// @param back_face @c true 이면 법선 내향 반전.
        MeshData Sphere(double us, double ue, int uRes,
                        double vs, double ve, int vRes,
                        float radius = 0.5f, bool back_face = false);

        /// @brief Assimp @c aiMesh -> @c MeshData 변환.
        /// @details position / normal / texCoord(채널 0) 채널을 그대로 복사하고
        ///          삼각형 face 인덱스를 선형 배열로 펼침.
        ///          Assimp @c aiProcess_Triangulate 전처리 가정 - 모든 face 는 정확히 3 인덱스.
        /// @param mesh 변환할 @c aiMesh 포인터. @c nullptr 입력은 정의되지 않은 동작.
        MeshData FromAssimp(const aiMesh *mesh);

        /// @brief NDC clip-space 화면 가득 quad - SP4 post-processing 용.
        /// @details position 은 (-1,-1)~(1,1) NDC 좌표, UV 는 (0,0)~(1,1) (좌하단 원점).
        ///          @c postprocess.vs 가 model/view/proj 곱셈 없이 @c gl_Position = vec4(aPos,1) 직접 사용.
        ///          normal 은 +Z (Vertex 구조체 필드 충족용 - 실제 라이팅에 사용되지 않음).
        MeshData ScreenQuad();
    } // namespace Geometry
} // namespace SJH

#endif // __OBJECT_GEOMETRY_H__
