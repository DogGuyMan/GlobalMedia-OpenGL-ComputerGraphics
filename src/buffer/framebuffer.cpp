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

    FramebufferUPtr Framebuffer::CreateWithDepthTexture(int width, int height)
    {
        auto framebuffer = FramebufferUPtr(new Framebuffer());
        if (!framebuffer->InitWithSizeAndDepthTexture(width, height))
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
        // DeviceContext::BeginFrame(target&) 이 default backbuffer 와 FBO 둘 다 동일 코드로 처리.
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);
        glViewport(0, 0, GetWidth(), GetHeight());
    }

    void Framebuffer::Resize(int width, int height)
    {
        // color 어태치먼트 — 같은 텍스처 핸들로 in-place 재할당 (Texture::Resize).
        if (mColorAttachment)
            mColorAttachment->Resize(width, height);

        // depth/stencil RBO — 같은 RBO 핸들로 스토리지 재할당.
        if (mRBODepthStencilBuffer)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, mRBODepthStencilBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        // depth/stencil 텍스처 모드(CreateWithDepthTexture) — 같은 텍스처 핸들로 스토리지 재할당.
        // Texture::Resize 가 저장된 format/type triple(GL_DEPTH_STENCIL/GL_UNSIGNED_INT_24_8)로 정확히 재할당.
        // RBO 분기와 상호배타 — 텍스처 모드면 mRBODepthStencilBuffer==0.
        if (mDepthAttachment)
            mDepthAttachment->Resize(width, height);

        // FBO 핸들·어태치먼트 결합 불변(텍스처/RBO ID 동일) -> 재attach 불필요. 상태만 재검증.
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);
        auto result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (result != GL_FRAMEBUFFER_COMPLETE)
            spdlog::error("Framebuffer::Resize 실패 {}x{}: {}", width, height, result);
        BindToDefault();
    }

    int Framebuffer::GetWidth() const {
	if (mColorAttachment)
		return mColorAttachment->GetWidth();
	return 0;
    }

    int Framebuffer::GetHeight() const {
	if (mColorAttachment)
		return mColorAttachment->GetHeight();
	return 0;
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
        // 내부 RGBA8 텍스처 생성 — Texture::Create(w,h,format) 가 TextureUPtr 반환 ->
        // shared_ptr 로 transfer (unique->shared move 변환). 이후 mColorAttachment 공유 소유.
        auto textureU = Texture::Create(width, height, GL_RGBA);
        if (!textureU)
        {
            spdlog::error("Framebuffer::Create(w,h): 내부 텍스처 생성 실패 — {}x{}", width, height);
            return false;
        }
        return InitWithColorAttachment(TexturePtr(std::move(textureU)));
    }

    bool Framebuffer::InitWithSizeAndDepthTexture(int width, int height)
    {
        // 색 RGBA8 텍스처 (기존 3-arg) — GL_COLOR_ATTACHMENT0.
        auto colorU = Texture::Create(width, height, GL_RGBA);
        if (!colorU)
        {
            spdlog::error("Framebuffer::CreateWithDepthTexture: color 텍스처 생성 실패 — {}x{}", width, height);
            return false;
        }
        mColorAttachment = TexturePtr(std::move(colorU));

        // depth-stencil 텍스처 (5-arg) — sampler2D 로 .r=depth, stencil 보존.
        auto depthU = Texture::Create(width, height,
                                      GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
        if (!depthU)
        {
            spdlog::error("Framebuffer::CreateWithDepthTexture: depth 텍스처 생성 실패 — {}x{}", width, height);
            return false;
        }
        mDepthAttachment = TexturePtr(std::move(depthU));

        glGenFramebuffers(1, &mFBOFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mColorAttachment->GetTextureID(), 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               mDepthAttachment->GetTextureID(), 0);

        auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            spdlog::error("Framebuffer::CreateWithDepthTexture: incomplete — {}", status);
            return false;
        }

        BindToDefault();
        return true;
    }
}
