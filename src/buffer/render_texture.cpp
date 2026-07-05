/**
 * @file render_texture.cpp
 * @brief RenderTexture factory/Init 변형/소멸자/Bind/Resize 구현.
 *
 * @details
 *  ### 책임
 *  - 세 가지 factory (@c Create(TexturePtr) / @c Create(int,int) / @c CreateWithDepthTexture) 의
 *    힙 할당 -> 대응 @c Init* 위임 -> 실패 시 @c nullptr 반환 패턴.
 *  - @c InitWithColorAttachment: FBO gen -> color tex attach -> RBO depth/stencil gen+attach ->
 *    @c glCheckFramebufferStatus 검증.
 *  - @c InitWithSize: 내부 RGBA8 텍스처 생성 후 @c InitWithColorAttachment 위임.
 *  - @c InitWithSizeAndDepthTexture: RGBA8 color + @c GL_DEPTH24_STENCIL8 depth 텍스처 생성 ->
 *    두 어태치먼트 직접 attach -> 상태 검증.
 *  - @c Resize: color/depth 스토리지 in-place 재할당 + FBO 완결성 재검증.
 *  - 소멸자: RBO -> FBO 순으로 GL 자원 해제.
 *
 *  ### 비-책임
 *  - [X] 텍스처 소멸 - @c mColorAttachment/@c mDepthAttachment 는 @c shared_ptr, 소멸은 마지막 holder 책임.
 */
#include "render_texture.h"
#include <spdlog/spdlog.h>

namespace SJH
{

    RenderTextureUPtr RenderTexture::Create(const TexturePtr colorAttachment)
    {
        auto framebuffer = RenderTextureUPtr(new RenderTexture());
        if (!framebuffer->InitWithColorAttachment(colorAttachment))
            return nullptr;
        return framebuffer;
    }

    RenderTextureUPtr RenderTexture::Create(int width, int height)
    {
        auto framebuffer = RenderTextureUPtr(new RenderTexture());
        if (!framebuffer->InitWithSize(width, height))
            return nullptr;
        return framebuffer;
    }

    RenderTextureUPtr RenderTexture::CreateWithDepthTexture(int width, int height)
    {
        auto framebuffer = RenderTextureUPtr(new RenderTexture());
        if (!framebuffer->InitWithSizeAndDepthTexture(width, height))
            return nullptr;
        return framebuffer;
    }

    RenderTexture::~RenderTexture()
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

    void RenderTexture::BindToDefault()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void RenderTexture::Bind()
    {
        // RenderTarget contract - glBindFramebuffer + glViewport. SP4 멀티패스에서
        // DeviceContext::BeginFrame(target&) 이 default backbuffer 와 FBO 둘 다 동일 코드로 처리.
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);
        glViewport(0, 0, GetWidth(), GetHeight());
    }

    void RenderTexture::Resize(int width, int height)
    {
        // color 어태치먼트 - 같은 텍스처 핸들로 in-place 재할당 (Texture::Resize).
        if (mColorAttachment)
            mColorAttachment->Resize(width, height);

        // depth/stencil RBO - 같은 RBO 핸들로 스토리지 재할당.
        if (mRBODepthStencilBuffer)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, mRBODepthStencilBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        // depth/stencil 텍스처 모드(CreateWithDepthTexture) - 같은 텍스처 핸들로 스토리지 재할당.
        // Texture::Resize 가 저장된 format/type triple(GL_DEPTH_STENCIL/GL_UNSIGNED_INT_24_8)로 정확히 재할당.
        // RBO 분기와 상호배타 - 텍스처 모드면 mRBODepthStencilBuffer==0.
        if (mDepthAttachment)
            mDepthAttachment->Resize(width, height);

        // FBO 핸들/어태치먼트 결합 불변(텍스처/RBO ID 동일) -> 재attach 불필요. 상태만 재검증.
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);
        auto result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (result != GL_FRAMEBUFFER_COMPLETE)
            spdlog::error("RenderTexture::Resize 실패 {}x{}: {}", width, height, result);
        BindToDefault();
    }

    int RenderTexture::GetWidth() const {
	if (mColorAttachment)
		return mColorAttachment->GetWidth();
	return 0;
    }

    int RenderTexture::GetHeight() const {
	if (mColorAttachment)
		return mColorAttachment->GetHeight();
	return 0;
    }

    bool RenderTexture::InitWithColorAttachment(const TexturePtr colorAttachment)
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

    bool RenderTexture::InitWithSize(int width, int height)
    {
        // 내부 RGBA8 텍스처 생성 - Texture::Create(w,h,format) 가 TextureUPtr 반환 ->
        // shared_ptr 로 transfer (unique->shared move 변환). 이후 mColorAttachment 공유 소유.
        auto textureU = Texture::Create(width, height, GL_RGBA);
        if (!textureU)
        {
            spdlog::error("RenderTexture::Create(w,h): 내부 텍스처 생성 실패 - {}x{}", width, height);
            return false;
        }
        return InitWithColorAttachment(TexturePtr(std::move(textureU)));
    }

    bool RenderTexture::InitWithSizeAndDepthTexture(int width, int height)
    {
        // 색 RGBA8 텍스처 (기존 3-arg) - GL_COLOR_ATTACHMENT0.
        auto colorU = Texture::Create(width, height, GL_RGBA);
        if (!colorU)
        {
            spdlog::error("RenderTexture::CreateWithDepthTexture: color 텍스처 생성 실패 - {}x{}", width, height);
            return false;
        }
        mColorAttachment = TexturePtr(std::move(colorU));

        // depth-stencil 텍스처 (5-arg) - sampler2D 로 .r=depth, stencil 보존.
        auto depthU = Texture::Create(width, height,
                                      GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
        if (!depthU)
        {
            spdlog::error("RenderTexture::CreateWithDepthTexture: depth 텍스처 생성 실패 - {}x{}", width, height);
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
            spdlog::error("RenderTexture::CreateWithDepthTexture: incomplete - {}", status);
            return false;
        }

        BindToDefault();
        return true;
    }
}
