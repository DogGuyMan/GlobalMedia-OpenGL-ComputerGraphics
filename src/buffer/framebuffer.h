/**
 * @file framebuffer.h
 * @brief GL 프레임버퍼 객체(FBO) RAII 래퍼 - 색상 텍스처 어태치먼트 + 깊이/스텐실 저장소.
 *
 * @details
 *  ### 책임
 *  - @c glGenFramebuffers ~ @c glDeleteFramebuffers + RBO(@c glGenRenderbuffers) 수명 관리.
 *  - 세 가지 factory 로 FBO 초기화:
 *    - @c Create(TexturePtr)       - 외부 텍스처를 색상 어태치먼트로 공유 (기존 API 보존).
 *    - @c Create(int, int)         - 내부 RGBA8 텍스처 + RBO depth/stencil 자동 생성 (SP4 멀티패스 일반 케이스).
 *    - @c CreateWithDepthTexture   - 내부 RGBA8 + depth-stencil *텍스처* 생성 - 셰이더가 depth sampler 로 읽을 때.
 *  - @c RenderTarget 인터페이스 구현 - SP4 멀티패스에서 default backbuffer 와 동일 코드로 처리.
 *  - @c Resize - FBO 핸들 유지/color/depth 스토리지 재할당 (윈도우 크기 변경 대응).
 *  - @c BindToDefault 정적 메서드 - 기본 프레임버퍼(스크린, FBO 0) 로 복귀.
 *
 *  ### 비-책임
 *  - [X] 색상 텍스처 *단독 소유* - @c TexturePtr 는 @c shared_ptr. Framebuffer 소멸 후에도 텍스처 유효.
 *  - [X] 포스트프로세스 셰이더 구동 - @c Context::Render 가 담당.
 *  - [X] 기본 프레임버퍼(backbuffer) 관리 - @c Application 책임. @c ResourceRegistry 대상 아님.
 */

#ifndef __SJH_FRAMEBUFFER_H__
#define __SJH_FRAMEBUFFER_H__

#include "common/common.h"
#include "buffer/render_target.h"
#include "texture/texture.h"
#include <cstdint>

namespace SJH
{
    CLASS_PTR(Framebuffer);
    /**
     * @brief GL 프레임버퍼 객체(FBO) RAII 래퍼 - @c RenderTarget 인터페이스 구현.
     * @details
     *  색상 어태치먼트는 외부 @c Texture 를 공유 소유(@c shared_ptr)로 받거나 내부 생성해
     *  @c GL_COLOR_ATTACHMENT0 에 연결. 깊이/스텐실은 RBO 또는 텍스처로 attach.
     *
     *  ### factory 선택 기준
     *  | factory                   | 색상          | 깊이/스텐실    | 사용 시나리오                   |
     *  |---------------------------|---------------|----------------|---------------------------------|
     *  | @c Create(TexturePtr)     | 외부 공유 tex  | RBO (불투명)   | 기존 텍스처 재사용              |
     *  | @c Create(int,int)        | 내부 RGBA8    | RBO (불투명)   | SP4 멀티패스 일반 케이스        |
     *  | @c CreateWithDepthTexture | 내부 RGBA8    | 내부 depth tex | 셰이더가 depth sampler 로 읽을 때|
     */
    class Framebuffer : public RenderTarget
    {
    public:
        /**
         * @brief FBO 를 생성하고 @p colorAttachment 텍스처를 색상 어태치먼트로 연결.
         * @param colorAttachment 색상 버퍼로 쓸 텍스처 (@c shared_ptr - Framebuffer 와 공유 소유).
         * @return 생성 성공 시 @c FramebufferUPtr, 실패 시 @c nullptr.
         */
        static FramebufferUPtr Create(const TexturePtr colorAttachment);

        /**
         * @brief 내부 RGBA8 텍스처 + 깊이/스텐실 RBO 를 자동 생성하는 factory.
         * @details SP4 멀티패스의 일반 시나리오 - App 이 크기만 지정하면 자족(self-contained) FBO 생성.
         *          내부 텍스처는 @c Texture::Create(w, h, GL_RGBA) 로 생성되므로 RAII 보장.
         * @param width   FBO 색상 버퍼 너비 (픽셀).
         * @param height  FBO 색상 버퍼 높이 (픽셀).
         * @return 생성 성공 시 @c FramebufferUPtr, 실패 시 @c nullptr.
         */
        static FramebufferUPtr Create(int width, int height);

