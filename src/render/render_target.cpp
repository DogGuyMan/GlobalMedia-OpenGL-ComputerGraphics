
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
