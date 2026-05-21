#ifndef __SJH_RENDER_TARGET_H__
#define __SJH_RENDER_TARGET_H__

#include "GL/gl3w.h"

namespace SJH
{
    struct Size { int Width; int Height; };

    /// @brief 그릴 대상 (default framebuffer 또는 FBO) 의 추상.
    /// @note  SP4 에서 FrameBufferTarget 파생 추가 예정 — 본 인터페이스는 안정.
    class RenderTarget
    {
    public:
        virtual ~RenderTarget() = default;
        virtual void Bind() = 0;          ///< glBindFramebuffer + glViewport
        virtual Size GetSize() const = 0;
    };

    /// @brief 화면(FBO 0) 을 가리키는 기본 타깃.
    class DefaultRenderTarget : public RenderTarget
    {
    public:
        DefaultRenderTarget(int width, int height);
        void Bind() override;
        Size GetSize() const override;
    private:
        Size mSize;
    };
}

#endif // __SJH_RENDER_TARGET_H__
