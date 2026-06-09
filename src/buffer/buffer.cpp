/**
 * @file buffer.cpp
 * @brief Buffer 팩토리/소멸자/Init/Bind 구현.
 *
 * @details
 *  ### 책임
 *  - @c CreateWithData 팩토리: @c Buffer 힙 할당 -> @c Init 위임 -> 실패 시 @c nullptr 반환.
 *  - @c Init: @c glGenBuffers -> @c Bind -> @c glBufferData + 각 단계 @c Diagnostics::GLDebug::Check* 호출.
 *  - @c Bind: @c glBindBuffer + @c CheckGLBindBuffer 진단.
 *  - 소멸자: 핸들 유효 시 @c glDeleteBuffers.
 *
 *  ### 비-책임
 *  - [X] VAO / @c glVertexAttribPointer - @c SJH::VertexLayout (@c src/layout/) 책임.
 */
#include "buffer/buffer.h"
#include "diagnostics/gl_log.h"
#include <GL/glcorearb.h>
#include <memory>

namespace SJH
{
	BufferUPtr Buffer::CreateWithData(GLuint buffer_type, GLuint usage, const void *data, size_t stride, size_t count)
	{
		auto buffer = std::unique_ptr<Buffer>(new Buffer());
		if (!buffer->Init(buffer_type, usage, data, stride, count))
			return nullptr;
		return std::move(buffer);
	}

	Buffer::~Buffer()
	{
		if (mBuffer != 0)
			glDeleteBuffers(1, &mBuffer);
	}

	bool Buffer::Bind() const
	{
		glBindBuffer(mBufferType, mBuffer);
		return Diagnostics::GLDebug::CheckGLBindBuffer(mBuffer);
	}

	bool Buffer::Init(GLuint buffer_type, GLuint usage, const void *data, size_t stride, size_t count)
	{
		mBufferType = buffer_type; // 1. GL_ARRAY_BUFFER
		                           // 2. GL_ELEMENT_ARRAY_BUFFER

		mUsage = usage; // GL_STATIC_DRAW / GL_DYNAMIC_DRAW / GL_STREAM_DRAW

		mStride = stride;

		mCount = count;

		glGenBuffers(1, &mBuffer);
		if (!Diagnostics::GLDebug::CheckGLGenBuffers(mBuffer))
			return false;
		if (!Bind())
			return false;
		
		glBufferData(buffer_type, mStride * mCount, data, usage);
		if (!Diagnostics::GLDebug::CheckGLBufferData(stride * count))
			return false;
		return true;
	}
} // namespace SJH
