#include "object/geometry.h"
#include "engine/constants.h"
#include "engine/geometry.h"
#include <vmath.h>

namespace SJH
{
    namespace
    {
        // engine 의 13-float interleaved(pos4 + color4 + normal3 + uv2) 출력을
        // SJH::Vertex(pos3 + normal3 + uv2) 로 변환. color(4 float)는 폐기.
        MeshData FromInterleaved(const std::vector<GLfloat> &raw,
                                 const std::vector<GLuint> &idx)
        {
            constexpr int STRIDE = Engine::Constants::GEOMETRY::VERTEX_LEN; // 13
            MeshData data;
            const size_t count = raw.size() / STRIDE;
            data.vertices.reserve(count);
            for (size_t i = 0; i < count; i++)
            {
                const size_t b = i * STRIDE;
                Vertex v;
                v.position = vmath::vec3(raw[b + 0], raw[b + 1], raw[b + 2]); // pos.w 폐기
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
            Engine::Model::BuildCubeIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Plane(bool back_face)
        {
            using namespace Engine::Constants::GEOMETRY;
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            // engine 에 BuildPlaneIndexed 가 없어 저수준 BuildQuadIndexed 를
            // QUAD_BASE_POSITION(z=0 XY quad)으로 직접 호출 — Cone 바닥 quad 와 동일 패턴.
            Engine::Model::BuildQuadIndexed(
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
            Engine::Model::BuildConeIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Tetrahedron(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildTetrahedronIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Octahedron(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildOctahedronIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Disk(double us, double ue, int uRes,
                      double vs, double ve, int vRes,
                      float radius, bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildDiskIndexed(
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
            Engine::Model::BuildCylinderIndexed(
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
            Engine::Model::BuildHemiSphereIndexed(
                raw, idx, us, ue, uRes, vs, ve, vRes,
                radius, vmath::vec3(0.0f, 0.0f, 0.0f), back_face);
            return FromInterleaved(raw, idx);
        }
    } // namespace Geometry
} // namespace SJH
