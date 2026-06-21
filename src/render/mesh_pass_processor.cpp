/**
 * @file mesh_pass_processor.cpp
 * @brief MeshPassProcessor 구현 - SortMultiStage 정렬 정책 + Process Orchestrator 발행 흐름.
 *
 * @details
 *  ### 책임 (SP-PipelineSetter 후 단순화, SP-MeshPassProcessor rename)
 *  - DrawCommand 순회 + program/material 전환 시점 결정.
 *  - 각 결정에서 Applier 위임:
 *    - Program 전환 -> @c DeviceContext::UseProgram + view/proj uniform 송신.
 *    - Material 전환 -> @c PropertyBlockSetter::Set (PropertyBlock -> uniform/texture).
 *    - 매 cmd -> @c DeviceContext::ApplyPipelineState (Pass.PipelineState -> GL state machine, D-RS-1).
 *
 *  ### 분리된 책임 (이전엔 본 파일 안에 있었음)
 *  - GL state machine 전환 (Stencil/Depth/Cull/Blend) -> @c DeviceContext::ApplyPipelineState (D-RS-1 흡수).
 *  - Material properties -> uniform/texture -> @c PropertyBlockSetter.
 *
 *  @c Process 본문 = 순서 + 조건 결정만 (Orchestrator 정통 - Unreal @c FMeshPassProcessor).
 *
 *  ### SortMultiStage 정렬 정책 요약
 *  - Opaque (queue < 2500): program 그룹 -> material 그룹 -> front-to-back (z-cull 효율).
 *  - Transparent (queue >= 2500): back-to-front (알파 합성 정확도).
 *  - @c std::stable_sort: z-fighting 깜빡임 차단 + Actor DFS 순서 보존 + 골든 이미지 결정성.
 *
 *  ### 비-책임
 *  - [X] GL state machine *캐싱/적용 로직* 소유 -> @c DeviceContext::ApplyPipelineState 위임 (호출 시점만 결정).
 *  - [X] uniform/texture 직접 송신 -> @c PropertyBlockSetter 위임.
 */
#include "render/mesh_pass_processor.h"
#include <glm/glm.hpp>
#include "render/device_context.h"
#include "render/mesh_renderer.h"     // DrawCommand 의 meshRenderer 경유 접근 (SSoT).
#include "render/property_block_setter.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "object/mesh.h"
#include "material/material.h"
#include "material/pass.h"
#include "common/constants.h"
#include "buffer/framebuffer.h"
#include <algorithm>
#include <functional>

namespace SJH
{
    // SP-MaterialSSoT - MergeBool 헬퍼 제거. Material 이 진실의 원천 (override 합성 없음).

    void MeshPassProcessor::SortMultiStage()
    {
        // --- 정렬 정책 (Unity TransparencySortMode 정통) ------------------------
        // Opaque (queue < 2500): "같은 셰이더/머티리얼끼리 모아 그려서 GL state 전환 최소화"
        //   -> program 그룹핑 -> material 그룹핑 -> 같은 그룹 안에서 *가까운 거 먼저*
        //     (z-cull 효율: 가까운 면이 depth buffer 채워 뒤 fragment 자동 skip)
        // Transparent (queue >= 2500): "뒤에서 앞으로 그려야 알파 합성이 정확"
        //   -> depth 만 본다 (그룹핑 무시) - *먼 거 먼저* 그린 위에 알파 블렌딩
        //
        // --- view-space z 부호 함정 ------------------------------------------
        // OpenGL 카메라는 -Z 방향을 본다 -> *카메라 앞 = 음수 z*. 멀수록 *더 음수*.
        //   가까운 객체: z = -10  <- 덜 음수 (큰 값)
        //   먼 객체:    z = -100 <- 더 음수 (작은 값)
        // 그래서 부호가 *방향 정반대*:
        //   front-to-back = a.depth > b.depth  (큰 값 = 덜 음수 = 가까움)
        //   back-to-front = a.depth < b.depth  (작은 값 = 더 음수 = 멈)
        //
        // --- std::less<> 의 이유 ---------------------------------------------
        // raw pointer 의 `<` 는 *서로 다른 객체끼리 UB* (C++ [expr.rel]).
        // std::less<> 는 *정의된 strict total order* 보장 - 안전한 그룹핑.
        //
        // --- 왜 std::stable_sort 인가? - 3 가치 ------------------------------
        // 1. **z-fighting 깜빡임 차단** - 같은 depth 두 면이 *매 프레임 같은 순서* 로
        //    그려져 flicker 회피 (std::sort 면 quicksort pivot 마다 순서 변동 -> 깜빡임)
        // 2. **Actor 트리 DFS 순서 보존** - Submit 순서 = 씬 계층 구조 ->
        //    시각 디버깅 시 *언제나 같은 순서* 로 그려져 회귀 추적 쉬움
        // 3. **시각 회귀 테스트 결정성** - 동등 cmd 의 정렬 결과가 *매 호출 동일* ->
        //    골든 이미지 비교 가능 (비결정적 정렬은 false-positive flicker 유발)
        std::stable_sort(mItems.begin(), mItems.end(),
            [](const DrawCommand& a, const DrawCommand& b) {
                if (a.queueLayer != b.queueLayer) return a.queueLayer < b.queueLayer;

                if (Pass::IsTransparentQueue(a.queueLayer))
                    return a.depth < b.depth;   // back-to-front

                // SSoT - meshRenderer 경유 program/material 추출 (DrawCommand 직접 필드 폐기).
                const Material* aMat = a.meshRenderer ? a.meshRenderer->Material : nullptr;
                const Material* bMat = b.meshRenderer ? b.meshRenderer->Material : nullptr;
                const Program*  aProg = aMat ? aMat->GetProgram() : nullptr;
                const Program*  bProg = bMat ? bMat->GetProgram() : nullptr;

                if (aProg != bProg) return std::less<const Program*>{}(aProg, bProg);
                if (aMat  != bMat)  return std::less<const Material*>{}(aMat, bMat);
                return a.depth > b.depth;       // front-to-back
            });
    }

