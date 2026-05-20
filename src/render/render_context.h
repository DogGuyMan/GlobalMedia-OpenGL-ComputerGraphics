#ifndef __SJH_RENDER_CONTEXT_H__
#define __SJH_RENDER_CONTEXT_H__

#include "render/render_target.h"
#include "GL/gl3w.h"
#include <memory>

namespace SJH
{
    class Program; // forward — 본 헤더는 Program 정의 의존 X

    /// @brief GL 호출의 단일 게이트웨이 + 현재 bound 상태 추적 (싱글톤).
    /// @details
    ///   책임 5 가지:
    ///   -# 현재 bound program 추적 (@c mBoundProgram)
    ///   -# GL 상태 변경 단일 진입점 (glUseProgram / glBindVertexArray / glBindTexture / glClear / glEnable)
    ///   -# Draw 명령 발행 (glDrawElements / glDrawArrays)
    ///   -# RenderTarget 바인딩 위임 (@c BindTarget / @c BeginFrame)
    ///   -# SP4 멀티패스 진입 지점 (@c BeginFrame 이 패스마다 다른 target 받음)
    ///
    ///   ### 순서 계약 (POLA 준수: 코드 강제 없음, 문서 명시)
    ///   - @c UseProgram(prog) 호출 후에만 그 prog 에 @c Uniforms::Set* 호출 가능.
    ///   - 디버그 빌드의 assertion 등은 *추가하지 않음* — hidden astonishment 회피.
    class RenderContext
    {
    public:
        /// @brief 싱글톤 접근. GL context 가 활성 상태일 때만 호출 유효.
        static RenderContext& Get();

        // --- bound 상태 변경 (primitive — Explicit Side Effects 준수) ---

        void UseProgram(const Program& prog);
        void BindVAO(GLuint vao);
        void BindTexture(GLuint unit, GLuint tex);
        void BindTarget(RenderTarget& target);
        void Clear(GLbitfield mask);
        void SetDepthTest(bool enabled, GLenum func = GL_LESS);
        void SetBlend(bool enabled, GLenum srcFactor = GL_SRC_ALPHA,
                                    GLenum dstFactor = GL_ONE_MINUS_SRC_ALPHA);

        // --- Draw 명령 ---

        void DrawIndexed(GLsizei count);
        void DrawArrays(GLenum mode, GLsizei count);

        // --- 편의 wrapper ---

        /// @brief BindTarget + Clear(color|depth) + SetDepthTest(true) + SetBlend(true) 의 alias.
        /// @note  Stencil pass 등 커스텀 상태가 필요하면 primitive 메서드를 *직접* 호출.
        void BeginFrame(RenderTarget& target);

        /// @brief 화면 기본 타깃 (lazy 생성, 윈도우 크기는 SetDefaultTargetSize 로 갱신).
        DefaultRenderTarget& GetDefaultTarget();

        /// @brief 윈도우 리사이즈 시 호출 — DefaultRenderTarget 크기 갱신.
        void SetDefaultTargetSize(int width, int height);

        // 복사·이동 차단 (싱글톤)
        RenderContext(const RenderContext&)            = delete;
        RenderContext& operator=(const RenderContext&) = delete;
        RenderContext(RenderContext&&)                 = delete;
        RenderContext& operator=(RenderContext&&)      = delete;

    private:
        RenderContext()  = default;
        ~RenderContext() = default;

        const Program*                      mBoundProgram = nullptr;
        std::unique_ptr<DefaultRenderTarget> mDefaultTarget;
    };
}

#endif // __SJH_RENDER_CONTEXT_H__