        /**
         * @brief 내부 RGBA8 색 텍스처 + depth-stencil *텍스처* 를 생성하는 factory.
         * @details depth 를 RBO 대신 텍스처로 attach - 셰이더가 @c sampler2D 로 .r=정규화 depth 읽기 가능.
         *          @c GL_DEPTH24_STENCIL8 사용 - depth 샘플링 + stencil(Outline) 동시 보존.
         * @param width   FBO 색상/깊이 버퍼 너비 (픽셀).
         * @param height  FBO 색상/깊이 버퍼 높이 (픽셀).
         * @return 성공 시 @c FramebufferUPtr, 실패 시 @c nullptr.
         */
        static FramebufferUPtr CreateWithDepthTexture(int width, int height);

        /// @brief 기본 프레임버퍼(스크린) 로 바인딩 복귀 (@c glBindFramebuffer(GL_FRAMEBUFFER, 0)).
        static void BindToDefault();

        /// @brief FBO + 렌더버퍼 GL 자원 해제.
        ~Framebuffer() override;

        // -- RenderTarget 인터페이스 구현 ------------------------------------------
        /// @brief 이 FBO 를 현재 프레임버퍼로 바인딩 + @c glViewport 를 color attachment 크기로 설정.
        void Bind() override;
        /// @brief color attachment 의 width/height 반환.
        virtual int GetWidth() const override;
	virtual int GetHeight() const override;


        // -- 기존 API 보존 ---------------------------------------------------------
        /// @brief GL FBO 핸들 반환.
        const uint32_t Get() const { return mFBOFramebuffer; }

        /// @brief 색상 어태치먼트 텍스처 반환 - 포스트프로세스 패스가 sampler 로 읽을 때 사용.
        const TexturePtr GetColorAttachment() const { return mColorAttachment; }

        /// @brief depth-stencil 텍스처 어태치먼트 반환 (텍스처 모드일 때만 non-null). depth-based fog 가 sampler 로 읽음.
        const TexturePtr GetDepthAttachment() const { return mDepthAttachment; }

        /**
         * @brief FBO 핸들을 유지하고 color/depth 스토리지를 새 크기로 재할당.
         * @details
         *  - color 어태치먼트(@c mColorAttachment): @c Texture::Resize 로 in-place 재할당.
         *  - depth/stencil RBO(@c mRBODepthStencilBuffer): @c glRenderbufferStorage 재호출.
         *  - depth 텍스처(@c mDepthAttachment): @c Texture::Resize 로 in-place 재할당.
         *    RBO 분기와 상호 배타 - 텍스처 모드면 @c mRBODepthStencilBuffer == 0.
         *  - FBO 핸들/어태치먼트 결합 불변(텍스처/RBO ID 동일) -> 재-attach 불필요.
         *
         * @warning @c Create(TexturePtr) 경로의 외부 공유 텍스처 Resize 는 공유 holder 에 영향.
         *          @c Create(int,int) 로 생성한 자족 FBO 에서만 안전.
         * @param width  새 너비 (픽셀).
         * @param height 새 높이 (픽셀).
         */
        void Resize(int width, int height);

    private:
        Framebuffer() = default;
        bool InitWithColorAttachment(const TexturePtr colorAttachment);
        bool InitWithSize(int width, int height);
        bool InitWithSizeAndDepthTexture(int width, int height);

        uint32_t   mFBOFramebuffer{0};       ///< GL FBO 핸들 - 0 은 invalid (기본 프레임버퍼).
        uint32_t   mRBODepthStencilBuffer{0};///< 깊이/스텐실 렌더버퍼 핸들 (@c GL_DEPTH24_STENCIL8). 텍스처 모드면 0.
        TexturePtr mColorAttachment;         ///< 색상 어태치먼트 텍스처 공유 포인터 - 소멸 순서 주의.
        TexturePtr mDepthAttachment;         ///< depth-stencil 텍스처 (텍스처 모드). RBO 모드면 nullptr.
    };
}
#endif // __SJH_FRAMEBUFFER_H__
