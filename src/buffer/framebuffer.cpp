#include "framebuffer.h"
#include <spdlog/spdlog.h>

namespace SJH
{

    FramebufferUPtr Framebuffer::Create(const TexturePtr colorAttachment)
    {
        auto framebuffer = FramebufferUPtr(new Framebuffer());
        if (!framebuffer->InitWithColorAttachment(colorAttachment))
            return nullptr;
        return framebuffer;
    }

    FramebufferUPtr Framebuffer::Create(int width, int height)
    {
        auto framebuffer = FramebufferUPtr(new Framebuffer());
        if (!framebuffer->InitWithSize(width, height))
            return nullptr;
        return framebuffer;
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

    void Framebuffer::Bind()
    {
        // RenderTarget contract — glBindFramebuffer + glViewport. SP4 멀티패스에서
        // RenderContext::BeginFrame(target&) 이 default backbuffer 와 FBO 둘 다 동일 코드로 처리.
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);
        const auto size = GetSize();
        glViewport(0, 0, size.Width, size.Height);
    }

    Size Framebuffer::GetSize() const
    {
        if (mColorAttachment)
            return Size{ mColorAttachment->GetWidth(), mColorAttachment->GetHeight() };
        return Size{ 0, 0 };
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

    bool Framebuffer::InitWithSize(int width, int height)
    {
        // 내부 RGBA8 텍스처 생성 — Texture::Create(w,h,format) 가 TextureUPtr 반환 →
        // shared_ptr 로 transfer (unique→shared move 변환). 이후 mColorAttachment 공유 소유.
        auto textureU = Texture::Create(width, height, GL_RGBA);
        if (!textureU)
        {
            spdlog::error("Framebuffer::Create(w,h): 내부 텍스처 생성 실패 — {}x{}", width, height);
            return false;
        }
        return InitWithColorAttachment(TexturePtr(std::move(textureU)));
    }
}
