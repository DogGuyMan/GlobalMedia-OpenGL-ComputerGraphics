/**
 * @file render_target.cpp
 * @brief DefaultRenderTarget 구현.
 *
 * @details
 *  ### 책임
 *  - @c DefaultRenderTarget::Bind() - FBO 0 바인딩 + viewport 설정.
 *
 *  ### 비-책임
 *  - [X] FBO 생성/소멸 - backbuffer 는 GPU 드라이버가 소유.
 *  - [X] 창 크기 변경 추적 - resize 시 Application 이 @c DefaultRenderTarget 을 재생성.
 */
#include "render/render_target.h"

namespace SJH
{
	DefaultRenderTarget::DefaultRenderTarget(int width, int height)
	    : mWidth(width), mHeight(height)
	{
	}

	void DefaultRenderTarget::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, mWidth, mHeight);
	}
} // namespace SJH
