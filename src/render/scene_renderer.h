#ifndef __SJH_SCENE_RENDERER_H__
#define __SJH_SCENE_RENDERER_H__

#include "render/mesh_pass_processor.h"
#include "render/render_stage.h"
#include <cstdint>
#include <vmath.h>
#include <vector>

namespace SJH::Scene { class Actor; class Camera; }
namespace SJH { class Program; class DirLight; class PointLight; class SpotLight; class RenderTarget; }

namespace SJH
{
    /// @brief Actor 트리 traverse -> MeshRenderer 수집 -> DrawCommand -> Queue Flush.
    /// @details per-frame 호출 (multi-pass 모드):
    ///   1. SceneContext::GetCameras() 직접 조회 — DFS 폐기 (Cocos2D `Scene::_cameras` 정통)
    ///   2. addCamera 호출 순서 그대로 렌더 (std::sort 폐기) — Cocos2D `addChild` 정통
    ///   3. 각 Camera 마다 RenderWithCamera 1패스:
    ///      - Camera::GetTargetRenderTarget() 가 있으면 해당 RT, 없으면 default backbuffer
    ///      - DeviceContext::BeginFrame(target) + Actor 트리 MeshRenderer 수집 + Queue Flush
    ///
    /// @brief High-level Orchestrator — Unreal `FSceneRenderer` 정통.
    ///        Actor 트리 traverse -> MeshRenderer 수집 -> DrawCommand -> MeshPassProcessor::Process 위임.
    class SceneRenderer : public IRenderStage
    {
    public:
        /// @brief SceneContext 의 Camera 컬렉션을 순회하며 직렬 렌더.
        /// @param target IRenderStage 계약 파라미터 — Phase B 이후 미사용 (모든 Camera 가 자기 FBO 보유).
        /// @details Camera 컴포넌트가 하나도 없으면 spdlog::warn + early return (프레임 skip).
        ///          addCamera 호출 순서대로 렌더 — Cocos2D `addChild` 정통.
        void Render(RenderTarget& target) override;

	// 상시 Camera를 찾는것은 이상하다 RenderTarget Plane을 가지고 있음.
	// Light 는 어떤 관점으로 바라봐야 하지?

    private:
        /// @brief Actor 트리 DFS — MeshRenderer 수집 + Queue 에 Submit.
        /// @param cullingMask Camera::GetCullingMask() — actor.GetLayer() 와 AND 검사로 필터 (SP4 D-15).
        ///        자식 트리는 visibleToCamera 와 무관하게 계속 traverse (자식이 다른 layer 일 수 있음).
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat, uint64_t cullingMask);

        /// @brief 단일 Camera 1패스 — cam.GetTargetRenderTarget() 바인딩 + Actor 수집 + Light uniform 송신 + Queue flush.
        /// @note cam.GetTargetRenderTarget() 이 nullptr 이면 assert (Phase B: Camera 강제 non-null).
        void RenderWithCamera(Scene::Camera& cam);

        /// @brief 활성 Light 들을 모든 program 에 송신 (SP5).
        /// @details lighting.fs 의 uniform 명 (dirLight / pointLights[i] / spotLights[i] + *Enabled int)
        ///          에 1:1 매핑. cam.GetEye() 가 viewPos uniform.
        ///          첫 인자는 ResourceRegistry::GetAllPrograms() 반환과 정합되도록 vector<Program*>.
        void SendLightUniforms(const std::vector<Program*>& programs,
                               DirLight* dir,
                               const std::vector<PointLight*>& points,
                               const std::vector<SpotLight*>& spots,
                               const vmath::vec3& viewPos);

        MeshPassProcessor mProcessor;
    };
}

#endif // __SJH_SCENE_RENDERER_H__
