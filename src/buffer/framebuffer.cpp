#include "framebuffer.h"
#include <spdlog/spdlog.h>

namespace SJH
{

    FramebufferUPtr Framebuffer::Create(const TexturePtr colorAttachment)
    {
        auto framebuffer = FramebufferUPtr(new Framebuffer());
        if (!framebuffer->InitWithColorAttachment(colorAttachment))
            return nullptr;
        return std::move(framebuffer);
    }

    Framebuffer::~Framebuffer()
    {
        if (mRBODepthStencilBuffer)
        {
            glDeleteRenderbuffers(1, &mRBODepthStencilBuffer);
        }
        if (mFBOFramebuffer)
        {
            glDeleteFramebuffers(1, &mFBOFramebuffer);
        }
    }

    void Framebuffer::BindToDefault()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);
    }

    bool Framebuffer::InitWithColorAttachment(const TexturePtr colorAttachment)
    {
        mColorAttachment = colorAttachment;
        glGenFramebuffers(1, &mFBOFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);

        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               colorAttachment->GetTextureID(), 0);

        glGenRenderbuffers(1, &mRBODepthStencilBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, mRBODepthStencilBuffer);
        glRenderbufferStorage(
            GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
            colorAttachment->GetWidth(), colorAttachment->GetHeight());
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, mRBODepthStencilBuffer);

        auto result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (result != GL_FRAMEBUFFER_COMPLETE)
        {
            spdlog::error("failed to create framebuffer: {}", result);
            return false;
        }

        BindToDefault();

        return true;
    }
}
