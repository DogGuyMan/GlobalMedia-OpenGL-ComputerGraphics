/**
 * @file uniform_buffer.cpp
 * @brief @c SJH::UniformBuffer 구현 - GL_UNIFORM_BUFFER 객체의 생성/갱신/결속/해제.
 *
 * @details
 *  - @c Create : private ctor + @c Init 위임 패턴 - 생성 실패(@c glGenBuffers 0) 시 nullptr.
 *  - @c Init   : @c glGenBuffers + @c glBufferData (@c GL_DYNAMIC_DRAW, nullptr data) 로 고정크기 할당.
 *                할당 직후 unbind - 다음 호출자가 @c GL_UNIFORM_BUFFER 타겟 오염 안 받게.
 *  - @c Update : @c glBindBuffer + @c glBufferSubData + unbind - 호출 단위 self-contained.
 *  - @c BindBase : @c glBindBufferBase - binding point 결속 상태 유지 (unbind 안 함, 그게 본질).
 *  - 소멸자: @c glDeleteBuffers (mBuffer != 0 가드).
 *
 * @note Phase 2 시점은 매 갱신마다 bind/unbind 하는 단순 구현. 향후 빈도가 문제되면
 *       @c glBindBufferRange 또는 persistent mapping 최적화 여지 있음.
 */
#include "program/uniform_buffer.h"

namespace SJH
{
    /// @copydoc UniformBuffer::Create
    UniformBufferUPtr UniformBuffer::Create(std::size_t size)
    {
        auto ubo = UniformBufferUPtr(new UniformBuffer());
        if (!ubo->Init(size))
            return nullptr;
        return ubo;
    }

    /// @copydoc UniformBuffer::Init
    bool UniformBuffer::Init(std::size_t size)
    {
        mSize = size;
        glGenBuffers(1, &mBuffer);
        if (mBuffer == 0)
            return false;

        // 고정크기 할당 - data=nullptr 로 GPU 메모리만 예약. 실 데이터는 Update 가 채움.
        glBindBuffer(GL_UNIFORM_BUFFER, mBuffer);
        glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        return true;
    }

    /// @copydoc UniformBuffer::~UniformBuffer
    UniformBuffer::~UniformBuffer()
    {
        if (mBuffer != 0)
            glDeleteBuffers(1, &mBuffer);
    }

    /// @copydoc UniformBuffer::Update
    void UniformBuffer::Update(const void* data, std::size_t bytes, std::size_t offset) const
    {
        glBindBuffer(GL_UNIFORM_BUFFER, mBuffer);
        glBufferSubData(GL_UNIFORM_BUFFER,
                        static_cast<GLintptr>(offset),
                        static_cast<GLsizeiptr>(bytes),
                        data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    /// @copydoc UniformBuffer::BindBase
    void UniformBuffer::BindBase(GLuint bindingPoint) const
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, mBuffer);
    }
}
