/**
 * @file pipeline_state_setter.h
 * @brief @c Pass::PipelineState 를 GL state machine 으로 적용 - last-applied dirty 캐싱.
 *
 * @details
 *  ### 책임 (SP-PipelineSetter, SP-MaterialSSoT)
 *  - **입력**: @c Pass::PipelineState - Material.Pass.Kind 가 결정한 완전한 GL state (Stencil 포함).
 *  - **출력**: GL state machine 전환
 *    (@c glEnable/Disable / @c glStencilFunc/Op/Mask / @c glDepthMask/Func / @c glBlendFunc / @c glCullFace).
 *  - **캐시**: @c mLast - 이전 적용 state 로 redundant GL 호출 회피 (dirty check).
 *  - **라이프사이클**: @c Set (per-command 호출) + @c RestoreDefaults (Flush 종료 시 표준 opaque 복원).
 *
 *  ### SP-MaterialSSoT - 시그니처 단순화
 *  - 이전: `Set(want, stencilOverride)` - MeshRenderer 의 Stencil override 별도 수신.
 *  - 현재: `Set(want)` - @c Pass::PipelineState 안에 이미 Stencil 필드 통합 보유
 *    (@c StencilEnable / @c StencilFunc / @c StencilRef / ...).
 *
 *  ### 비-책임
 *  - [X] 언제 호출할지 결정 -> @c MeshPassProcessor::Process (Orchestrator) 담당.
 *  - [X] Material uniform/texture 송신 -> @c PropertyBlockSetter 담당.
 *
 *  ### 정통 매핑
 *  - DirectX 12 @c ID3D12CommandList::SetPipelineState(pso) - 명령 발행.
 *  - Vulkan @c vkCmdBindPipeline - 파이프라인 활성화.
 *  - Unity SRP @c DrawRenderers(..., RenderStateBlock) 의 내부 적용 단계.
 *
 * @note Applier 패턴 - @c MeshPassProcessor 가 *언제* 를 결정, 본 클래스가 *어떻게* 를 결정.
 */
#ifndef __SJH_PIPELINE_STATE_SETTER_H__
#define __SJH_PIPELINE_STATE_SETTER_H__

#include "material/pass.h"        // Pass::PipelineState - 입력 값 객체 (Stencil 포함).

namespace SJH
{
    /**
     * @brief GL state machine 단일 Applier - @c Pass::PipelineState 입력 -> GL 호출 출력.
     * @details
     *  @c mLast 캐시로 redundant GL 호출 회피 (dirty check).
     *  @c mInitialized = false 이면 first-call 로 간주 - 모든 state 를 강제 적용.
     *  @c RestoreDefaults 호출 시 @c mInitialized = false 로 리셋되어 다음 @c Set 이 first-call 처럼 동작.
     */
    class PipelineStateSetter
    {
    public:
        /// @brief @c Pass::PipelineState (Stencil 포함) 를 GL state machine 에 적용 (dirty check).
        /// @details 내부적으로 Stencil 4결정 + Depth 3결정 + Cull 1결정 + Blend 2결정 단위 함수로 분기.
        ///          @c mInitialized = false 이면 dirty check 없이 모든 state 강제 적용.
        /// @param want 적용할 목표 PipelineState.
        void Set(const Pass::PipelineState& want);

        /// @brief Flush 종료 시 표준 opaque 상태로 복원 + @c mInitialized = false 리셋.
        /// @details 다음 Flush 가 표준 opaque 상태에서 시작한다고 가정하도록 복원:
        ///          stencil off / depth on + write on + func LESS / cull back / blend off.
        ///          @c mInitialized 를 false 로 리셋하여 다음 @c Set 이 first-call 로 강제 적용.
        void RestoreDefaults();

    private:
        Pass::PipelineState mLast;                 ///< 직전 적용된 GL state (Stencil 포함 통합 캐시).
        bool                mInitialized = false;  ///< first-call 강제 적용 단일 flag - false 이면 dirty check 없이 전체 적용.
    };
}

#endif // __SJH_PIPELINE_STATE_SETTER_H__
