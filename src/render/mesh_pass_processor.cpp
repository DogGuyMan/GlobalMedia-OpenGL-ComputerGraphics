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
    // SP-MaterialSSoT — MergeBool 헬퍼 제거. Material 이 진실의 원천 (override 합성 없음).

    void MeshPassProcessor::SortMultiStage()
    {
        // ─── 정렬 정책 (Unity TransparencySortMode 정통) ────────────────────────
        // Opaque (queue < 2500): "같은 셰이더/머티리얼끼리 모아 그려서 GL state 전환 최소화"
        //   -> program 그룹핑 -> material 그룹핑 -> 같은 그룹 안에서 *가까운 거 먼저*
        //     (z-cull 효율: 가까운 면이 depth buffer 채워 뒤 fragment 자동 skip)
        // Transparent (queue >= 2500): "뒤에서 앞으로 그려야 알파 합성이 정확"
        //   -> depth 만 본다 (그룹핑 무시) — *먼 거 먼저* 그린 위에 알파 블렌딩
        //
        // ─── view-space z 부호 함정 ──────────────────────────────────────────
        // OpenGL 카메라는 -Z 방향을 본다 -> *카메라 앞 = 음수 z*. 멀수록 *더 음수*.
        //   가까운 객체: z = -10  <- 덜 음수 (큰 값)
        //   먼 객체:    z = -100 <- 더 음수 (작은 값)
        // 그래서 부호가 *방향 정반대*:
        //   front-to-back = a.depth > b.depth  (큰 값 = 덜 음수 = 가까움)
        //   back-to-front = a.depth < b.depth  (작은 값 = 더 음수 = 멈)
        //
        // ─── std::less<> 의 이유 ─────────────────────────────────────────────
        // raw pointer 의 `<` 는 *서로 다른 객체끼리 UB* (C++ [expr.rel]).
        // std::less<> 는 *정의된 strict total order* 보장 — 안전한 그룹핑.
        //
        // ─── 왜 std::stable_sort 인가? — 3 가치 ──────────────────────────────
        // 1. **z-fighting 깜빡임 차단** — 같은 depth 두 면이 *매 프레임 같은 순서* 로
        //    그려져 flicker 회피 (std::sort 면 quicksort pivot 마다 순서 변동 -> 깜빡임)
        // 2. **Actor 트리 DFS 순서 보존** — Submit 순서 = 씬 계층 구조 ->
        //    시각 디버깅 시 *언제나 같은 순서* 로 그려져 회귀 추적 쉬움
        // 3. **시각 회귀 테스트 결정성** — 동등 cmd 의 정렬 결과가 *매 호출 동일* ->
        //    골든 이미지 비교 가능 (비결정적 정렬은 false-positive flicker 유발)
        std::stable_sort(mItems.begin(), mItems.end(),
            [](const DrawCommand& a, const DrawCommand& b) {
                if (a.queueLayer != b.queueLayer) return a.queueLayer < b.queueLayer;

                if (Pass::IsTransparentQueue(a.queueLayer))
                    return a.depth < b.depth;   // back-to-front

                if (a.program  != b.program)  return std::less<const Program*>{}(a.program, b.program);
                if (a.material != b.material) return std::less<const Material*>{}(a.material, b.material);
                return a.depth > b.depth;       // front-to-back 
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

            // 결정 1: Program 전환 — view/proj uniform 송신
            if (cmd.program != lastProg) {
                rc.UseProgram(*cmd.program);
                if (cmd.program->GetLocation(Const::UNI_VIEW) >= 0)
                    Uniforms::SetMat4(*cmd.program, Const::UNI_VIEW, viewMat);
                if (cmd.program->GetLocation(Const::UNI_PROJ) >= 0)
                    Uniforms::SetMat4(*cmd.program, Const::UNI_PROJ, projMat);
                lastProg = cmd.program;
                lastMat  = nullptr;   // program 바뀌면 material 재바인딩 강제
            }

            // 결정 2: Material 전환 — PropertyBlock -> uniform/texture 송신
            if (cmd.material != lastMat) {
                if (const auto* prog = cmd.material->GetProgram())
                    PropertyBlockSetter::Set(rc, cmd.material->Properties, *prog);
                lastMat = cmd.material;
            }

            // 결정 3: PipelineState 적용 
            //   override 합성 없음 — 변형은 Material::Clone() + 별도 인스턴스 사용 (Unreal MID 정통).
            const Pass::PipelineState passState = Pass::DefaultPipelineStateOf(cmd.material->GetPass());
            stateSetter.Set(passState);

            // 결정 4: model uniform + draw
            if (cmd.program->GetLocation(Const::UNI_MODEL) >= 0)
                Uniforms::SetMat4(*cmd.program, Const::UNI_MODEL, cmd.modelMatrix);
            rc.BindVAO(cmd.mesh->GetVAO());
            rc.DrawIndexed(cmd.mesh->GetIndexCount());
        }

        // 다음 패스/단계가 표준 opaque 가정하도록 복원 — Applier 가 라이프사이클 책임 (2-B 채택).
        stateSetter.RestoreDefaults();
    }
}
