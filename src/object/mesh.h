/**
 * @file mesh.h
 * @brief GL 메시 (VAO + VBO + EBO) RAII 래퍼 — 정점 배열 GPU 업로드 + 핸들 노출.
 *
 * @details
 *  ### 책임
 *  - Vertex 배열(VBO) + 인덱스 배열(EBO) 를 GL 에 업로드하고 VAO(@c VertexLayout) 로 묶음.
 *  - @c GetVAO / @c GetIndexCount getter 로 렌더 핸들을 노출 — 실제 GL 드로우는 RenderContext 가 수행.
 *  - @c CreateBox 팩토리로 기본 박스(큐브) 메시 생성.
 *  - @c CreatePlane 팩토리로 평면(quad) 메시 생성.
 *
 *  ### 비-책임
 *  - ❌ GL 드로우콜 직접 호출 — RenderContext::DrawIndexed 가 담당 (Pattern Y 정통).
 *  - ❌ 텍스처 바인딩 — Material / Context 가 담당.
 *  - ❌ 셰이더/uniform 전송 — Context::Render 가 담당.
 *  - ❌ VAO/VBO 재사용 최적화 — 현재 메시별 독립 VAO.
 */

#ifndef __MESH_H__
#define __MESH_H__

#include "buffer/buffer.h"
#include "common/common.h"
#include "layout/vertex_layout.h"
#include "object/vertex.h"
#include "GL/gl3w.h"
#include <vector>

namespace SJH
{
    CLASS_PTR(Mesh);
    /**
     * @brief GL 버퍼(VBO/EBO) + VAO 를 소유하는 메시 단위 RAII 래퍼.
     * @details
     *  정점 데이터를 GPU 에 업로드하고 @c GetVAO / @c GetIndexCount getter 로 렌더 핸들을 노출 —
     *  실제 드로우는 @c RenderContext 가 수행 (SP3 Pattern Y).
     *  소유 관계: @c Mesh -> @c VertexLayout (VAO) + @c Buffer ×2 (VBO/EBO).
     *  박스 메시는 @ref CreateBox 팩토리, 평면 메시는 @ref CreatePlane 팩토리가 제공.
     */
    class Mesh
    {
    public:
        /**
         * @brief 정점/인덱스 배열로 메시를 생성하고 GPU 에 업로드.
         * @param vertices      정점 배열 (position + normal + texCoord).
         * @param indices       인덱스 배열 (EBO 에 업로드).
         * @param primitiveType 드로우 토폴로지 (@c GL_TRIANGLES / @c GL_LINES 등).
         * @return 생성된 메시 (@c unique_ptr). 실패 시 @c nullptr.
         */
        static MeshUPtr Create(const std::vector<Vertex> &vertices, const std::vector<GLuint> &indices, GLuint primitiveType);

        /// @brief 기본 박스(큐브) 메시 생성 — 6면 × 2삼각형, 정점 24개, 법선/UV 포함.
        static MeshUPtr CreateBox();

        /// @brief 평면(quad) 메시 생성 — XZ 평면 1×1 사각형. 바닥면 + 포스트프로세스 화면 quad 겸용.
        static MeshUPtr CreatePlane();

        /// @brief 현재 메시의 VAO 관찰자. Mesh 보다 오래 보관 금지 — 소유자는 @c mVertexLayout.
        const VertexLayout *GetVertexLayout() const
        {
            return mVertexLayout.get();
        }
        /// @brief VBO 공유 포인터 — 정점 데이터 버퍼.
        BufferPtr GetVertexBuffer() const { return mVertexBuffer; }
        /// @brief EBO 공유 포인터 — 인덱스 데이터 버퍼.
        BufferPtr GetIndexBuffer() const { return mIndexBuffer; }
        /// @brief 드로우 토폴로지 반환 (@c GL_TRIANGLES / @c GL_LINES 등).
        GLuint GetPrimitiveType() const { return mPrimitiveType; }

        /// @brief VAO GL 핸들 — RenderContext::BindVAO 인자.
        GLuint GetVAO() const;

        /// @brief 인덱스 개수 — RenderContext::DrawIndexed 인자.
        GLsizei GetIndexCount() const;

    private:
        Mesh() = default;
        void Init(const std::vector<Vertex> &vertices, const std::vector<GLuint> &indices, GLuint primitiveType);

        GLuint mPrimitiveType{GL_TRIANGLES}; ///< 드로우 토폴로지 (@c GL_TRIANGLES / @c GL_LINES 등)
        VertexLayoutUPtr mVertexLayout;      ///< VAO 소유 — 메시와 수명 결합
        BufferPtr mVertexBuffer;             ///< VBO 공유 포인터 (정점 데이터)
        BufferPtr mIndexBuffer;              ///< EBO 공유 포인터 (인덱스 데이터)
    };

}

#endif // __MESH_H__
