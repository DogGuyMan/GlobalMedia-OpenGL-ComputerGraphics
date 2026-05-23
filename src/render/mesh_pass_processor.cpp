/**
 * @file mesh_pass_processor.cpp
 * @brief Low-level Orchestrator — DrawCommand 컬렉션의 *순서 + 조건* 결정 + Applier 들에게 위임.
 *
 * @details
 *  ### 책임 (SP-PipelineSetter 후 단순화, SP-MeshPassProcessor rename)
 *  - DrawCommand 순회 + program/material 전환 시점 결정
 *  - 각 결정에서 Applier 위임:
 *    > Program 전환 -> `DeviceContext::UseProgram` + view/proj uniform 송신
 *    > Material 전환 -> `PropertyBlockSetter::Set` (PropertyBlock -> uniform/texture)
 *    > 매 cmd -> `PipelineStateSetter::Set` (Pass.PipelineState -> GL state machine)
 *
 *  ### 분리된 책임 (이전엔 본 파일 안에 있었음)
 *  - GL state machine 전환 (Stencil/Depth/Cull/Blend) -> `PipelineStateSetter`
 *  - Material properties -> uniform/texture -> `PropertyBlockSetter`
 *
 *  Process 본문 = *순서 + 조건 결정* 만 (Orchestrator 정통 — Unreal `FMeshPassProcessor`).
 */
#include "render/mesh_pass_processor.h"
#include "render/device_context.h"
#include "render/property_block_setter.h"
#include "render/pipeline_state_setter.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "object/mesh.h"
#include "material/material.h"
#include "material/pass.h"   
#include "common/constants.h"
#include <algorithm>
#include <functional>

namespace SJH
{
    namespace
    {
        /// @brief Pass.PipelineState 와 MeshRenderer override 합성 — *false 가 더 강함* (AND 합성).
        /// @details Transparent material 의 DepthWrite=false 가 *MeshRenderer 의 기본 true* 위에
        ///          자동 적용 (Pass 결정 우선). 사용자가 MeshRenderer.DepthWrite=false 로 *명시*
        ///          override 한 경우는 그대로 false 유지 (Outline 케이스).
        bool MergeBool(bool passValue, bool mrValue)
        {
            return passValue && mrValue;
        }
    }

    void MeshPassProcessor::SortMultiStage()
    {
        // Unity TransparencySortMode 정통 — Opaque 와 Transparent 의 sort 우선순위가 다르다.
        //  > Opaque (queue < 2500) : program/material 그룹핑 우선 (state change 회피) -> depth front-to-back (z-cull 효율)
        //  > Transparent (queue >= 2500) : depth back-to-front 우선 (정확성) — 그룹핑은 무시
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

    void MeshPassProcessor::Process(DeviceContext& rc,
                            const vmath::mat4& viewMat,
                            const vmath::mat4& projMat)
    {
        const Program*       lastProg = nullptr;
        const Material*      lastMat  = nullptr;
        PipelineStateSetter  stateSetter;  // ★ GL state machine 의 단일 owner — Process 내 한정.

        for (const auto& cmd : mItems)
        {
            if (!cmd.program || !cmd.mesh || !cmd.material) continue;

            // === 결정 1: Program 전환 — view/proj uniform 송신 ===
            if (cmd.program != lastProg) {
                rc.UseProgram(*cmd.program);
                // Program 이 transform schema 를 안 받으면 send skip — warn-once 노이즈 차단.
                // (postfx 처럼 NDC 기반 셰이더는 uModel/uView/uProj 가 없다.)
                if (cmd.program->GetLocation(Const::UNI_VIEW) >= 0)
                    Uniforms::SetMat4(*cmd.program, Const::UNI_VIEW, viewMat);
                if (cmd.program->GetLocation(Const::UNI_PROJ) >= 0)
                    Uniforms::SetMat4(*cmd.program, Const::UNI_PROJ, projMat);
                lastProg = cmd.program;
                lastMat  = nullptr;   // program 바뀌면 material 재바인딩 강제
            }

            // === 결정 2: Material 전환 — PropertyBlock -> uniform/texture 송신 ===
            if (cmd.material != lastMat) {
                if (const auto* prog = cmd.material->GetProgram())
                    PropertyBlockSetter::Set(rc, cmd.material->Properties, *prog);
                lastMat = cmd.material;
            }

            // === 결정 3: PipelineState 적용 (매 cmd — Applier 가 dirty check) ===
            // Material.GetPass() 가 진실의 원천 — Cocos technique / Unity SurfaceType 정통.
            // MeshRenderer override (DepthTest/DepthWrite=false) 는 *AND 합성* 으로 더 강함.
            Pass::PipelineState passState = Pass::DefaultPipelineStateOf(cmd.material->GetPass());
            passState.DepthTest  = MergeBool(passState.DepthTest,  cmd.depthTest);
            passState.DepthWrite = MergeBool(passState.DepthWrite, cmd.depthWrite);
            stateSetter.Set(passState, cmd.stencil);

            // === 결정 4: model uniform + draw ===
            if (cmd.program->GetLocation(Const::UNI_MODEL) >= 0)
                Uniforms::SetMat4(*cmd.program, Const::UNI_MODEL, cmd.modelMatrix);
            rc.BindVAO(cmd.mesh->GetVAO());
            rc.DrawIndexed(cmd.mesh->GetIndexCount());
        }

        // 다음 패스/단계가 표준 opaque 가정하도록 복원 — Applier 가 라이프사이클 책임 (2-B 채택).
        stateSetter.RestoreDefaults();
    }
}
