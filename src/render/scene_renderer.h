#ifndef __SJH_SCENE_RENDERER_H__
#define __SJH_SCENE_RENDERER_H__

#include "render/mesh_pass_processor.h"
#include <cstdint>
#include <vmath.h>
#include <vector>

namespace SJH::Scene { class Actor; class Camera; }
namespace SJH { class Framebuffer; class Program; class DirLight; class PointLight; class SpotLight; class RenderTarget; }
#include <unordered_set>

namespace SJH
{
    /// @brief Actor 트리 traverse -> MeshRenderer 수집 -> DrawCommand -> Queue Flush.
    /// @details per-frame 호출 (multi-pass 모드):
    ///   1. Scene::Root() 부터 DFS — 모든 Camera 컴포넌트 수집 (IsActive + IsEnabled)
    ///   2. Camera::GetDepth() 오름차순 stable_sort (Unity Camera.depth 정통)
    ///   3. 각 Camera 마다 RenderWithCamera 1패스:
    ///      - Camera::GetTargetFramebuffer() 가 있으면 해당 FBO, 없으면 default backbuffer
    ///      - DeviceContext::BeginFrame(target) + Actor 트리 MeshRenderer 수집 + Queue Flush
    ///
    /// @brief High-level Orchestrator — Unreal `FSceneRenderer` 정통.
    ///        Actor 트리 traverse -> MeshRenderer 수집 -> DrawCommand -> MeshPassProcessor::Process 위임.
    class SceneRenderer
    {
    public:
        /// @brief 씬 트리의 모든 Camera 컴포넌트 수집 -> depth 정렬 -> 직렬 렌더.
        /// @param defaultTarget Application 이 보유한 window backbuffer — Camera 의 targetFramebuffer 가
        ///                      nullptr 일 때 fallback 으로 사용 (SP-RTOwnership — DeviceContext 슬림화).
        /// @details Camera 컴포넌트가 하나도 없으면 spdlog::warn + early return (프레임 skip).
        ///          Unity Camera.depth 정통 — 작은 depth 가 먼저 렌더.
        void Render(RenderTarget& defaultTarget);

        /// @brief 명시 view/proj — 단위 테스트 + 디버그용 (CameraComponent 우회).
        /// @details 기존 인터페이스 보존 — CameraComponent 없이 임의 view/proj 직접 주입 가능.
        void Render(RenderTarget& defaultTarget,
                    const vmath::mat4& viewMat, const vmath::mat4& projMat);

    private:
        /// @brief Actor 트리 DFS — MeshRenderer 수집 + Queue 에 Submit.
        /// @param cullingMask Camera::GetCullingMask() — actor.GetLayer() 와 AND 검사로 필터 (SP4 D-15).
        ///        자식 트리는 visibleToCamera 와 무관하게 계속 traverse (자식이 다른 layer 일 수 있음).
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat, uint32_t cullingMask);

        /// @brief Actor 트리 DFS — IsActive + IsEnabled Camera 컴포넌트를 out 에 수집.
        void CollectCameras(const Scene::Actor& actor, std::vector<Scene::Camera*>& out);

        /// @brief 단일 Camera 1패스 — target FB 바인딩 + Actor 수집 + Light uniform 송신 + Queue flush.
        void RenderWithCamera(Scene::Camera& cam, RenderTarget& defaultTarget);

        /// @brief Actor 트리 DFS — DirLight/PointLight/SpotLight 컴포넌트 수집 (SP5).
        /// @details DirLight 첫 1개, PointLight 모두 (셰이더 #define NUM_POINT_LIGHTS 2 와 매칭),
        ///          SpotLight 첫 1개. 초과 발견 시 spdlog::warn + 첫 N 만 사용.
        void CollectLights(const Scene::Actor& actor,
                           DirLight*& outDir,
                           std::vector<PointLight*>& outPoints,
                           SpotLight*& outSpot);

        /// @brief Actor 트리 DFS — MeshRenderer 의 Material 의 Program 을 unique set 으로 수집 (SP5).
        /// @details Light uniform 을 어떤 program 에 송신할지 — 씬 안에 등장한 모든 program.
        void CollectPrograms(const Scene::Actor& actor,
                             std::unordered_set<const Program*>& out);

        /// @brief 활성 Light 들을 모든 program 에 송신 (SP5).
        /// @details lighting.fs 의 uniform 명 (dirLight / pointLights[i] / spotLight + *Enabled int)
        ///          에 1:1 매핑. cam.GetEye() 가 viewPos uniform.
        void SendLightUniforms(const std::unordered_set<const Program*>& programs,
                               DirLight* dir,
                               const std::vector<PointLight*>& points,
                               SpotLight* spot,
                               const vmath::vec3& viewPos);

        MeshPassProcessor mProcessor;
    };
}

#endif // __SJH_SCENE_RENDERER_H__
