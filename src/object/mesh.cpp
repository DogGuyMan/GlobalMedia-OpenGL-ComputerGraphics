#include "mesh.h"
#include "diagnostics/gl_validate.h" // Cat A — CheckIndices

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

        // Cat A 진단 — EBO 인덱스 OOB / degenerate / duplicate 검사.
        // CPU 측 vector 만 검사하므로 GL state 변경 없음.
        Diagnostics::GLValidate::CheckIndices(indices, vertices.size(), "Mesh::Init");
    }

    MeshUPtr Mesh::CreateBox()
    {
        // clang-format off
        std::vector<Vertex> vertices = {
            Vertex{vmath::vec3(-0.5f, -0.5f, -0.5f), vmath::vec3(0.0f, 0.0f, -1.0f), vmath::vec2(0.0f, 0.0f)},
            Vertex{vmath::vec3(0.5f, -0.5f, -0.5f), vmath::vec3(0.0f, 0.0f, -1.0f), vmath::vec2(1.0f, 0.0f)},
            Vertex{vmath::vec3(0.5f, 0.5f, -0.5f), vmath::vec3(0.0f, 0.0f, -1.0f), vmath::vec2(1.0f, 1.0f)},
            Vertex{vmath::vec3(-0.5f, 0.5f, -0.5f), vmath::vec3(0.0f, 0.0f, -1.0f), vmath::vec2(0.0f, 1.0f)},

            Vertex{vmath::vec3(-0.5f, -0.5f, 0.5f), vmath::vec3(0.0f, 0.0f, 1.0f), vmath::vec2(0.0f, 0.0f)},
            Vertex{vmath::vec3(0.5f, -0.5f, 0.5f), vmath::vec3(0.0f, 0.0f, 1.0f), vmath::vec2(1.0f, 0.0f)},
            Vertex{vmath::vec3(0.5f, 0.5f, 0.5f), vmath::vec3(0.0f, 0.0f, 1.0f), vmath::vec2(1.0f, 1.0f)},
            Vertex{vmath::vec3(-0.5f, 0.5f, 0.5f), vmath::vec3(0.0f, 0.0f, 1.0f), vmath::vec2(0.0f, 1.0f)},

            Vertex{vmath::vec3(-0.5f, 0.5f, 0.5f), vmath::vec3(-1.0f, 0.0f, 0.0f), vmath::vec2(1.0f, 0.0f)},
            Vertex{vmath::vec3(-0.5f, 0.5f, -0.5f), vmath::vec3(-1.0f, 0.0f, 0.0f), vmath::vec2(1.0f, 1.0f)},
            Vertex{vmath::vec3(-0.5f, -0.5f, -0.5f), vmath::vec3(-1.0f, 0.0f, 0.0f), vmath::vec2(0.0f, 1.0f)},
            Vertex{vmath::vec3(-0.5f, -0.5f, 0.5f), vmath::vec3(-1.0f, 0.0f, 0.0f), vmath::vec2(0.0f, 0.0f)},

            Vertex{vmath::vec3(0.5f, 0.5f, 0.5f), vmath::vec3(1.0f, 0.0f, 0.0f), vmath::vec2(1.0f, 0.0f)},
            Vertex{vmath::vec3(0.5f, 0.5f, -0.5f), vmath::vec3(1.0f, 0.0f, 0.0f), vmath::vec2(1.0f, 1.0f)},
            Vertex{vmath::vec3(0.5f, -0.5f, -0.5f), vmath::vec3(1.0f, 0.0f, 0.0f), vmath::vec2(0.0f, 1.0f)},
            Vertex{vmath::vec3(0.5f, -0.5f, 0.5f), vmath::vec3(1.0f, 0.0f, 0.0f), vmath::vec2(0.0f, 0.0f)},

            Vertex{vmath::vec3(-0.5f, -0.5f, -0.5f), vmath::vec3(0.0f, -1.0f, 0.0f), vmath::vec2(0.0f, 1.0f)},
            Vertex{vmath::vec3(0.5f, -0.5f, -0.5f), vmath::vec3(0.0f, -1.0f, 0.0f), vmath::vec2(1.0f, 1.0f)},
            Vertex{vmath::vec3(0.5f, -0.5f, 0.5f), vmath::vec3(0.0f, -1.0f, 0.0f), vmath::vec2(1.0f, 0.0f)},
            Vertex{vmath::vec3(-0.5f, -0.5f, 0.5f), vmath::vec3(0.0f, -1.0f, 0.0f), vmath::vec2(0.0f, 0.0f)},

            Vertex{vmath::vec3(-0.5f, 0.5f, -0.5f), vmath::vec3(0.0f, 1.0f, 0.0f), vmath::vec2(0.0f, 1.0f)},
            Vertex{vmath::vec3(0.5f, 0.5f, -0.5f), vmath::vec3(0.0f, 1.0f, 0.0f), vmath::vec2(1.0f, 1.0f)},
            Vertex{vmath::vec3(0.5f, 0.5f, 0.5f), vmath::vec3(0.0f, 1.0f, 0.0f), vmath::vec2(1.0f, 0.0f)},
            Vertex{vmath::vec3(-0.5f, 0.5f, 0.5f), vmath::vec3(0.0f, 1.0f, 0.0f), vmath::vec2(0.0f, 0.0f)},
        };

        // 6면 × 12 인덱스 = 36. 각 면이 자기 4개 정점만 참조 (i4~i6 가 오작성되어 있던 것을 정정).
        std::vector<uint32_t> indices = {
             0,  2,  1,    2,  0,  3,   // back   (정점 0~3, normal -Z)
             4,  5,  6,    6,  7,  4,   // front  (정점 4~7, normal +Z)
             8,  9, 10,   10, 11,  8,   // left   (정점 8~11, normal -X)
            12, 14, 13,   14, 12, 15,   // right  (정점 12~15, normal +X)
            16, 17, 18,   18, 19, 16,   // bottom (정점 16~19, normal -Y)
            20, 22, 21,   22, 20, 23,   // top    (정점 20~23, normal +Y)
        };
        // clang-format on

        return Create(vertices, indices, GL_TRIANGLES);
    }

    MeshUPtr Mesh::CreatePlane()
    {
        // clang-format off
        std::vector<Vertex> vertices = {
            Vertex{vmath::vec3(-0.5f, -0.5f, 0.0f), vmath::vec3(0.0f, 0.0f, 1.0f), vmath::vec2(0.0f, 0.0f)},
            Vertex{vmath::vec3(0.5f, -0.5f, 0.0f), vmath::vec3(0.0f, 0.0f, 1.0f), vmath::vec2(1.0f, 0.0f)},
            Vertex{vmath::vec3(0.5f, 0.5f, 0.0f), vmath::vec3(0.0f, 0.0f, 1.0f), vmath::vec2(1.0f, 1.0f)},
            Vertex{vmath::vec3(-0.5f, 0.5f, 0.0f), vmath::vec3(0.0f, 0.0f, 1.0f), vmath::vec2(0.0f, 1.0f)},
        };

        std::vector<uint32_t> indices = {
            0,1,2,
            2,3,0,
        };
        // clang-format on

        return Create(vertices, indices, GL_TRIANGLES);
    }

    void Mesh::Draw() const
    {
        mVertexLayout->Bind();
        glDrawElements(mPrimitiveType, mIndexBuffer->GetCount(), GL_UNSIGNED_INT, 0);
    }
}
