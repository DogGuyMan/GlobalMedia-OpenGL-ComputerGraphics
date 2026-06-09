/**
 * @file mesh_pass_processor.h
 * @brief DrawCommand 큐 관리/정렬/GL draw 발행을 담당하는 Low-level Orchestrator.
 *
 * @details
 *  ### 책임 (SP-MeshPassProcessor)
 *  - @c Submit / @c Clear / @c Size - DrawCommand 수집/관리.
 *  - @c SortMultiStage - queueLayer / program / material / depth 다단계 정렬.
 *  - @c Process - 정렬된 command 를 Program/Material 전환 + Applier 위임 + GL draw 발행.
 *
 *  ### 비-책임 (분리된 책임)
 *  - [X] GL state machine (stencil/depth/cull/blend) 전환 -> @c PipelineStateSetter 위임.
 *  - [X] Material properties (uniform/texture) 송신 -> @c PropertyBlockSetter 위임.
 *
 *  ### DrawCommand 구조
 *  두 가지 Kind 를 구분:
 *  - @c WorldMesh - @c MeshRenderer 포인터를 SSoT 로 사용 (program/mesh/material/actor 모두 경유).
 *  - @c ScreenQuad - PassComponent (PostFX) 용 inputFB -> outputFB blit.
 *
 *  ### 정통 매핑
 *  - Unreal `FMeshPassProcessor` - 한 Pass 안의 mesh draw command 들을 처리하는 Orchestrator.
 *  - Cocos2D `RenderQueue` - Layer 정렬 + 순서 발행.
 *
 * @note @c SortMultiStage 는 @c std::stable_sort 사용 - z-fighting 깜빡임 차단 + 결정성 보장
 *       (골든 이미지 비교 가능). 자세한 정렬 정책은 @c mesh_pass_processor.cpp 참조.
 */
#ifndef __SJH_MESH_PASS_PROCESSOR_H__
#define __SJH_MESH_PASS_PROCESSOR_H__

#include <vmath.h>
#include <cstddef>
#include <vector>

namespace SJH::Scene { class MeshRenderer; }
namespace SJH
{
    class DeviceContext;
    class Framebuffer;
    class Material;
    class Mesh;

    /**
     * @brief 한 프레임의 정렬 가능한 draw command - WorldMesh 또는 ScreenQuad(PassComponent) 구분.
     * @details
     *  ### SSoT - MeshRenderer 단일 의존 (WorldMesh)
     *  program/mesh/material/actor 4-필드 직접 보관 폐기. 모두 @c meshRenderer 경유 접근:
     *  - mesh:     `meshRenderer->Mesh`
     *  - material: `meshRenderer->Material`
     *  - program:  `meshRenderer->Material->GetProgram()`
     *  - actor:    `meshRenderer->GetOwner()` (Component 베이스)
     *
     *  *수집 시점 가변 데이터* (@c modelMatrix / @c queueLayer / @c depth) 만 별도 필드 - per-frame 계산값.
     *  GL state 는 @c Pass::DefaultPipelineStateOf(material->GetPass()) 로 도출 (Material 이 SSoT).
     */
    struct DrawCommand
    {
        /// @brief DrawCommand 의 종류 - WorldMesh(일반 메시) 또는 ScreenQuad(PostFX PassComponent).
        enum class Kind { WorldMesh, ScreenQuad };

        // -- 공통 필드 ------------------------------------------------------
        Kind  kind       = Kind::WorldMesh;  ///< DrawCommand 종류.
        int   queueLayer = 2000;             ///< 렌더 큐 레이어 (낮을수록 먼저 그림). Opaque~=2000, Transparent~=3000.
        float depth      = 0.0f;             ///< view-space z (카메라 전방이 음수 - 정렬 시 부호 주의).

        // -- WorldMesh 전용 -------------------------------------------------
        const Scene::MeshRenderer *meshRenderer = nullptr;      ///< SSoT - program/mesh/material/actor 모두 경유. @c Kind::WorldMesh 에서만 유효.
        vmath::mat4                modelMatrix  = vmath::mat4::identity();  ///< 수집 시점 Actor 월드 행렬.

        // -- ScreenQuad 전용 (PassComponent) -------------------------------
        Framebuffer *inputFB      = nullptr;  ///< 읽기 소스 - @c uScene sampler 바인딩 대상. @c Kind::ScreenQuad 에서만 유효.
        Framebuffer *outputFB     = nullptr;  ///< 쓰기 대상 - @c BeginFrame 바인딩 + @c mLastOutputFB 갱신.
        Material    *passMaterial = nullptr;  ///< ScreenQuad 셰이더. nullptr 이면 bypass blit (passthrough).
    };

