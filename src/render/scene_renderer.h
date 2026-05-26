#ifndef __SJH_SCENE_RENDERER_H__
#define __SJH_SCENE_RENDERER_H__

#include "render/light_uniform_dispatcher.h"
#include "render/mesh_pass_processor.h"
#include "render/postfx_pass.h"
#include "render/render_stage.h"
#include <cstdint>
#include <vector>
#include <vmath.h>

namespace SJH::Scene { class Actor; class Camera; }
namespace SJH { class RenderTarget; class Framebuffer; class Mesh; }

namespace SJH
{
    /// @brief Actor 트리 traverse -> MeshRenderer 수집 -> DrawCommand -> Queue Flush.
    /// @details per-frame 호출 (multi-pass 모드):
    ///   1. SceneContext::GetCameras() 직접 조회 — DFS 폐기 (Cocos2D `Scene::_cameras` 정통)
    ///   2. addCamera 호출 순서 그대로 렌더 (std::sort 폐기) — Cocos2D `addChild` 정통
    ///   3. 각 Camera 마다 RenderWithCamera 1패스:
    ///      - Camera::GetTargetRenderTarget() 강제 — nullptr 이면 warn+skip (Phase B)
    ///      - DeviceContext::BeginFrame(target) + Actor 트리 MeshRenderer 수집 + Queue Flush
    ///   4. PostFX 체인 — 씬 카메라 렌더 직후 RunPostFXChain 호출 (doc/design/PostFX.md §3.2).
    ///      Camera-per-pass 폐기 — PostFXPass 가 체인 인덱스 = 실행 순서 (Godot Array[RID] 정통).
    ///
    /// @brief High-level Orchestrator — Unreal `FSceneRenderer` 정통.
    class SceneRenderer : public IRenderStage
    {
    public:
        /// @brief SceneContext 의 Camera 컬렉션을 순회하며 직렬 렌더 후 PostFX 체인 실행.
        /// @param defaultTarget IRenderStage 인터페이스 준수용 — 내부에서는 Camera 및 PostFX 전용 RT 사용.
        void Render(RenderTarget& defaultTarget) override;

        /// @brief PostFX 체인 등록 + 풀스크린 quad 메쉬 지정.
        /// @param chain    PostFXPass 배열 — *인덱스 순서* 가 곧 실행 순서 (doc/design/PostFX.md §3.1).
        /// @param quadMesh ScreenQuad 메쉬 — RunPostFXChain 에서 blit 에 사용.
        void SetPostFXChain(std::vector<PostFXPass> chain, Mesh* quadMesh);

        /// @brief PostFX 체인 비우기. GetActiveFXOutput() 은 nullptr 반환 → ScreenQuadStage fallback.
        void ClearPostFXChain();

        /// @brief 마지막으로 렌더된 PostFX pass 의 OutputFB.
        /// @return nullptr = 활성 pass 없음 (체인 비거나 전부 disabled) — 호출자가 SceneFB 를 fallback 으로.
        const Framebuffer* GetActiveFXOutput() const { return mLastActiveFXOutput; }

    private:
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat, uint64_t cullingMask);
        void RenderWithCamera(Scene::Camera& cam);

        /// @brief 씬 카메라 렌더 직후 PostFX 체인 순차 실행 — doc/design/PostFX.md §3.2.
        /// @param sceneInput 씬 카메라 출력 FB — 첫 번째 PostFX pass 의 입력.
        void RunPostFXChain(const Framebuffer& sceneInput);

        MeshPassProcessor      mProcessor;
        LightUniformDispatcher mDispatcher;

        std::vector<PostFXPass> mPostFXChain;
        Mesh*                   mQuadMesh          = nullptr;
        const Framebuffer*      mLastActiveFXOutput = nullptr;
    };
}

#endif // __SJH_SCENE_RENDERER_H__
