/**
 * @file pipeline_state_setter.h
 * @brief Pass::PipelineState + Stencil override 를 GL state 로 적용 — last-applied dirty 캐싱.
 *
 * @details
 *  ### 책임 (SP-PipelineSetter)
 *  - **입력**: `Pass::PipelineState` (Material.Pass.Kind 가 결정한 값) + `Scene::StencilState` (MeshRenderer override)
 *  - **출력**: GL state machine 전환 (`glEnable/Disable/StencilFunc/Op/Mask/DepthMask/DepthFunc/BlendFunc/CullFace`)
 *  - **캐시**: 이전 적용 state — redundant GL 호출 회피 (dirty check)
 *  - **라이프사이클**: `Set` (per-command 호출) + `RestoreDefaults` (Flush 종료 시 표준 opaque 로 복원)
 *
 *  ### 정통 매핑
 *  - DirectX 12 `ID3D12CommandList::SetPipelineState(pso)` — 명령 발행
 *  - Vulkan `vkCmdBindPipeline` — 파이프라인 활성화
 *  - Unity SRP `DrawRenderers(..., RenderStateBlock)` 의 *내부 적용* 단계
 *
 *  ### Orchestrator 와의 관계 (MeshPassProcessor::Process)
 *  Orchestrator (MeshPassProcessor) 가 *언제* PipelineStateSetter 를 호출할지 결정.
 *  본 클래스는 *어떻게 GL 호출로 변환* 만 책임 — Applier 패턴 정통 (Material 의 형제 `PropertyBlockSetter`).
 */
#ifndef __SJH_PIPELINE_STATE_SETTER_H__
#define __SJH_PIPELINE_STATE_SETTER_H__

#include "material/pass.h"        // Pass::PipelineState — 입력 값 객체.
#include "render/mesh_renderer.h" // Scene::StencilState — MeshRenderer override.

namespace SJH
{
    /// @brief GL state machine 의 단일 진실의 원천 — PipelineState 입력 → GL 호출 출력.
    /// @details mLast 캐시로 redundant 호출 회피. mInitialized=false 면 first-call 강제 적용.
    class PipelineStateSetter
    {
    public:
        /// @brief Pass.PipelineState + Stencil override 를 GL state 로 적용 (dirty check).
        void Set(const Pass::PipelineState& want, const Scene::StencilState& stencilOverride);

        /// @brief Flush 종료 시 표준 opaque 상태로 복원 — 다음 Flush 가 깨끗한 시작점 가정.
        /// @details (stencil off / depth on+write on+func LESS / cull back / blend off)
        void RestoreDefaults();

    private:
        // ─── 캐시 — 이전 적용 state ─────────────────────────────────────────
        Pass::PipelineState mLast;                 ///< 직전 적용된 GL state (1-B 채택: PipelineState 통일)
        Scene::StencilState mLastStencil;          ///< 직전 적용된 Stencil override
        bool                mInitialized = false;  ///< first-call 강제 적용 단일 flag
    };
}

#endif // __SJH_PIPELINE_STATE_SETTER_H__
