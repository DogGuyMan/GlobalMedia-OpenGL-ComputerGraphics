/**
 * @file render_stage.impls.h
 * @brief @c IRenderStage 구현체 통합 선언 - @c SceneRenderer (orchestrator) + @c ScreenQuadStage (final blit) + @c CameraStage (단일 Camera wrap).
 *
 * @details
 *  ### 거주지 (render_stage 서브폴더로 통합)
 *  세 클래스 모두 @c IRenderStage (@c render/render_stage/render_stage.h) 구현체라 한 TU 로 모았다.
 *  단일 @c sjhopengl_render 라이브러리에 소속 - 별도 라이브러리로 쪼개지 않는다:
 *  @c CameraStage 가 @c SceneRenderer 를 역참조하고 @c SceneRenderer 는 render-core
 *  (@c MeshPassProcessor 등) 에 의존하므로, 분리 라이브러리면 상호 순환이 된다.
 *  (최상위 추상 @c IRenderStage 의 선언/vtable 은 같은 폴더의 @c render_stage.{h,cpp} - 파일 그대로 유지.)
 *
 *  - @c SceneRenderer  - Actor 트리 순회 -> DrawCommand 큐 Flush 까지 한 프레임을 총괄.
 *  - @c ScreenQuadStage - N 개 FBO color attachment 를 backbuffer 에 합성하는 최종 스테이지.
 *  - @c CameraStage    - 단일 @c Camera 를 @c IRenderStage 로 래핑 (RenderWithCamera 위임).
 */
#ifndef __SJH_RENDER_STAGE_IMPLS_H__
#define __SJH_RENDER_STAGE_IMPLS_H__

#include "render/light_ubo_uploader.h"
#include "render/mesh_pass_processor.h"
#include "render/render_stage/render_stage.h"
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
     *  - @c mUploader - 수집된 Light 를 공유 LightBlock UBO 로 패킹 + Program 에 결속/loose 송신.
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
        LightUboUploader       mUploader;           ///< 수집된 Light 를 공유 LightBlock UBO 패킹 + Program 에 결속/송신 (D-LUD O4).

        std::vector<Program*>  mActivePrograms;     ///< D-1 push -- 외부 주입 Program 집합 (rr pull 대체).
        const Framebuffer*     mLastSceneOutput = nullptr;  ///< 이번 프레임 마지막 PassComponent 출력 FB.
    };

    /// @brief FBO color attachment 를 backbuffer 로 합성하는 최종 렌더 스테이지.
    /// @details
    ///   - `SetSources` 로 등록된 FBO 들을 순서대로 backbuffer 에 합성.
    ///   - 첫 소스: depth test OFF + blend OFF (replace). 2+ 소스: alpha blend ON.
    ///   - passthrough Program 의 sampler 이름 컨벤션 = `uScene` (migrate_demo SP4 정통).
    ///   - **Effekseer / Box2D VAO-EBO 오염 방어**: 매 프레임 @c ebo->Bind() 재핀.
    class ScreenQuadStage : public IRenderStage
    {
      public:
        /// @brief passthrough Program + screen quad Mesh 주입.
        /// @param passthrough  `uScene` sampler + `aPos`/`aUV` attribute 만 사용하는 셰이더 프로그램.
        /// @param screenQuad   NDC 화면 가득 덮는 quad mesh (Geometry::ScreenQuad 산출물).
        /// @note 두 인자 모두 *비소유* 참조 - ScreenQuadStage 보다 오래 살아야 한다.
        ScreenQuadStage(Program &passthrough, Mesh &screenQuad);

        /// @brief 합성 소스 FBO 목록 교체 - 매 프레임 또는 resize 시 호출.
        /// @param sources color attachment 를 backbuffer 에 합성할 FBO 포인터 목록 (순서 = 합성 순서).
        ///                각 원소는 non-null 이어야 함 (@c assert 로 검사 - skip 아님).
        void SetSources(std::vector<const Framebuffer *> sources);

        /// @brief IRenderStage - sources 를 target(backbuffer) 에 합성.
        /// @param target Application 이 보유한 DefaultRenderTarget - glBindFramebuffer(0) + viewport.
        /// @note sources 가 비어 있으면 no-op (backbuffer 변경 없음).
        void Render(RenderTarget &target) override;

        /// @brief Window resize 시 no-op (소스 FBO 크기는 호출자 책임).
        void OnResize(int /*w*/, int /*h*/) override {}

      private:
        Program &mProgram;                         ///< passthrough 셰이더 (비소유).
        Mesh &mMesh;                               ///< screen quad (비소유).
        std::vector<const Framebuffer *> mSources; ///< 합성 소스 FBO 목록 (비소유 포인터).
    };

    /**
     * @brief 단일 @c Camera 를 @c IRenderStage 로 래핑하는 구체 스테이지.
     * @details
     *  Application 이 @c mStages 에 @c CameraStage 를 원하는 순서로 push 하여 카메라 렌더 순서를
     *  *명시적으로* 제어. @c SceneContext.GetCameras() 자동 순회를 사용하지 않으므로,
     *  월드 카메라 -> 스크린 카메라 순서를 Application 이 직접 배열 가능 (Unity URP ScriptableRenderPass 정통).
     *
     * @note @c Render(target) 의 @p target 인자는 사용하지 않음 -
     *       Camera 가 자기 @c SetTargetRenderTarget() 으로 FBO/backbuffer 를 독립 지정.
     */
    class CameraStage : public IRenderStage
    {
      public:
        /// @brief 렌더러 + 카메라 주입.
        /// @param renderer 씬 렌더 위임 대상 (비소유). @c nullptr 시 @c Render 호출 무시.
        /// @param camera   렌더할 카메라 (비소유). @c nullptr 시 @c Render 호출 무시.
        CameraStage(SceneRenderer* renderer, Scene::Camera* camera);

        /// @brief @c SceneRenderer::RenderWithCamera(*mCamera) 위임.
        /// @param target 이 스테이지에서 미사용 - Camera 내부 RT 설정 우선.
        void Render(RenderTarget& target) override;

      private:
        SceneRenderer* mRenderer; ///< 씬 렌더 위임 대상 (비소유).
        Scene::Camera* mCamera;   ///< 렌더할 카메라 (비소유).
    };

} // namespace SJH

#endif // __SJH_RENDER_STAGE_IMPLS_H__