    /**
     * @brief Low-level Orchestrator - DrawCommand 컬렉션의 정렬/조건 결정 + Applier 위임.
     * @details
     *  Unreal @c FMeshPassProcessor 정통 - 한 Pass 안의 mesh draw command 들을 처리.
     *
     *  책임 (orchestration 만):
     *  - @c Submit / @c Clear / @c Size - command 수집/관리.
     *  - @c SortMultiStage - queueLayer/program/material/depth 다단계 정렬.
     *  - @c Process - 정렬된 command 발행 (Program/Material 전환 + Applier 위임 + draw).
     *
     *  GL state machine (stencil/depth/cull/blend) 은 @c PipelineStateSetter 에 위임.
     *  Material properties (uniform/texture) 는 @c PropertyBlockSetter 에 위임.
     */
    class MeshPassProcessor
    {
    public:
        /// @brief DrawCommand 를 큐에 추가.
        /// @param cmd 추가할 DrawCommand.
        void Submit(const DrawCommand& cmd) { mItems.push_back(cmd); }

        /// @brief 큐를 비우고 @c mLastOutputFB 를 nullptr 로 초기화 (프레임 시작 시 호출).
        void Clear()
        {
            mItems.clear();
            mLastOutputFB = nullptr;  // 프레임마다 리셋
        }

        /// @brief 현재 큐에 쌓인 DrawCommand 개수.
        std::size_t Size() const { return mItems.size(); }

        /// @brief ScreenQuad 드로우에 사용할 풀스크린 메시 지정.
        /// @param mesh nullptr 이면 ScreenQuad DrawCommand 가 skip 됨.
        void SetScreenQuadMesh(Mesh *mesh) { mScreenQuadMesh = mesh; }

        /// @brief disabled PassComponent 의 bypass blit 에 사용할 passthrough material.
        /// @details @c cmd.passMaterial == nullptr 일 때 이 material 로 inputFB -> outputFB blit.
        /// @param mat passthrough 셰이더를 담은 Material. nullptr 이면 blit skip.
        void SetBypassMaterial(Material *mat) { mBypassMat = mat; }

        /// @brief 마지막으로 처리된 ScreenQuad(PassComponent) 의 outputFB.
        /// @return 이번 프레임 PassComponent 가 없거나 아직 @c Process 전이면 nullptr.
        const Framebuffer *GetLastOutputFB() const { return mLastOutputFB; }

        /// @brief 큐를 다단계 정렬 - queueLayer -> program -> material -> depth (back-to-front / front-to-back).
        /// @details @c std::stable_sort 사용으로 동등 명령의 순서가 매 프레임 동일 (결정성 보장).
        void SortMultiStage();

        /// @brief 정렬된 DrawCommand 를 순서대로 발행 - Program/Material 전환 + PipelineState 적용 + draw.
        /// @details
        ///  - Program 전환 시 @c DeviceContext::UseProgram + view/proj uniform 송신.
        ///  - Material 전환 시 @c PropertyBlockSetter::Set (PropertyBlock -> uniform/texture 송신).
        ///  - 매 command 마다 @c PipelineStateSetter::Set (Pass.PipelineState -> GL state machine).
        ///  - Flush 종료 시 @c PipelineStateSetter::RestoreDefaults - 다음 패스를 위한 표준 상태 복원.
        /// @param rc      DeviceContext 레퍼런스 (UseProgram/BindVAO/DrawIndexed 등).
        /// @param viewMat 이번 패스 View 행렬.
        /// @param projMat 이번 패스 Projection 행렬.
        void Process(DeviceContext& rc,
                     const vmath::mat4& viewMat,
                     const vmath::mat4& projMat);

    private:
        std::vector<DrawCommand> mItems;                          ///< 이번 프레임 DrawCommand 큐.
        Mesh              *mScreenQuadMesh = nullptr;             ///< ScreenQuad 드로우용 풀스크린 메시.
        Material          *mBypassMat      = nullptr;             ///< PassComponent disabled 시 bypass blit material.
        const Framebuffer *mLastOutputFB   = nullptr;             ///< 마지막 ScreenQuad outputFB 추적.
    };
}

#endif // __SJH_MESH_PASS_PROCESSOR_H__
