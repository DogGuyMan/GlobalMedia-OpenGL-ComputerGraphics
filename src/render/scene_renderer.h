#ifndef __SJH_SCENE_RENDERER_H__
#define __SJH_SCENE_RENDERER_H__

#include "render/light_uniform_dispatcher.h"
#include "render/mesh_pass_processor.h"
#include "render/render_stage.h"
#include <cstdint>
#include <vmath.h>

namespace SJH::Scene { class Actor; class Camera; }
namespace SJH { class RenderTarget; }

namespace SJH
{
    /// @brief Actor 트리 traverse -> MeshRenderer 수집 -> DrawCommand -> Queue Flush.
    /// @details per-frame 호출 (multi-pass 모드):
    ///   1. SceneContext::GetCameras() 직접 조회 — DFS 폐기 (Cocos2D `Scene::_cameras` 정통)
    ///   2. addCamera 호출 순서 그대로 렌더 (std::sort 폐기) — Cocos2D `addChild` 정통
    ///   3. 각 Camera 마다 RenderWithCamera 1패스:
    ///      - Camera::GetTargetRenderTarget() 강제 — nullptr 이면 warn+skip (Phase B)
    ///      - DeviceContext::BeginFrame(target) + Actor 트리 MeshRenderer 수집 + Queue Flush
    ///
    /// @brief High-level Orchestrator — Unreal `FSceneRenderer` 정통.
    ///        Actor 트리 traverse -> MeshRenderer 수집 -> DrawCommand -> MeshPassProcessor::Process 위임.
    class SceneRenderer : public IRenderStage
    {
    public:
        /// @brief SceneContext 의 Camera 컬렉션을 순회하며 직렬 렌더.
        /// @param defaultTarget IRenderStage 인터페이스 준수용 — SceneRenderer 내부에서는 사용하지 않음.
        ///                      각 Camera 가 자기 RenderTarget 을 보유 (Phase B).
        /// @details Camera 컴포넌트가 하나도 없으면 spdlog::warn + early return (프레임 skip).
        ///          addCamera 호출 순서대로 렌더 — Cocos2D `addChild` 정통.
        void Render(RenderTarget& defaultTarget) override;

    private:
        /// @brief Actor 트리 DFS — MeshRenderer 수집 + Queue 에 Submit.
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat, uint64_t cullingMask);

        /// @brief 단일 Camera 1패스 — cam.GetTargetRenderTarget() 강제. nullptr 이면 warn+skip.
        void RenderWithCamera(Scene::Camera& cam);

        MeshPassProcessor mProcessor;
        LightUniformDispatcher mDispatcher; ///< Light uniform 일괄 송신 위임 (Phase 2).
    };
}

#endif // __SJH_SCENE_RENDERER_H__
