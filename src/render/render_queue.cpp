#include "render/render_queue.h"
#include "render/render_context.h"
#include "render/material_applier.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "object/mesh.h"
#include "material/material.h"
#include <algorithm>
#include <functional>

namespace SJH
{
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
                MaterialApplier::WriteUniforms(*cmd.program, *cmd.material);
                MaterialApplier::BindTextures(rc, *cmd.material);
                lastMat = cmd.material;
            }
            Uniforms::SetMat4(*cmd.program, "uModel", cmd.modelMatrix);
            rc.BindVAO(cmd.mesh->GetVAO());
            rc.DrawIndexed(cmd.mesh->GetIndexCount());
        }
    }
}
