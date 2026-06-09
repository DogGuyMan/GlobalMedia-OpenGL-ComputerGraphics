/**
 * @file buffer.h
 * @brief VBO/EBO 통합 RAII 래퍼 - GPU 버퍼 자원의 생성/바인딩/소멸을 단일 클래스로 관리.
 *
 * @details
 *  ### 책임
 *  - @c glGenBuffers ~ @c glDeleteBuffers 수명 관리.
 *  - VBO (@c GL_ARRAY_BUFFER) 와 EBO (@c GL_ELEMENT_ARRAY_BUFFER) 를 @c buffer_type 인자로 통합.
 *  - stride/count 보관 - VertexLayout 이 @c glVertexAttribPointer 호출 시 참조.
 *  - 진단 통합: Init/Bind 각 단계에서 @c Diagnostics::GLDebug::Check* 호출, 실패 즉시 전파.
 *
 *  ### 비-책임
 *  - [X] 정점 *구조(layout)* 정의 - VAO + @c glVertexAttribPointer 는 @c SJH::VertexLayout (@c src/layout/) 책임.
 *  - [X] VAO 바인딩 - Buffer 는 데이터 전용. 올바른 사용 순서:
 *    VAO bind -> VBO 생성/bind + 업로드 -> @c glVertexAttribPointer.
 *
 * @note VBO/EBO 는 @c shared_ptr(@c BufferPtr) 로 선언 - 여러 VAO 가 동일 버퍼를 재사용 가능.
 */

#ifndef __SJH_BUFFER_H__
#define __SJH_BUFFER_H__

#include "common/common.h"
#include "GL/gl3w.h"

namespace SJH
{
    CLASS_PTR(Buffer)

    /**
     * @brief VBO/EBO 통합 RAII 래퍼 - @c glGenBuffers ~ @c glDeleteBuffers 자원 수명 관리.
     *
     * @details
     *  ### 개념
     *  - **VBO** (Vertex Buffer Object, @c GL_ARRAY_BUFFER) - 정점 *데이터*. CPU 메모리의
     *    정점 배열을 GPU 로 옮긴 raw byte 묶음 (position/normal/color/uv 등 interleaved 가능).
     *  - **EBO** (Element Buffer Object, @c GL_ELEMENT_ARRAY_BUFFER) - 인덱스 데이터.
     *    어떤 정점을 어떤 순서로 그릴지 지정 (@c glDrawElements 가 사용).
     *  - 둘은 GL 객체 종류가 같음 - @c buffer_type 인자로만 구분 -> 동일 클래스로 통합.
     *
     *  ### 다른 GL 객체와의 경계 - VAO 는 별도
     *  VBO/EBO 는 *데이터*. 그 데이터의 *구조(layout)* 를 알려주는 descriptor 는
     *  @c SJH::VertexLayout (VAO, @c src/layout/) 의 책임.
     *
     *  ### 진단 통합
     *  @c Init() / @c Bind() 내부에 @c Diagnostics::GLDebug::CheckGLGenBuffers /
     *  @c CheckGLBindBuffer / @c CheckGLBufferData 호출 - 실패 시 즉시 @c false 전파.
     */
    class Buffer
    {
    public:
        /**
         * @brief 데이터를 업로드한 새 Buffer 객체를 생성하는 팩토리.
         * @param buffer_type @c GL_ARRAY_BUFFER (VBO) 또는 @c GL_ELEMENT_ARRAY_BUFFER (EBO).
         * @param usage       @c GL_STATIC_DRAW / @c GL_DYNAMIC_DRAW / @c GL_STREAM_DRAW.
         * @param data        업로드 원본 포인터 (호출 반환 후 해제 가능).
         * @param stride      정점 1개(또는 인덱스 1개)의 바이트 크기.
         * @param count       요소(정점/인덱스) 개수. 업로드 총 바이트 = @c stride * @c count.
         * @return 성공 시 @c BufferUPtr, 진단 실패 시 @c nullptr.
         * @warning @p data 의 *바이트 내용*만 GPU 로 복사 - 타입 정보는 사라짐.
         *          잘못된 타입(예: 인덱스 버퍼에 @c GLfloat)은 진단으로 못 잡힘 - 호출자 책임.
         */
        static BufferUPtr CreateWithData(GLuint buffer_type, GLuint usage,
                                         const void *data, size_t stride, size_t count);

        /// @brief @c glDeleteBuffers 호출 (핸들이 0 이 아닐 때만).
        ~Buffer();

        /// @brief 내부 GL 버퍼 핸들 반환 - 디버깅/직접 GL 호출 시 사용.
        GLuint Get() const { return mBuffer; }

        /// @brief 정점(또는 인덱스) 1개의 바이트 크기 반환.
        size_t GetStride() const { return mStride; }

        /// @brief 요소(정점/인덱스) 총 개수 반환.
        size_t GetCount() const { return mCount; }

        /// @brief 본 버퍼를 자신의 @c mBufferType 슬롯에 바인딩 (@c glBindBuffer).
        /// @return 진단 통과 시 @c true. 실패 시 spdlog 에러 출력 + @c false.
        bool Bind() const;

    private:
        Buffer() = default;

        /// @brief Gen + Bind + 데이터 업로드 + 각 단계 진단. @c CreateWithData 내부에서만 호출.
        bool Init(GLuint buffer_type, GLuint usage, const void* data, size_t stride, size_t count);

        GLuint mBuffer{0};      ///< GL 버퍼 객체 핸들.
        GLuint mBufferType{0};  ///< @c GL_ARRAY_BUFFER 또는 @c GL_ELEMENT_ARRAY_BUFFER.
        GLuint mUsage{0};       ///< @c GL_STATIC_DRAW / @c GL_DYNAMIC_DRAW / @c GL_STREAM_DRAW.
        size_t mStride{0};      ///< 요소 1개의 바이트 크기.
        size_t mCount{0};       ///< 요소 총 개수.
    };

} // namespace SJH
#endif // __SJH_BUFFER_H__
