#ifndef __SJH_RENDER_QUEUE_H__
#define __SJH_RENDER_QUEUE_H__

#include "scene/components.h"   // StencilState
#include <vmath.h>
#include <cstddef>
#include <vector>

namespace SJH::Scene { class Actor; }
namespace SJH
{
    class Program;
    class Mesh;
    class Material;
    class RenderContext;

    /// @brief Cocos 식 Layer A — 한 프레임의 정렬 가능한 draw command.
    /// @details MeshRenderer 의 per-actor GL 상태 (Stencil / DepthTest / DepthWrite) 를 함께 캐리.
    struct DrawCommand
    {
        const Program*       program     = nullptr;
        const Mesh*          mesh        = nullptr;
        const Material*      material    = nullptr;
        vmath::mat4          modelMatrix = vmath::mat4::identity(); ///< 미지정 시 항등 — 디버그 가능 default.
        int                  queueLayer  = 2000;
        const Scene::Actor*  actor       = nullptr;   ///< 디버그 추적
        float                depth       = 0.0f;      ///< view-space z (back-to-front)

        // ── per-actor override (MeshRenderer 에서 복사) ──
        Scene::StencilState  stencil;                  ///< 기본 disabled.
        bool                 depthTest   = true;
        bool                 depthWrite  = true;
    };

    class RenderQueue
    {
    public:
        void Submit(const DrawCommand& cmd) { mItems.push_back(cmd); }
        void Clear()                        { mItems.clear(); }
        std::size_t Size() const            { return mItems.size(); }

        /// @brief Multi-stage sort: queueLayer -> program -> material -> depth (back-to-front).
        void SortMultiStage();

        /// @brief 정렬된 command 발행.
        /// @details program 전환 시 UseProgram + view/proj uniform. material 전환 시
        ///          MaterialApplier::WriteUniforms + BindTextures. per-draw 는 model matrix.
        void Flush(RenderContext& rc,
                   const vmath::mat4& viewMat,
                   const vmath::mat4& projMat);

    private:
        std::vector<DrawCommand> mItems;
    };
}

#endif // __SJH_RENDER_QUEUE_H__
