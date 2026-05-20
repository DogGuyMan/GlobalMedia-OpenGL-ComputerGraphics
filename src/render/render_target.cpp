#include "render/render_target.h"

namespace SJH
{
    DefaultRenderTarget::DefaultRenderTarget(int width, int height)
        : mSize{width, height} {}

    void DefaultRenderTarget::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, mSize.Width, mSize.Height);
    }

    Size DefaultRenderTarget::GetSize() const { return mSize; }
}
