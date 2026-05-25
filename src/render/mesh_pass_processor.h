#ifndef __SJH_MESH_PASS_PROCESSOR_H__
#define __SJH_MESH_PASS_PROCESSOR_H__

#include <vmath.h>
#include <cstddef>
#include <vector>

namespace SJH::Scene { class MeshRenderer; }
namespace SJH
{
    class DeviceContext;

    /// @brief Cocos 식 Layer A — 한 프레임의 정렬 가능한 draw command.
    /// @details
    ///   ### SSoT — MeshRenderer 단일 의존
    ///   program/mesh/material/actor 4-필드 직접 보관 폐기. 모두 meshRenderer 경유 접근.
    ///   접근 경로:
    ///   - mesh:     `meshRenderer->Mesh`
    ///   - material: `meshRenderer->Material`
    ///   - program:  `meshRenderer->Material->GetProgram()`
    ///   - actor:    `meshRenderer->GetOwner()` (Component 베이스)
    ///
    ///   *수집 시점 가변 데이터* (modelMatrix/queueLayer/depth) 만 별도 필드 — per-frame 계산값.
    ///   GL state 는 `Pass::DefaultPipelineStateOf(material->GetPass())` 도출 (Material 이 SSoT).
    struct DrawCommand
    {
        const Scene::MeshRenderer* meshRenderer = nullptr;             ///< SSoT — program/mesh/material/actor 모두 경유 접근.
        vmath::mat4                modelMatrix  = vmath::mat4::identity(); ///< 미지정 시 항등 — 디버그 가능 default.
        int                        queueLayer   = 2000;
        float                      depth        = 0.0f;                ///< view-space z (back-to-front)
    };

    /// @brief Low-level Orchestrator — DrawCommand 컬렉션의 *순서 + 조건* 결정 + Applier 들에게 위임.
    /// @details Unreal `FMeshPassProcessor` 정통 — *한 Pass 안의 mesh draw command 들을 처리*.
    ///   책임 (orchestration 만):
    ///   - Submit/Clear/Size — command 수집/관리
    ///   - SortMultiStage — queueLayer/program/material/depth 다단계 정렬
    ///   - Process — 정렬된 command 발행 (program/material 전환 + Applier 위임 + draw)
    ///
    ///   GL state machine (stencil/depth/cull/blend) 은 `PipelineStateSetter` 에 위임.
    ///   Material properties (uniform/texture) 는 `PropertyBlockSetter` 에 위임.
    class MeshPassProcessor
    {
    public:
        void Submit(const DrawCommand& cmd) { mItems.push_back(cmd); }
        void Clear()                        { mItems.clear(); }
        std::size_t Size() const            { return mItems.size(); }

        /// @brief Multi-stage sort: queueLayer -> program -> material -> depth (back-to-front).
        void SortMultiStage();

        /// @brief 정렬된 command 발행 (옛 MeshPassProcessor::Process rename).
        /// @details program 전환 시 UseProgram + view/proj uniform. material 전환 시
        ///          PropertyBlockSetter::Set + BindTextures. per-draw 는 PipelineStateSetter::Set + model matrix.
        void Process(DeviceContext& rc,
                     const vmath::mat4& viewMat,
                     const vmath::mat4& projMat);

    private:
        std::vector<DrawCommand> mItems;
    };
}

#endif // __SJH_MESH_PASS_PROCESSOR_H__
