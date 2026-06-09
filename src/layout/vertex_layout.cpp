/**
 * @file vertex_layout.cpp
 * @brief VertexLayout - VAO RAII 래퍼 구현.
 *
 * @details
 *  ### 책임
 *  - @c glGenVertexArrays / @c glDeleteVertexArrays RAII 생애주기 구현.
 *  - @c TrySetAttrib : @c glEnableVertexAttribArray + @c glVertexAttribPointer 순차 실행,
 *    각 단계 후 @c SJH::Diagnostics::GLDebug 진단 호출 및 결과 전파.
 *  - @c Bind : @c glBindVertexArray 후 @c GLDebug::CheckGLBindVertexArray 진단.
 *
 *  ### 비-책임
 *  - [X] VBO/EBO 데이터 업로드 (@c glBufferData) - @c SJH::Buffer 담당.
 *  - [X] draw 호출 (@c glDrawArrays / @c glDrawElements) - 상위 렌더러 담당.
 *
 * @note @c Init() 의 @c GLDebug::CheckGLGenVertexArrays 호출은 n=1 고정이라
 *       에러 발생 여지가 거의 없지만, @c buffer.cpp 의 Gen/Bind/BufferData 체크와
 *       *눈으로 보이는 대칭성* 유지를 위해 존재한다.
 */
#include "vertex_layout.h"
#include "diagnostics/gl_log.h"
#include <memory>

namespace SJH
{
    VertexLayoutUPtr VertexLayout::Create()
    {
        auto vao = std::unique_ptr<VertexLayout>(new VertexLayout());
        vao->Init();
        return std::move(vao);
    }

    VertexLayout::~VertexLayout()
    {
        if (mVertexArrayObject != 0)
            glDeleteVertexArrays(1, &mVertexArrayObject);
    }

    bool VertexLayout::Bind() const
    {
        glBindVertexArray(mVertexArrayObject);
        return Diagnostics::GLDebug::CheckGLBindVertexArray(mVertexArrayObject);
    }

    bool VertexLayout::TrySetAttrib(GLuint attrib_index, int count, GLuint type, bool normalized, GLsizei stride, uint64_t offset)
    {
        glEnableVertexAttribArray(attrib_index);
        if (!SJH::Diagnostics::GLDebug::CheckGLEnableVertexAttribArray(attrib_index))
            return false;

        glVertexAttribPointer(attrib_index, count, type, normalized, stride, (const void *)offset);
        if (!SJH::Diagnostics::GLDebug::CheckGLVertexAttribPointer({stride}))
            return false;

        return true;
    }

    void VertexLayout::DisableAttrib(int attrib_idx) const
    {
    }

    void VertexLayout::Init()
    {
        glGenVertexArrays(1, &mVertexArrayObject);
        // buffer.cpp Gen/Bind/BufferData 체크와 *눈으로 보이는 대칭성* 유지.
        Diagnostics::GLDebug::CheckGLGenVertexArrays();
        Bind();
    }

}
