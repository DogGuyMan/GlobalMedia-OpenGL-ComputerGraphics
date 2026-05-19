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
        // 누락 진단 적용 — buffer.cpp가 Gen/Bind/BufferData 모두 체크하는 것과 일관성 맞춤.
        // n=1 고정이라 GL_INVALID_VALUE 발생할 일 거의 없지만, 다른 GLDebug 호출과의 *대칭성*이 더 큰 가치
        // (smell linter가 호출 누락을 체크하기 어려운 영역에서 *눈으로 보이는 일관성*이 핵심).
        Diagnostics::GLDebug::CheckGLGenVertexArrays();
        Bind();
    }

}