    void MeshPassProcessor::Process(DeviceContext& rc,
                            const glm::mat4& viewMat,
                            const glm::mat4& projMat)
    {
        const Program*       lastProg = nullptr;
        const Material*      lastMat  = nullptr;

        // Process 진입 - GL state 캐시 무효화 (per-Process 캐시 불변식 보존, D-RS-2).
        //   직전 stage(다른 카메라 Process / Effekseer ParticleStage 등)가 GL state 를 캐시 뒤에서
        //   바꿨을 수 있으므로, 첫 ApplyPipelineState 가 first-call 처럼 전체 강제 적용하도록 한다.
        rc.InvalidateStateCache();

        for (const auto& cmd : mItems)
        {
            // -- ScreenQuad (PassComponent) ----------------------------------------
            if (cmd.kind == DrawCommand::Kind::ScreenQuad)
            {
                if (!cmd.inputFB || !cmd.outputFB || !mScreenQuadMesh)
                    continue;
                // passMaterial == nullptr ->disabled 패스 bypass: passthrough blit
                Material *effectiveMat = cmd.passMaterial ? cmd.passMaterial : mBypassMat;
                if (!effectiveMat)
                    continue;
                auto *prog = effectiveMat->GetProgram();
                if (!prog)
                    continue;

                rc.BeginFrame(*cmd.outputFB);
                // ScreenQuad blit state-as-data (D-RS-5) - depth test/write off, cull off, blend off(replace).
                rc.ApplyPipelineState(Pass::DefaultPipelineStateOf(Pass::Kind::Screen));

                effectiveMat->Properties.Textures["uScene"] = {
                    cmd.inputFB->GetColorAttachment().get(), 0};

                rc.UseProgram(*prog);
                PropertyBlockSetter::Set(rc, effectiveMat->Properties, *prog);

                rc.BindVAO(mScreenQuadMesh->GetVAO());
                // VAO 오염 가드 - Effekseer/Box2D 가 EBO 를 덮어쓸 수 있음
                if (auto ebo = mScreenQuadMesh->GetIndexBuffer())
                    ebo->Bind();
                rc.DrawIndexed(mScreenQuadMesh->GetIndexCount());

                // 상태 복원 불요 - 다음 WorldMesh 가 ApplyPipelineState 로, 다음 패스는 BeginFrame 으로 자기 state 적용.
                mLastOutputFB = cmd.outputFB;
                // FB 전환 후 program/material 상태 초기화 - 다음 WorldMesh 가 재바인딩
                lastProg = nullptr;
                lastMat  = nullptr;
                continue;
            }

            // -- WorldMesh ---------------------------------------------------------
            // SSoT - meshRenderer 경유 program/mesh/material 추출 (DrawCommand 직접 필드 폐기).
            if (!cmd.meshRenderer) continue;
            const Material* material = cmd.meshRenderer->Material;
            const Mesh*     mesh     = cmd.meshRenderer->Mesh;
            if (!material || !mesh) continue;
            const Program*  program  = material->GetProgram();
            if (!program) continue;

            // Phase 2 T4 - Slang UBO 셰이더 여부 게이트 (program 가 active uniform block 1개 이상 보유).
            //   true  -> UpdateUniformBlock + BindUniformBlocks 경로 (FrameBlock/DrawBlock/MaterialBlock 분할 갱신).
            //   false -> 기존 loose glUniform* 경로 (비-UBO 셰이더 공존 보존 - phong/skybox/postfx 등 Phase 3 까지 유지).
            //   행렬은 D13 결정에 따라 비전치 raw 바이트 송신 (R1 = T5 PoC 육안 게이트, 실패 시 T6 전치 분기).
            const bool useUbo = program->HasUniformBlocks();

            // 결정 1: Program 전환 - view/proj 송신 + (UBO 시) BindBufferBase 결속.
            if (program != lastProg) {
                rc.UseProgram(*program);
                if (useUbo) {
                    // FrameBlock std140 : { mat4 uView @0; mat4 uProj @64; } - 비전치 raw 바이트 (D13).
                    program->UpdateUniformBlock("FrameBlock", &viewMat,
                                                sizeof(glm::mat4), 0);
                    program->UpdateUniformBlock("FrameBlock", &projMat,
                                                sizeof(glm::mat4), sizeof(glm::mat4));
                    program->BindUniformBlocks();
                } else {
                    if (program->GetLocation(Const::UNI_VIEW) >= 0)
                        Uniforms::SetMat4(*program, Const::UNI_VIEW, viewMat);
                    if (program->GetLocation(Const::UNI_PROJ) >= 0)
                        Uniforms::SetMat4(*program, Const::UNI_PROJ, projMat);
                }
                lastProg = program;
                lastMat  = nullptr;   // program 바뀌면 material 재바인딩 강제
            }

            // 결정 2: Material 전환 - PropertyBlock -> uniform/texture 송신.
            //   UBO 시 MaterialBlock 의 baseColor 는 PropertyBlockSetter 의 *자연 skip* 대상 (P4).
            //   PropertyBlock 의 active uniform 캐시는 UBO 멤버를 location=-1 로 보유 -> glUniform* 가 no-op.
            //   따라서 baseColor 는 *별도 경로* 로 직접 주입 + PropertyBlockSetter 호출은 보존 (비-UBO 셰이더 + 텍스처 송신용).
            if (material != lastMat) {
                if (useUbo) {
                    // MaterialBlock std140 : { vec4 baseColor @0; } - Material PropertyBlock 의 Vec4s 에서 추출.
                    //   simple = 색, phong = albedo(.rgb), simple_texture = tint. 동일 레이아웃이라 분기 불요.
                    auto it = material->Properties.Vec4s.find("baseColor");
                    const glm::vec4 base = (it != material->Properties.Vec4s.end())
                                                ? it->second
                                                : glm::vec4(1, 1, 1, 1);   // 기본 흰색.
                    program->UpdateUniformBlock("MaterialBlock", &base,
                                                sizeof(glm::vec4), 0);
                }
                // Phase 3 Slice 0.5 - sampler/loose uniform 송신 (UBO/loose 공통).
                //   GL 4.1 은 sampler 를 UBO 에 못 넣으므로 UBO 셰이더라도 sampler(uTex 등)는 loose glUniform1i.
                //   PropertyBlockSetter 는 program active uniform 캐시를 순회 - UBO 멤버는 location=-1 라
                //   자연 skip (baseColor 이중송신 없음), sampler 만 바인딩. textureless UBO(phong/simple)는 무해(대상 0).
                //   (구 S7 의 UBO bypass 해제 - textured UBO 셰이더 합류로 sampler 바인딩 필요. PropertyBlockSetter
                //    제거는 Phase C/D-DPP-4 에서 sampler 전용 경로 분리 후.)
                PropertyBlockSetter::Set(rc, material->Properties, *program);
                lastMat = material;
            }

            // 결정 3: PipelineState 적용
            //   override 합성 없음 - 변형은 Material::Clone() + 별도 인스턴스 사용 (Unreal MID 정통).
            const Pass::PipelineState passState = Pass::DefaultPipelineStateOf(material->GetPass());
            rc.ApplyPipelineState(passState);

            // 결정 4: model 송신 + draw.
            if (useUbo) {
                // DrawBlock std140 : { mat4 uModel @0; } - 비전치 raw (D13).
                program->UpdateUniformBlock("DrawBlock", &cmd.modelMatrix,
                                            sizeof(glm::mat4), 0);
            } else {
                if (program->GetLocation(Const::UNI_MODEL) >= 0)
                    Uniforms::SetMat4(*program, Const::UNI_MODEL, cmd.modelMatrix);
            }
            rc.BindVAO(mesh->GetVAO());
            rc.DrawIndexed(mesh->GetIndexCount());
        }

        // RestoreDefaults 불요 (D-RS-2) - 다음 consumer(BeginFrame / 다음 Process / foreign InvalidateStateCache)가
        //   진입 시 캐시를 무효화하고 자기 state 를 적용한다. "consumer 진입 시 무효화" 불변식.
    }
}
