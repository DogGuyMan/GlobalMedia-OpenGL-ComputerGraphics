#include "render/render_queue.h"
#include "render/render_context.h"
#include "render/material_applier.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "object/mesh.h"
#include "material/material.h"
#include "GL/gl3w.h"   // gl 직접 호출 — per-command stencil/depth state
#include <algorithm>
#include <functional>

namespace SJH
{
    namespace
    {
        /// @brief 마지막 적용된 GL 상태 캐시 — 동일 상태 연속 적용 시 GL 호출 생략.
        ///        Flush 한 번의 라이프타임 한정 (정적 스코프 reset 은 매 Flush 시작에).
        struct LastState
        {
            bool  hasStencil = false;
            bool  stencilOn  = false;
            GLenum sFunc     = GL_ALWAYS;
            GLint  sRef      = 0;
            GLuint sTestMask = 0xFFu;
            GLenum sSFail    = GL_KEEP;
            GLenum sDpFail   = GL_KEEP;
            GLenum sDpPass   = GL_KEEP;
            GLuint sWriteMsk = 0xFFu;

            bool   depthOn    = true;
            GLenum depthFunc  = GL_LESS;
            bool   depthWrite = true;
        };

        void ApplyStencil(const Scene::StencilState& s, LastState& last)
        {
            if (s.Enabled)
            {
                if (!last.hasStencil || !last.stencilOn)
                {
                    glEnable(GL_STENCIL_TEST);
                    last.stencilOn = true;
                }
                if (!last.hasStencil || last.sFunc != s.Func ||
                    last.sRef != s.Ref || last.sTestMask != s.TestMask)
                {
                    glStencilFunc(s.Func, s.Ref, s.TestMask);
                    last.sFunc = s.Func; last.sRef = s.Ref; last.sTestMask = s.TestMask;
                }
                if (!last.hasStencil || last.sSFail != s.SFail ||
                    last.sDpFail != s.DpFail || last.sDpPass != s.DpPass)
                {
                    glStencilOp(s.SFail, s.DpFail, s.DpPass);
                    last.sSFail = s.SFail; last.sDpFail = s.DpFail; last.sDpPass = s.DpPass;
                }
                if (!last.hasStencil || last.sWriteMsk != s.WriteMask)
                {
                    glStencilMask(s.WriteMask);
                    last.sWriteMsk = s.WriteMask;
                }
            }
            else
            {
                if (!last.hasStencil || last.stencilOn)
                {
                    glDisable(GL_STENCIL_TEST);
                    last.stencilOn = false;
                }
            }
            last.hasStencil = true;
        }

        void ApplyDepth(bool test, bool write, LastState& last)
        {
            if (test != last.depthOn)
            {
                if (test) glEnable(GL_DEPTH_TEST);
                else      glDisable(GL_DEPTH_TEST);
                last.depthOn = test;
            }
            if (write != last.depthWrite)
            {
                glDepthMask(write ? GL_TRUE : GL_FALSE);
                last.depthWrite = write;
            }
        }
    }

    void RenderQueue::SortMultiStage()
    {
        // 포인터 비교는 std::less<> 로 — raw pointer < 는 서로 다른 객체 간 UB (C++ [expr.rel]).
        // std::less<> 는 모든 포인터에 total order 를 보장.
        std::stable_sort(mItems.begin(), mItems.end(),
            [](const DrawCommand& a, const DrawCommand& b) {
                if (a.queueLayer != b.queueLayer) return a.queueLayer < b.queueLayer;
                if (a.program    != b.program)    return std::less<const Program*>{}(a.program, b.program);
                if (a.material   != b.material)   return std::less<const Material*>{}(a.material, b.material);
                return a.depth > b.depth;   // back-to-front (큰 depth 가 먼저)
            });
    }

    void RenderQueue::Flush(RenderContext& rc,
                            const vmath::mat4& viewMat,
                            const vmath::mat4& projMat)
    {
        const Program*  lastProg = nullptr;
        const Material* lastMat  = nullptr;
        LastState last; // Flush 진입 시 reset — 캐시는 한 패스 내 한정.

        for (const auto& cmd : mItems)
        {
            if (!cmd.program || !cmd.mesh || !cmd.material) continue;

            if (cmd.program != lastProg) {
                rc.UseProgram(*cmd.program);
                Uniforms::SetMat4(*cmd.program, "uView", viewMat);
                Uniforms::SetMat4(*cmd.program, "uProj", projMat);
                lastProg = cmd.program;
                lastMat  = nullptr;   // program 바뀌면 material 재바인딩 강제
            }
            if (cmd.material != lastMat) {
                // SP6 — properties bag 통합 Apply (UniformCache 교집합 + 텍스처 바인딩 일괄).
                MaterialApplier::Apply(rc, *cmd.material);
                lastMat = cmd.material;
            }

            // per-actor 상태 — stencil + depth (last cache 로 redundant 호출 회피).
            ApplyStencil(cmd.stencil, last);
            ApplyDepth(cmd.depthTest, cmd.depthWrite, last);

            Uniforms::SetMat4(*cmd.program, "uModel", cmd.modelMatrix);
            rc.BindVAO(cmd.mesh->GetVAO());
            rc.DrawIndexed(cmd.mesh->GetIndexCount());
        }

        // 다음 패스/단계가 표준 opaque 가정하도록 복원 (stencil 끄기, depth on, write on).
        if (last.stencilOn) glDisable(GL_STENCIL_TEST);
        glStencilMask(0xFFu);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
}
