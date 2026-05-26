#ifndef __SJH_SCENE_RENDERER_H__
#define __SJH_SCENE_RENDERER_H__

#include "render/light_uniform_dispatcher.h"
#include "render/mesh_pass_processor.h"
#include "render/render_stage.h"
#include <cstdint>
#include <vmath.h>

namespace SJH::Scene { class Actor; class Camera; class PassComponent; }
namespace SJH { class RenderTarget; class Framebuffer; class Mesh; }

namespace SJH
{
    /// @brief Actor 트리 traverse -> MeshRenderer 수집 -> DrawCommand -> Queue Flush.
    /// @details per-frame 호출 (multi-pass 모드):
    ///   1. SceneContext::GetCameras() 직접 조회 — DFS 폐기 (Cocos2D `Scene::_cameras` 정통)
    ///   2. addCamera 호출 순서 그대로 렌더 (std::sort 폐기) — Cocos2D `addChild` 정통
    ///   3. 각 Camera 마다 RenderWithCamera 1패스:
    ///      - Camera::GetTargetRenderTarget() 강제 — nullptr 이면 warn+skip (Phase B)
    ///      - DeviceContext::BeginFrame(target) + Actor 트리 MeshRenderer+PassComponent 수집 + Queue Flush
    ///
    /// @brief High-level Orchestrator — Unreal `FSceneRenderer` 정통.
    class SceneRenderer : public IRenderStage
    {
    public:
        /// @brief SceneContext 의 Camera 컬렉션을 순회하며 직렬 렌더 후 PostFX 체인 실행.
        /// @param defaultTarget IRenderStage 인터페이스 준수용 — 내부에서는 Camera 및 PostFX 전용 RT 사용.
        void Render(RenderTarget& defaultTarget) override;

        /// @brief ScreenQuad mesh 지정 — PassComponent DrawCommand 처리 시 사용.
        void SetScreenQuadMesh(Mesh *mesh);

        /// @brief 이번 프레임 PassComponent 체인의 마지막 출력 FB.
        /// @return nullptr = PassComponent 없음 — caller 가 sceneFB fallback.
        const Framebuffer *GetLastSceneOutput() const { return mLastSceneOutput; }

    private:
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat, uint64_t cullingMask);
        void RenderWithCamera(Scene::Camera& cam);

        MeshPassProcessor      mProcessor;
        LightUniformDispatcher mDispatcher;

        const Framebuffer*     mLastSceneOutput = nullptr;
    };
}

#endif // __SJH_SCENE_RENDERER_H__
