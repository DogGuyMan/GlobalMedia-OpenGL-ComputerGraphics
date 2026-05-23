#ifndef __SJH_BUFFER_H__
#define __SJH_BUFFER_H__

#include "common/common.h"
#include "GL/gl3w.h"

// SharedPtr을 사용하자.
// VBO EBO는 다른 VAO와 연결하여 재사용할 수 있다.
namespace SJH
{
    CLASS_PTR(Buffer)

    /**
     * @brief VBO/EBO 통합 RAII 래퍼 — `glGenBuffers` ~ `glDeleteBuffers` 자원 수명 관리.
     *
     * @details
     *  ### 개념
     *  - **VBO** (Vertex Buffer Object, @c GL_ARRAY_BUFFER) — 정점 *데이터*. CPU 메모리의
     *    정점 배열을 GPU 로 옮긴 raw byte 묶음 (position/normal/color/uv 등 interleaved 가능).
     *  - **EBO** (Element Buffer Object, @c GL_ELEMENT_ARRAY_BUFFER) — 인덱스 데이터.
     *    어떤 정점을 어떤 순서로 그릴지 지정 (`glDrawElements` 가 사용).
     *  - 둘은 GL 객체 종류가 같음 — @c buffer_type 인자로만 구분 -> 동일 클래스로 통합.
     *
     *  ### 다른 GL 객체와의 경계 — VAO 는 별도
     *  - VBO/EBO 는 *데이터*. 그 데이터의 *구조* (layout) 를 알려주는 descriptor 는 @c SJH::VertexLayout
     *    (VAO, `src/layout/`) 의 책임.
     *
     *  ### 호출 흐름
     *  ```
     *  VAO 바인딩 -> VBO 생성/바인딩 + 데이터 업로드 -> glVertexAttribPointer
     *  ```
     *  VAO 바인딩 *없이* `Buffer::CreateWithData(GL_ARRAY_BUFFER, ...)` 만 호출해도 GL 자체는
     *  성공하지만, 후속 `glVertexAttribPointer` 가 GL_INVALID_OPERATION (3.3 core 강제).
     *
     *  ### 진단 통합
     *  `Init()` / `Bind()` 내부에 `Diagnostics::GLDebug::CheckGLGenBuffers` /
     *  `CheckGLBindBuffer` / `CheckGLBufferData` 호출 — 실패 시 즉시 `false` 전파.
     */
    class Buffer
    {
    public:
        /**
         * @brief 데이터를 업로드한 새 buffer 객체를 생성 (팩토리).
         * @param buffer_type @c GL_ARRAY_BUFFER (VBO) 또는 @c GL_ELEMENT_ARRAY_BUFFER (EBO).
         * @param usage       @c GL_STATIC_DRAW / @c GL_DYNAMIC_DRAW / @c GL_STREAM_DRAW.
         * @param data        업로드 원본 포인터.
         * @param data_size   바이트 단위 크기.
         * @return 성공 시 @c BufferUPtr, 진단 실패 시 @c nullptr.
         * @warning `data` 의 *바이트 내용* 만 GPU 로 복사되며 *타입 정보*는 사라짐.
         *          잘못된 타입 (예: 인덱스 버퍼에 GLfloat) 은 진단으로 못 잡힘 — 호출자 책임.
         */
        static BufferUPtr CreateWithData(GLuint buffer_type, GLuint usage,
                                         const void *data, size_t stride, size_t count);

        /// @brief @c glDeleteBuffers 호출 (핸들이 0 이 아닐 때만).
        ~Buffer();

        /// @brief 내부 GL 버퍼 핸들 반환 — 디버깅 / 직접 GL 호출 시 사용.
        GLuint Get() const { return mBuffer; }
        size_t GetStride() const {return mStride;}
        size_t GetCount() const {return mCount;}

        /// @brief 본 버퍼를 자신의 @c bufferType 슬롯에 바인딩 (`glBindBuffer`).
        /// @return 진단 통과 시 @c true. 실패 시 spdlog 출력 + @c false.
        bool Bind() const;

    private:
        Buffer() = default;

        /// @brief Gen + Bind + 데이터 업로드 + 각 단계 진단. `CreateWithData` 내부에서만 호출.
        bool Init(GLuint buffer_type, GLuint usage, const void* data, size_t stride, size_t count);

        GLuint mBuffer{0};
        GLuint mBufferType{0};
        GLuint mUsage{0};
        size_t mStride {0};
        size_t mCount {0};
    };

}
#endif // __BUFFER_H__
