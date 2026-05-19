/**
 * @file framebuffer.h
 * @brief GL 프레임버퍼 객체(FBO) RAII 래퍼 — 색상 텍스처 어태치먼트 + 렌더버퍼(깊이/스텐실).
 *
 * @details
 *  ### 책임
 *  - GL FBO 생성 + 색상 어태치먼트로 텍스처(@c TexturePtr) 연결.
 *  - 깊이/스텐실 저장은 렌더버퍼 (@c GL_DEPTH24_STENCIL8) 로 처리 — 텍스처 불필요.
 *  - @c BindToDefault 로 기본 프레임버퍼(스크린) 로 복귀.
 *
 *  ### 비-책임
 *  - ❌ 색상 텍스처 *소유* — TexturePtr 는 공유 소유 (@c shared_ptr). Framebuffer 소멸 후에도 텍스처 유효.
 *  - ❌ 포스트프로세스 셰이더 구동 — Context::Render 가 담당.
 */

#ifndef __SJH_FRAMEBUFFER_H__
#define __SJH_FRAMEBUFFER_H__

#include "resource_registry/texture.h"

namespace SJH
{
    CLASS_PTR(Framebuffer);
    /**
     * @brief GL 프레임버퍼 객체(FBO) RAII 래퍼.
     * @details 색상 어태치먼트는 외부에서 생성된 @c Texture 를 공유 소유(@c shared_ptr)로 받아
     *          `GL_COLOR_ATTACHMENT0` 에 연결. 깊이/스텐실은 내부 렌더버퍼로 자동 할당.
     */
    class Framebuffer
    {
    public:
        /**
         * @brief FBO 를 생성하고 @p colorAttachment 텍스처를 색상 어태치먼트로 연결.
         * @param colorAttachment 색상 버퍼로 쓸 텍스처 (@c shared_ptr — Framebuffer 와 공유 소유).
         * @return 생성 성공 시 @c FramebufferUPtr, 실패 시 @c nullptr.
         */
        static FramebufferUPtr Create(const TexturePtr colorAttachment);

        /// @brief 기본 프레임버퍼(스크린) 로 바인딩 복귀 (@c glBindFramebuffer(GL_FRAMEBUFFER, 0)).
        static void BindToDefault();

        /// @brief FBO + 렌더버퍼 GL 자원 해제.
        ~Framebuffer();

        /// @brief GL FBO 핸들 반환.
        const uint32_t Get() const { return mFBOFramebuffer; }

        /// @brief 이 FBO 를 현재 프레임버퍼로 바인딩 (@c glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer)).
        void Bind() const;

        /// @brief 색상 어태치먼트 텍스처 반환 — 포스트프로세스 패스가 sampler 로 읽을 때 사용.
        const TexturePtr GetColorAttachment() const { return mColorAttachment; }

    private:
        Framebuffer() {}
        bool InitWithColorAttachment(const TexturePtr colorAttachment);

        uint32_t mFBOFramebuffer{0};       ///< GL FBO 핸들 — 0 은 invalid (기본 프레임버퍼).
        uint32_t mRBODepthStencilBuffer{0};///< 깊이/스텐실 렌더버퍼 핸들 (@c GL_DEPTH24_STENCIL8).
        TexturePtr mColorAttachment;    ///< 색상 어태치먼트 텍스처 공유 포인터 — 소멸 순서 주의.
    };
}
#endif // __SJH_FRAMEBUFFER_H__
