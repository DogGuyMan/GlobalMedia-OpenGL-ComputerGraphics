/**
 * @file mesh.cpp
 * @brief Mesh RAII 래퍼 구현 - GPU 업로드, VAO/VBO/EBO 초기화, 팩토리 함수.
 * @details
 *  ### 책임
 *  - `Mesh::Create` - 정점/인덱스 배열을 받아 GPU 에 업로드하고 @c VertexLayout 로 VAO 구성.
 *  - `Mesh::CreateBox / CreatePlane / CreateScreenQuad` - @c Geometry 네임스페이스에 도형 데이터를
 *    위임 후 @c Create 로 GPU 업로드 (팩토리 편의 래퍼).
 *  - `Mesh::GetVAO / GetIndexCount` - 렌더 핸들 getter (nullptr guard 포함).
 *
 *  ### Vertex 레이아웃 약속
 *  - attrib 0 : position (vec3, @c offsetof(Vertex, position))
 *  - attrib 1 : normal   (vec3, @c offsetof(Vertex, normal))
 *  - attrib 2 : texCoord (vec2, @c offsetof(Vertex, texCoord))
 *
 * @note `Mesh::Init` 마지막에 `Diagnostics::GLValidate::CheckIndices` (Cat A) 를 호출해
 *       EBO 인덱스 OOB / degenerate / duplicate 를 CPU 측에서 검사한다 (GL state 변경 없음).
 */
#include "mesh.h"
#include "object/geometry.h"
#include "diagnostics/gl_validate.h" // Cat A - CheckIndices

namespace SJH
{
    MeshUPtr Mesh::Create(
        const std::vector<Vertex> &vertices,
        const std::vector<uint32_t> &indices,
        uint32_t primitiveType)
    {
        auto mesh = MeshUPtr(new Mesh());
        mesh->Init(vertices, indices, primitiveType);
        return std::move(mesh);
    }

    void Mesh::Init(
        const std::vector<Vertex> &vertices,
        const std::vector<uint32_t> &indices,
        uint32_t primitiveType)
    {
        mVertexLayout = VertexLayout::Create();
        mVertexBuffer = Buffer::CreateWithData(GL_ARRAY_BUFFER, GL_STATIC_DRAW, vertices.data(), sizeof(Vertex), vertices.size());
        mIndexBuffer = Buffer::CreateWithData(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, indices.data(), sizeof(uint32_t), indices.size());
        mVertexLayout->TrySetAttrib(0, 3, GL_FLOAT, false, sizeof(Vertex), offsetof(Vertex, position));
        // offsetof -> Vertex 구조체에 normal, texCoord 등등 얼마나 오프셋이 되어 있냐를 사용할 수 있다.
        mVertexLayout->TrySetAttrib(1, 3, GL_FLOAT, false, sizeof(Vertex), offsetof(Vertex, normal));
        mVertexLayout->TrySetAttrib(2, 2, GL_FLOAT, false, sizeof(Vertex), offsetof(Vertex, texCoord));

        // Cat A 진단 - EBO 인덱스 OOB / degenerate / duplicate 검사.
        // CPU 측 vector 만 검사하므로 GL state 변경 없음.
        Diagnostics::GLValidate::CheckIndices(indices, vertices.size(), "Mesh::Init");
    }

    MeshUPtr Mesh::CreateBox()
    {
        // 도형 데이터 생성은 SJH::Geometry 책임 - engine 빌더에 위임.
        MeshData data = Geometry::Box();
        return Create(data.vertices, data.indices, GL_TRIANGLES);
    }

    MeshUPtr Mesh::CreatePlane()
    {
        // 도형 데이터 생성은 SJH::Geometry 책임 - engine 빌더에 위임.
        MeshData data = Geometry::Plane();
        return Create(data.vertices, data.indices, GL_TRIANGLES);
    }

    MeshUPtr Mesh::CreateScreenQuad()
    {
        // NDC clip-space 화면 가득 quad - SP4 post-processing 패스용.
        MeshData data = Geometry::ScreenQuad();
        return Create(data.vertices, data.indices, GL_TRIANGLES);
    }

    GLuint Mesh::GetVAO() const
    {
        return mVertexLayout ? mVertexLayout->GetVAO() : 0;
    }

    GLsizei Mesh::GetIndexCount() const
    {
        return mIndexBuffer ? static_cast<GLsizei>(mIndexBuffer->GetCount()) : 0;
    }
}
