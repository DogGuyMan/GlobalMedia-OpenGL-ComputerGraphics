/**
 * @file scene_renderer.h
 * @brief Actor 트리 순회 -> MeshRenderer/PassComponent 수집 -> DrawCommand 큐 Flush 까지
 *        한 프레임 렌더링을 총괄하는 High-level Orchestrator.
 *
 * @details
 *  ### 책임
 *  - SceneContext 에 등록된 Camera 컬렉션을 순서대로 순회하며 @c RenderWithCamera 호출.
 *  - Camera 마다: Light 수집 -> @c LightUniformDispatcher 위임 -> Actor DFS ->
 *    @c MeshPassProcessor 정렬/발행.
 *  - @c PassComponent (PostFX ScreenQuad) 를 DrawCommand 에 포함시켜 마지막 출력 FB 를 추적.
 *
 *  ### 비-책임
 *  - [X] GL uniform 값 직접 송신 - @c LightUniformDispatcher / @c PropertyBlockSetter 위임.
 *  - [X] GL 상태 머신 전환 - @c PipelineStateSetter / @c DeviceContext 위임.
 *  - [X] DrawCommand 정렬/GL draw 발행 - @c MeshPassProcessor 위임.
 *
 *  ### 정통 매핑
 *  - Unreal `FSceneRenderer` - 씬 전체 렌더 파이프라인 진입점.
 *  - Cocos2D `Renderer::render` - Camera 순회 + command 발행.
 *
 * @note @c Render(RenderTarget&) 는 deprecated. 신규 코드는 @c CameraStage + @c IRenderStage
 *       스테이지 컬렉션 패턴 사용.
 */
#ifndef __SJH_SCENE_RENDERER_H__
#define __SJH_SCENE_RENDERER_H__

#include "render/light_uniform_dispatcher.h"
#include "render/mesh_pass_processor.h"
#include "render/render_stage.h"
#include <cstdint>
#include <utility>
#include <vector>
#include <vmath.h>

namespace SJH::Scene { class Actor; class Camera; class PassComponent; }
namespace SJH { class RenderTarget; class Framebuffer; class Mesh; class Material; class Program; }

namespace SJH
{
    /**
     * @brief High-level Orchestrator - SceneContext Camera 컬렉션 순회 + 1패스 렌더 위임.
     * @details
     *  per-frame 호출 흐름 (multi-pass):
     *  1. @c SceneContext::GetCameras() 직접 조회 - DFS 폐기 (Cocos2D @c Scene::_cameras 정통).
     *  2. @c addCamera 호출 순서 그대로 렌더 (@c std::sort 폐기) - Cocos2D @c addChild 정통.
     *  3. 각 Camera 마다 @c RenderWithCamera 1패스:
     *     - @c Camera::GetTargetRenderTarget() 강제 - nullptr 이면 warn+skip.
     *     - @c DeviceContext::BeginFrame(target) + Actor 트리 MeshRenderer/PassComponent 수집 + 큐 Flush.
     *
     *  내부 멤버:
     *  - @c mProcessor - DrawCommand 큐 보유/정렬/GL draw 발행.
     *  - @c mDispatcher - 수집된 Light 를 모든 Program 에 일괄 송신.
     *  - @c mLastSceneOutput - 이번 프레임 PassComponent 체인의 마지막 출력 FB 추적.
     */
    class SceneRenderer : public IRenderStage
    {
    public:
        /// @brief SceneContext 의 Camera 컬렉션을 순회하며 직렬 렌더 후 PassComponent 체인 실행.
        /// @param defaultTarget IRenderStage 인터페이스 준수용 파라미터.
        ///        내부에서는 각 Camera 의 전용 RenderTarget 만 사용 - 이 값은 무시된다.
        /// @deprecated CameraStage + IRenderStage 스테이지 컬렉션 사용 권장 (4-엔진 정통).
        [[deprecated("Use CameraStage + IRenderStage stages 컬렉션 - Application 이 출차 책임 (4-엔진 정통)")]]
        void Render(RenderTarget& defaultTarget) override;

        /// @brief ScreenQuad mesh 지정 - PassComponent DrawCommand 처리 시 사용.
        /// @param mesh nullptr 이면 ScreenQuad DrawCommand 가 skip 됨.
        void SetScreenQuadMesh(Mesh *mesh);

        /// @brief disabled PassComponent 의 bypass blit material 지정.
        /// @details @c PassComponent::Enabled = false 일 때 inputFB -> outputFB 를 그대로 blit 하는 passthrough 셰이더.
        void SetBypassMaterial(Material *mat);

        /// @brief 이번 프레임 PassComponent 체인의 마지막 출력 FB.
        /// @return 마지막 PassComponent 가 출력한 Framebuffer 포인터.
        ///         PassComponent 가 없거나 아직 Flush 전이면 nullptr.
        const Framebuffer *GetLastSceneOutput() const { return mLastSceneOutput; }

        /// @brief 명시된 단일 Camera 에 대해 1패스 렌더 수행.
        /// @details @c SceneContext::GetCameras() 자동 순회를 우회하는 외부 진입점 - @c CameraStage 가 위임 호출.
        ///          @c Camera::GetTargetRenderTarget() 이 nullptr 이면 warn 후 skip.
        /// @param cam 렌더 대상 Camera (RenderTarget 이 연결되어 있어야 함).
        void RenderWithCamera(Scene::Camera& cam);

        /// @brief 라이트 uniform 송신 대상 Program 집합을 외부에서 주입 (D-1 push).
        /// @details rr 을 직접 pull 하던 의존을 끊기 위해 외부(Composition Root)가 매 프레임
        ///          @c ResourceRegistry::GetAllPrograms() 스냅샷을 push. render -> rr 의존 제거.
        /// @param programs 활성 Program 포인터 스냅샷 (owner = caller, 본 클래스는 복사 보유).
        void SetActivePrograms(std::vector<Program*> programs) { mActivePrograms = std::move(programs); }

    private:
        /// @brief Actor 트리 DFS - 활성 노드에서 MeshRenderer/PassComponent 를 DrawCommand 로 변환해 mProcessor 에 Submit.
        /// @param actor      현재 순회 노드.
        /// @param viewMat    이번 패스의 View 행렬 (depth 계산용).
        /// @param cullingMask 카메라 CullingMask - Actor layer 비트와 AND 로 가시성 판정.
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat, uint64_t cullingMask);

        MeshPassProcessor      mProcessor;          ///< DrawCommand 큐 보유/정렬/GL draw 발행.
        LightUniformDispatcher mDispatcher;          ///< 수집된 Light 를 모든 Program 에 일괄 uniform 송신.

        std::vector<Program*>  mActivePrograms;     ///< D-1 push -- 외부 주입 Program 집합 (rr pull 대체).
        const Framebuffer*     mLastSceneOutput = nullptr;  ///< 이번 프레임 마지막 PassComponent 출력 FB.
    };
}

#endif // __SJH_SCENE_RENDERER_H__
