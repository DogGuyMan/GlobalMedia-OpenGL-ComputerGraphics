#include "render/render_queue.h"
#include "render/render_context.h"
#include "render/material_applier.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "object/mesh.h"
#include "material/material.h"
#include "material/pass.h"   // Pass::DefaultStateOf / IsTransparentQueue — Material 의도 기반 자동 state.
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
            bool   hasDepthFunc = false;   // 첫 cmd 까지 미초기화.
            bool   depthWrite = true;

            // Blend (Pass 기반 자동 전환 — SP-Pass.3).
            bool   hasBlend  = false;   // 첫 cmd 까지 미초기화 — 무조건 적용.
            bool   blendOn   = false;
            GLenum blendSrc  = GL_SRC_ALPHA;
            GLenum blendDst  = GL_ONE_MINUS_SRC_ALPHA;

            // Cull (Pass 기반 자동 전환 — SP-Pass.13). CullMode == 0 => cull off.
            bool   hasCull   = false;   // 첫 cmd 까지 미초기화.
            bool   cullOn    = true;
            GLenum cullMode  = GL_BACK;
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

        void ApplyDepth(bool test, bool write, GLenum func, LastState& last)
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
            // DepthFunc — Skybox 의 GL_LEQUAL / 기본 GL_LESS 등 자동 전환 (SP-Pass.13).
            if (!last.hasDepthFunc || func != last.depthFunc)
            {
                glDepthFunc(func);
                last.depthFunc = func;
                last.hasDepthFunc = true;
            }
        }

        /// @brief Pass 기반 cull state 자동 전환 — Skybox 의 GL_FRONT / 기본 GL_BACK.
        /// @details `cullMode == 0` 이면 face culling disable (CullMode 의 sentinel).
        void ApplyCull(GLenum cullMode, LastState& last)
        {
            const bool wantOn = (cullMode != 0);
            if (!last.hasCull || wantOn != last.cullOn)
            {
                if (wantOn) glEnable(GL_CULL_FACE);
                else        glDisable(GL_CULL_FACE);
                last.cullOn = wantOn;
            }
            if (wantOn && (!last.hasCull || cullMode != last.cullMode))
            {
                glCullFace(cullMode);
                last.cullMode = cullMode;
            }
            last.hasCull = true;
        }

        /// @brief Pass 기반 blend state 자동 전환 — Material.PassKind 가 진실의 원천.
        /// @details Cocos technique / Unity SurfaceType 정통 — Transparent 면 blend on,
        ///          Opaque/AlphaTest 면 blend off. RenderContext.SetBlend 가 redundant 호출 회피.
        void ApplyBlend(const Pass::State& s, LastState& last)
        {
            if (last.hasBlend && last.blendOn == s.BlendEnable &&
                last.blendSrc == s.BlendSrc && last.blendDst == s.BlendDst)
                return;
            if (s.BlendEnable)
            {
                glEnable(GL_BLEND);
                glBlendFunc(s.BlendSrc, s.BlendDst);
            }
            else
            {
                glDisable(GL_BLEND);
            }
            last.hasBlend = true;
            last.blendOn  = s.BlendEnable;
            last.blendSrc = s.BlendSrc;
            last.blendDst = s.BlendDst;
        }

        /// @brief Pass.State 와 MeshRenderer override 합성 — *false 가 더 강함* (AND 합성).
        /// @details Transparent material 의 DepthWrite=false 가 *MeshRenderer 의 기본 true* 위에
        ///          자동 적용 (Pass 결정 우선). 사용자가 MeshRenderer.DepthWrite=false 로 *명시*
        ///          override 한 경우는 그대로 false 유지 (Outline 케이스).
        bool MergeBool(bool passValue, bool mrValue)
        {
            return passValue && mrValue;
        }
    }

    void RenderQueue::SortMultiStage()
    {
        // Unity TransparencySortMode 정통 — Opaque 와 Transparent 의 sort 우선순위가 다르다.
        //  · Opaque (queue < 2500) : program/material 그룹핑 우선 (state change 회피) → depth front-to-back (z-cull 효율)
        //  · Transparent (queue >= 2500) : depth back-to-front 우선 (정확성) — 그룹핑은 무시
        // view-space z 는 카메라 forward 가 -Z 이므로 *카메라 앞 = 음수*. 멀수록 더 작은 음수.
        //   front-to-back = a.depth > b.depth (큰 값 = 가까운 음수 = 가까움)
        //   back-to-front = a.depth < b.depth (작은 값 = 큰 음수 = 멈)
        // 포인터 비교는 std::less<> 로 — raw pointer < 는 서로 다른 객체 간 UB (C++ [expr.rel]).
        std::stable_sort(mItems.begin(), mItems.end(),
            [](const DrawCommand& a, const DrawCommand& b) {
                if (a.queueLayer != b.queueLayer) return a.queueLayer < b.queueLayer;

                // 같은 layer — Transparent 면 depth 우선, Opaque 면 program/material 우선.
                if (Pass::IsTransparentQueue(a.queueLayer))
                    return a.depth < b.depth;   // back-to-front

                if (a.program  != b.program)  return std::less<const Program*>{}(a.program, b.program);
                if (a.material != b.material) return std::less<const Material*>{}(a.material, b.material);
                return a.depth > b.depth;       // front-to-back (z-cull 효율)
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

            // === Pass 기반 자동 state (SP-Pass.3) ===
            // Material.GetPass() 가 진실의 원천 — Cocos technique / Unity SurfaceType 정통.
            // MeshRenderer override (DepthTest/DepthWrite=false) 는 *AND 합성* 으로 더 강함.
            const auto passState = cmd.material
                ? Pass::DefaultStateOf(cmd.material->GetPass())
                : Pass::State{};

            ApplyStencil(cmd.stencil, last);
            ApplyDepth(MergeBool(passState.DepthTest,  cmd.depthTest),
                       MergeBool(passState.DepthWrite, cmd.depthWrite),
                       passState.DepthFunc, last);
            ApplyCull (passState.CullMode, last);
            ApplyBlend(passState, last);

            Uniforms::SetMat4(*cmd.program, "uModel", cmd.modelMatrix);
            rc.BindVAO(cmd.mesh->GetVAO());
            rc.DrawIndexed(cmd.mesh->GetIndexCount());
        }

        // 다음 패스/단계가 표준 opaque 가정하도록 복원
        // (stencil off / depth on+write on+func LESS / cull back / blend off).
        if (last.stencilOn) glDisable(GL_STENCIL_TEST);
        glStencilMask(0xFFu);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);    // SP-Pass.13 — Skybox 가 LEQUAL 로 바꾼 상태 복원.
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);     // SP-Pass.13 — Skybox 가 FRONT 로 바꾼 상태 복원.
        glDisable(GL_BLEND);     // SP-Pass.3 — Pass 가 blend on 한 상태 복원.
    }
}
