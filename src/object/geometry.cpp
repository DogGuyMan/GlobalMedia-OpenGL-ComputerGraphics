#include "object/geometry.h"
#include "common/constants.h"
#include <vmath.h>
#include <assimp/mesh.h>
#include <cmath>

namespace SJH
{
    namespace
    {
        // 익명 네임스페이스 — 절차적 도형 빌더는 외부 노출 X (geometry.cpp 내부 한정).
        // 13-float interleaved(pos4 + color4 + normal3 + uv2) 출력으로 통일, FromInterleaved 가
        // SJH::Vertex(pos3 + normal3 + uv2) 로 변환 (color 폐기).
        using namespace Const::GEOMETRY;

        // === Utility functions ===
        vmath::vec3 ComputeFaceNormal(const vmath::vec4 &p0,
                                      const vmath::vec4 &p1,
                                      const vmath::vec4 &p2)
        {
            vmath::vec3 e1 = vmath::vec3(p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]);
            vmath::vec3 e2 = vmath::vec3(p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]);
            return vmath::normalize(vmath::cross(e1, e2));
        }

        void PushVertex(std::vector<GLfloat> &vertices,
                        const vmath::vec4 pos,
                        const vmath::vec4 color,
                        const vmath::vec3 normal,
                        const vmath::vec2 uv,
                        const vmath::vec3 &offset)
        {
            vertices.push_back(pos[0] + offset[0]);
            vertices.push_back(pos[1] + offset[1]);
            vertices.push_back(pos[2] + offset[2]);
            vertices.push_back(pos[3]);
            vertices.push_back(color[0]);
            vertices.push_back(color[1]);
            vertices.push_back(color[2]);
            vertices.push_back(color[3]);
            vertices.push_back(normal[0]);
            vertices.push_back(normal[1]);
            vertices.push_back(normal[2]);
            vertices.push_back(uv[0]);
            vertices.push_back(uv[1]);
        }

        // =================================================
        // === Indexed Builders (VBO + EBO 진짜 인덱싱) ===
        // =================================================

        void BuildTriangleIndexed(
            std::vector<GLfloat> &vertices,
            std::vector<GLuint> &indices,
            const std::vector<vmath::vec4> &positions,
            const std::vector<vmath::vec4> &colors,
            const std::vector<vmath::vec2> &uvs,
            const std::vector<GLuint> &position_idxs,
            const vmath::vec3 &offset,
            const std::vector<GLuint> &face_idxs)
        {
            const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

            const vmath::vec4 &fp0 = positions[position_idxs[face_idxs[0]]];
            const vmath::vec4 &fp1 = positions[position_idxs[face_idxs[1]]];
            const vmath::vec4 &fp2 = positions[position_idxs[face_idxs[2]]];
            const vmath::vec3 faceNormal = ComputeFaceNormal(fp0, fp1, fp2);

            // 삼각형은 3 정점 = 3 unique. 인덱스도 0,1,2 그대로.
            for (size_t i = 0; i < 3; i++)
            {
                GLuint k = face_idxs[i];
                PushVertex(vertices, positions[position_idxs[k]], colors[k], faceNormal, uvs[k], offset);
            }
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
        }

        void BuildQuadIndexed(
            std::vector<GLfloat> &vertices,
            std::vector<GLuint> &indices,
            const std::vector<vmath::vec4> &positions,
            const std::vector<vmath::vec4> &colors,
            const std::vector<vmath::vec2> &uvs,
            const std::vector<GLuint> &position_idxs,
            const vmath::vec3 &offset,
            const std::vector<GLuint> &face_idxs)
        {
            const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

            const vmath::vec4 &fp0 = positions[position_idxs[face_idxs[0]]];
            const vmath::vec4 &fp1 = positions[position_idxs[face_idxs[1]]];
            const vmath::vec4 &fp2 = positions[position_idxs[face_idxs[2]]];
            const vmath::vec3 faceNormal = ComputeFaceNormal(fp0, fp1, fp2);

            // 펼친 6 정점을 순회하며 (position_idxs[k], QUAD_MESH_UVS_FAN[k]) 쌍을 키로 dedupe.
            // FRONT/BACK 모두 자동 처리 — winding 반전은 face_idxs reorder 가 인덱스 순서를 바꿔서 해결.
            // 전제: face_idxs / position_idxs / QUAD_MESH_UVS_FAN 조합이 평면 quad 의 표준 패턴
            //       (6 → 4 unique). 비표준 입력으로 5+ unique 가 나오면 seenPos[4]/seenUv[4] 가 overrun.
            int localIdx[6];
            GLuint seenPos[4];
            GLuint seenUv[4];
            int uniqueCount = 0;
            for (size_t i = 0; i < 6; i++)
            {
                GLuint k = face_idxs[i];
                GLuint pkey = position_idxs[k];
                GLuint ukey = QUAD_MESH_UVS_FAN[k];
                int found = -1;
                for (int j = 0; j < uniqueCount; j++)
                {
                    if (seenPos[j] == pkey && seenUv[j] == ukey)
                    {
                        found = j;
                        break;
                    }
                }
                if (found < 0)
                {
                    found = uniqueCount;
                    seenPos[uniqueCount] = pkey;
                    seenUv[uniqueCount] = ukey;
                    uniqueCount++;
                    // 첫 등장의 colors[k] 만 저장 — 같은 (pkey, ukey) 쌍이 다른 color 라면 무시.
                    // 현 사용처는 모두 COLOR_ALL_WHITE_* 이라 영향 없음.
                    PushVertex(vertices, positions[pkey], colors[k], faceNormal, uvs[ukey], offset);
                }
                localIdx[i] = found;
            }
            // uniqueCount 는 항상 4 (평면 quad).
            for (size_t i = 0; i < 6; i++)
                indices.push_back(base + static_cast<GLuint>(localIdx[i]));
        }

        void BuildCubeIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
                              const vmath::vec3 &offset, bool back_face)
        {
            const auto &face_idxs = back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES;
            for (size_t f = 0; f < 6; f++)
                BuildQuadIndexed(vertices, indices,
                                 CUBE_BASE_POSITIONS, COLOR_ALL_WHITE_6,
                                 QUAD_BASE_MESH_UVS, CUBE_FACE_INDICES[f],
                                 offset, face_idxs);
        }

        void BuildConeIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
                              const vmath::vec3 &offset, bool back_face)
        {
            const auto &tri_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
            const auto &quad_idxs = back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES;
            for (size_t f = 0; f < 4; f++)
                BuildTriangleIndexed(vertices, indices,
                                     CONE_SIDE_BASE_POSITION, COLOR_ALL_WHITE_4,
                                     TRIANGLE_BASE_MESH_UVS, CONE_SIDE_FACE_INDICES[f],
                                     offset, tri_idxs);
            BuildQuadIndexed(vertices, indices,
                             CONE_BOTTOM_BASE_POSITION, COLOR_ALL_WHITE_6,
                             QUAD_BASE_MESH_UVS, QUAD_MESH_UVS_FAN,
                             offset, quad_idxs);
        }

        void BuildTetrahedronIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
                                     const vmath::vec3 &offset, bool back_face)
        {
            const auto &tri_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
            for (size_t f = 0; f < 4; f++)
                BuildTriangleIndexed(vertices, indices,
                                     TETRA_BASE_POSITION, COLOR_ALL_WHITE_4,
                                     TRIANGLE_BASE_MESH_UVS, TETRA_FACE_INDICES[f],
                                     offset, tri_idxs);
        }

        void BuildOctahedronIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
                                    const vmath::vec3 &offset, bool back_face)
        {
            // 위·아래 사각뿔(CONE_SIDE 의 y 축 거울상). 기본은 윗절반=FRONT, 아랫절반=BACK
            // (xz 거울상이라 서로 반대 winding). back_face=true 면 둘 다 뒤집음.
            const auto &top_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
            const auto &bottom_idxs = back_face ? TRIANGLE_FACE_INDICES : TRIANGLE_FACE_INDICES_BACK;

            for (size_t f = 0; f < 4; f++)
                BuildTriangleIndexed(vertices, indices,
                                     CONE_SIDE_BASE_POSITION, COLOR_ALL_WHITE_4,
                                     TRIANGLE_BASE_MESH_UVS, CONE_SIDE_FACE_INDICES[f],
                                     offset, top_idxs);

            std::vector<vmath::vec4> coneDownSideBasePosition;
            vmath::mat4 xzMirrorMat = vmath::mat4::identity();
            xzMirrorMat[1][1] = -1;
            for (const auto &pos : CONE_SIDE_BASE_POSITION)
                coneDownSideBasePosition.push_back(pos * xzMirrorMat);

            for (size_t f = 0; f < 4; f++)
                BuildTriangleIndexed(vertices, indices,
                                     coneDownSideBasePosition, COLOR_ALL_WHITE_4,
                                     TRIANGLE_BASE_INV_MESH_UVS, CONE_SIDE_FACE_INDICES[f],
                                     offset, bottom_idxs);
        }

        void BuildDiskIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
                              double us, double ue, int uRes,
                              double vs, double ve, int vRes,
                              float radius,
                              const vmath::vec3 &offset, bool back_face)
        {
            // XZ 평면 원판/고리(ring). vs=0 -> 꽉찬 디스크, vs>0 -> ring.
            int numCols = uRes + 1;
            int numRows = vRes + 1;

            const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

            double deltaRad = (ve - vs) / (float)vRes;
            double deltaAngle = (ue - us) / (float)uRes;

            const vmath::vec3 diskNormal = back_face ? vmath::vec3(0.0f, -1.0f, 0.0f)
                                                    : vmath::vec3(0.0f, 1.0f, 0.0f);
            const vmath::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

            for (int row = 0; row < numRows; row++)
            {
                for (int col = 0; col < numCols; col++)
                {
                    double currentRad = (vs + row * deltaRad) * radius;
                    double currentAngle = (us + col * deltaAngle);

                    vmath::vec4 pos(
                        (float)(currentRad * cos(currentAngle)),
                        0.0f,
                        (float)(-currentRad * sin(currentAngle)),
                        1.0f);
                    vmath::vec2 uv((float)col / (float)uRes, (float)row / (float)vRes);
                    PushVertex(vertices, pos, white, diskNormal, uv, offset);
                }
            }

            // Disk 는 cylinder/hemisphere 와 달리 normal 이 ±Y (라디알 아님). 위치식 z=-sin(θ) 는
            // +Y 에서 봤을 때 CW 회전이라, 표준 winding {p0,p1,p2,p0,p2,p3} 는 cross=-Y 가 된다.
            // → +Y 외향(back_face=false)을 보장하려면 인덱스를 뒤집어야 함.
            for (int row = 0; row < vRes; row++)
            {
                for (int col = 0; col < uRes; col++)
                {
                    GLuint p0 = base + static_cast<GLuint>(row * numCols + col);
                    GLuint p1 = base + static_cast<GLuint>(row * numCols + (col + 1));
                    GLuint p2 = base + static_cast<GLuint>((row + 1) * numCols + (col + 1));
                    GLuint p3 = base + static_cast<GLuint>((row + 1) * numCols + col);
                    if (!back_face)
                    {
                        // CCW 외향 = +Y. (p0,p3,p2) cross = +Y, (p0,p2,p1) cross = +Y.
                        indices.push_back(p0);
                        indices.push_back(p3);
                        indices.push_back(p2);
                        indices.push_back(p0);
                        indices.push_back(p2);
                        indices.push_back(p1);
                    }
                    else
                    {
                        // CCW 외향 = -Y. 위치식이 +Y 에서 CW 라서 표준 winding 이 곧 -Y front.
                        indices.push_back(p0);
                        indices.push_back(p1);
                        indices.push_back(p2);
                        indices.push_back(p0);
                        indices.push_back(p2);
                        indices.push_back(p3);
                    }
                }
            }
        }

        void BuildCylinderIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
                                  double us, double ue, int uRes,
                                  double vs, double ve, int vRes,
                                  float radius, float height,
                                  const vmath::vec3 &offset, bool back_face)
        {
            // XZ 평면에 base, +Y 로 높이 height. 법선은 축에서 바깥으로 (xz 라디알).
            int numCols = uRes + 1;
            int numRows = vRes + 1;

            const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

            double deltaV = (ve - vs) / (float)vRes;
            double deltaAngle = (ue - us) / (float)uRes;

            const vmath::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

            for (int row = 0; row < numRows; row++)
            {
                for (int col = 0; col < numCols; col++)
                {
                    double currentV = (vs + row * deltaV) * height;
                    double currentAngle = (us + col * deltaAngle);

                    vmath::vec4 pos(
                        (float)(radius * cos(currentAngle)),
                        (float)currentV,
                        (float)(-radius * sin(currentAngle)),
                        1.0f);
                    vmath::vec2 uv((float)col / (float)uRes, (float)row / (float)vRes);
                    vmath::vec3 outN((float)cos(currentAngle), 0.0f, (float)-sin(currentAngle));
                    vmath::vec3 normal = back_face ? -outN : outN;
                    PushVertex(vertices, pos, white, normal, uv, offset);
                }
            }

            for (int row = 0; row < vRes; row++)
            {
                for (int col = 0; col < uRes; col++)
                {
                    GLuint p0 = base + static_cast<GLuint>(row * numCols + col);
                    GLuint p1 = base + static_cast<GLuint>(row * numCols + (col + 1));
                    GLuint p2 = base + static_cast<GLuint>((row + 1) * numCols + (col + 1));
                    GLuint p3 = base + static_cast<GLuint>((row + 1) * numCols + col);
                    if (!back_face)
                    {
                        indices.push_back(p0);
                        indices.push_back(p1);
                        indices.push_back(p2);
                        indices.push_back(p0);
                        indices.push_back(p2);
                        indices.push_back(p3);
                    }
                    else
                    {
                        indices.push_back(p0);
                        indices.push_back(p2);
                        indices.push_back(p1);
                        indices.push_back(p0);
                        indices.push_back(p3);
                        indices.push_back(p2);
                    }
                }
            }
        }

        void BuildHemiSphereIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
                                    double us, double ue, int uRes,
                                    double vs, double ve, int vRes,
                                    float radius,
                                    const vmath::vec3 &offset, bool back_face)
        {
            // 북반구(+Y), 중심 원점. 법선 = 중심→정점 (구면).
            int numCols = uRes + 1;
            int numRows = vRes + 1;

            const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

            double deltaV = (ve - vs) / (float)vRes;
            double deltaAngle = (ue - us) / (float)uRes;

            const vmath::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

            for (int row = 0; row < numRows; row++)
            {
                for (int col = 0; col < numCols; col++)
                {
                    double currentV = vs + row * deltaV;
                    double latitude = currentV * (M_PI / 2.0);
                    double currentAngle = (us + col * deltaAngle);

                    double r = radius * cos(latitude);
                    double y = radius * sin(latitude);

                    float px = (float)(r * cos(currentAngle));
                    float py = (float)y;
                    float pz = (float)(-r * sin(currentAngle));

                    vmath::vec4 pos(px, py, pz, 1.0f);
                    vmath::vec2 uv((float)col / (float)uRes, (float)row / (float)vRes);
                    vmath::vec3 outN = vmath::normalize(vmath::vec3(px, py, pz));
                    vmath::vec3 normal = back_face ? -outN : outN;
                    PushVertex(vertices, pos, white, normal, uv, offset);
                }
            }

            for (int row = 0; row < vRes; row++)
            {
                for (int col = 0; col < uRes; col++)
                {
                    GLuint p0 = base + static_cast<GLuint>(row * numCols + col);
                    GLuint p1 = base + static_cast<GLuint>(row * numCols + (col + 1));
                    GLuint p2 = base + static_cast<GLuint>((row + 1) * numCols + (col + 1));
                    GLuint p3 = base + static_cast<GLuint>((row + 1) * numCols + col);
                    if (!back_face)
                    {
                        indices.push_back(p0);
                        indices.push_back(p1);
                        indices.push_back(p2);
                        indices.push_back(p0);
                        indices.push_back(p2);
                        indices.push_back(p3);
                    }
                    else
                    {
                        indices.push_back(p0);
                        indices.push_back(p2);
                        indices.push_back(p1);
                        indices.push_back(p0);
                        indices.push_back(p3);
                        indices.push_back(p2);
                    }
                }
            }
        }

        // === 13-float interleaved → MeshData 변환 ===
        // 빌더의 raw(pos4 + color4 + normal3 + uv2) 출력을 SJH::Vertex(pos3 + normal3 + uv2) 로 변환.
        // color(4 float) 와 pos.w 는 폐기.
        MeshData FromInterleaved(const std::vector<GLfloat> &raw,
                                 const std::vector<GLuint> &idx)
        {
            constexpr int STRIDE = VERTEX_LEN; // 13
            MeshData data;
            const size_t count = raw.size() / STRIDE;
            data.vertices.reserve(count);
            for (size_t i = 0; i < count; i++)
            {
                const size_t b = i * STRIDE;
                Vertex v;
                v.position = vmath::vec3(raw[b + 0], raw[b + 1], raw[b + 2]);
                v.normal = vmath::vec3(raw[b + 8], raw[b + 9], raw[b + 10]);
                v.texCoord = vmath::vec2(raw[b + 11], raw[b + 12]);
                data.vertices.push_back(v);
            }
            data.indices.reserve(idx.size());
            for (GLuint k : idx)
                data.indices.push_back(static_cast<uint32_t>(k));
            return data;
        }
    } // namespace

    namespace Geometry
    {
        MeshData Box(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            BuildCubeIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData ScreenQuad()
        {
            // NDC clip-space 화면 가득 quad — postprocess.vs 가 model/view/proj 우회.
            // position 은 NDC 좌표, UV 는 화면 매핑 (좌하단 0,0 → 우상단 1,1).
            // normal 은 +Z (사용 안 함, Vertex 구조체 충족).
            MeshData data;
            data.vertices = {
                { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
                { {  1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
                { {  1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
                { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
            };
            data.indices = { 0, 1, 2, 0, 2, 3 };
            return data;
        }

        MeshData Plane(bool back_face)
        {
            using namespace Const::GEOMETRY;
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            // BuildPlaneIndexed 가 없어 저수준 BuildQuadIndexed 를 QUAD_BASE_POSITION(z=0 XY quad)
            // 으로 직접 호출 — Cone 바닥 quad 와 동일 패턴.
            BuildQuadIndexed(
                raw, idx,
                QUAD_BASE_POSITION, COLOR_ALL_WHITE_6, QUAD_BASE_MESH_UVS,
                QUAD_MESH_UVS_FAN,
                vmath::vec3(-0.5f, -0.5f, 0.0f),
                back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES);
            return FromInterleaved(raw, idx);
        }

        MeshData Cone(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            BuildConeIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Tetrahedron(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            BuildTetrahedronIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Octahedron(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            BuildOctahedronIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Disk(double us, double ue, int uRes,
                      double vs, double ve, int vRes,
                      float radius, bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            BuildDiskIndexed(
                raw, idx, us, ue, uRes, vs, ve, vRes,
                radius, vmath::vec3(0.0f, 0.0f, 0.0f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Cylinder(double us, double ue, int uRes,
                          double vs, double ve, int vRes,
                          float radius, float height, bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            BuildCylinderIndexed(
                raw, idx, us, ue, uRes, vs, ve, vRes,
                radius, height, vmath::vec3(0.0f, 0.0f, 0.0f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData HemiSphere(double us, double ue, int uRes,
                            double vs, double ve, int vRes,
                            float radius, bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            BuildHemiSphereIndexed(
                raw, idx, us, ue, uRes, vs, ve, vRes,
                radius, vmath::vec3(0.0f, 0.0f, 0.0f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData FromAssimp(const aiMesh *mesh)
        {
            // assimp Triangulate 전처리 가정 — 모든 face 는 3 인덱스. position/normal/texCoord
            // 채널을 1:1 로 복사. UV 는 채널 0 (mTextureCoords[0]) 만 사용.
            MeshData data;
            data.vertices.resize(mesh->mNumVertices);
            for (uint32_t i = 0; i < mesh->mNumVertices; i++)
            {
                auto &v = data.vertices[i];
                v.position = vmath::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
                v.normal = vmath::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
                v.texCoord = vmath::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            }

            data.indices.resize(static_cast<size_t>(mesh->mNumFaces) * 3);
            for (uint32_t i = 0; i < mesh->mNumFaces; i++)
            {
                data.indices[3 * i + 0] = mesh->mFaces[i].mIndices[0];
                data.indices[3 * i + 1] = mesh->mFaces[i].mIndices[1];
                data.indices[3 * i + 2] = mesh->mFaces[i].mIndices[2];
            }
            return data;
        }
    } // namespace Geometry
} // namespace SJH
