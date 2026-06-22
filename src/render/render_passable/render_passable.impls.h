/**
 * @file render_passable.impls.h
 * @brief @c IPassable 구현체 통합 선언 - @c SceneRenderer (orchestrator) + @c ScreenQuadStage (final blit) + @c CameraStage (단일 Camera wrap).
 *
 * @details
 *  ### 거주지 (render_passable 서브폴더로 통합)
 *  세 클래스 모두 @c IPassable (@c render/render_passable/render_passable.h) 구현체라 한 TU 로 모았다.
 *  단일 @c sjhopengl_render 라이브러리에 소속 - 별도 라이브러리로 쪼개지 않는다:
 *  @c CameraStage 가 @c SceneRenderer 를 역참조하고 @c SceneRenderer 는 render-core
 *  (@c RenderableProcessor 등) 에 의존하므로, 분리 라이브러리면 상호 순환이 된다.
 *  (최상위 추상 @c IPassable 의 선언/vtable 은 같은 폴더의 @c render_passable.{h,cpp} - 파일 그대로 유지.)
 *
 *  - @c SceneRenderer  - Actor 트리 순회 -> IRenderable flat 큐 Flush 까지 한 프레임을 총괄 (Task 2.4).
 *  - @c ScreenQuadStage - N 개 FBO color attachment 를 backbuffer 에 합성하는 최종 스테이지.
 *  - @c CameraStage    - 단일 @c Camera 를 @c IPassable 로 래핑 (RenderWithCamera 위임).
 *
 *  ### 현황 (Phase 3 진행) - 신규 역할 Pass(keeper) + dead 전이 클래스 혼재
 *  신규(keeper): @c WorldPass(3.1) / @c SkyboxPass(3.2) / @c PostFxPass(3.5a per-effect) / @c ScreenQuadStage(present).
 *  dead(미사용, 삭제 후보): @c SceneRenderer / @c CameraStage. grep: [DEAD-PHASE5].
 *
 *  | 클래스 | 상태 | 비고 |
 *  |--------|------|------|
 *  | @c WorldPass / @c SkyboxPass | keeper | 역할 Pass (worldCam / skybox) |
 *  | @c PostFxPass      | keeper | per-effect PostFX blit (3.5a) |
 *  | @c ScreenQuadStage | keeper(present) | 최종 FBO -> backbuffer 합성. Phase 5 에서 PresentPass 개명 검토 |
 *  | @c SceneRenderer   | [DEAD-PHASE5] | world->WorldPass, screen->PostFxPass 이관 완료. 호출처 0 |
 *  | @c CameraStage     | [DEAD-PHASE5] | worldCam/screenCam 대체됨. 사용자 0 |
 *
 *  삭제는 *모든 Task 종료 후 Phase 5* 판정 - dead 클래스 + @c GameSystems::mScenesRender 제거, 이 파일을 keeper Pass 로 재구성.
 */
#ifndef __SJH_RENDER_PASSABLE_IMPLS_H__
#define __SJH_RENDER_PASSABLE_IMPLS_H__

#include "render/light_ubo_uploader.h"
#include "render/mesh_pass_processor.h"
#include "render/render_passable/render_passable.h"
#include <cstdint>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

namespace SJH::Scene { class Actor; class Camera; class PassComponent; }
namespace SJH { class RenderTarget; class Framebuffer; class Mesh; class Material; class Program; class Texture; class DeviceContext; }

namespace SJH
{
    /**
     * @brief 단일 World Camera 의 씬을 자기완결로 렌더하는 v5 역할 Pass (D5/D8).
     * @details
     *  구 @c SceneRenderer::RenderWithCamera(worldCam) + @c CameraStage(worldCam) 의 책임을
     *  하나로 합친 자기완결 Pass. 자체 @c RenderableProcessor + @c LightUboUploader 를 보유한다.
     *  - per-frame: 카메라 RT bind -> 광원 수집 + LightBlock UBO 업로드 -> Actor 트리에서
     *    MeshRenderer 만 Submit -> Sort -> Process(IRenderable 잎 위임).
     *  - PassComponent(ScreenQuad/PostFX)는 수집하지 않는다 - Task 3.5 @c PostFxPass 담당.
     *  - Skybox 분리(Task 3.2) 전까지 skybox 는 큐(2500)로 본 Pass 안에서 그려진다(정상).
     * @note SceneRenderer/CameraStage 대체용 - 신규 코드는 이 클래스를 사용. mScreenQuadMesh 미설정이라
     *       (구 RenderableProcessor 의) screen 경로는 자동 skip - 월드 메시만 그린다.
     */
    class WorldPass : public IPassable
    {
      public:
        /// @brief 렌더할 World Camera 주입(비소유). nullptr 시 Draw 는 warn+skip.
        explicit WorldPass(Scene::Camera *cam) : mCamera(cam) {}

        /// @brief 카메라 RT 에 월드 메시 렌더(구 RenderWithCamera 본문 자기완결 이식).
        void Draw(DeviceContext &rec, const Texture *before) override;

        /// @brief 이 Pass 의 출력 = 카메라 RT 의 color attachment (다음 Pass 의 before 입력, D6). 없으면 nullptr.
        const Texture *GetPassResult() const override;

        /// @brief 라이트 uniform 송신 대상 Program 집합을 외부에서 주입(D-1 push). 매 프레임 main 이 호출.
        /// @param programs 활성 Program 포인터 스냅샷(owner=caller, 본 클래스는 복사 보유).
        void SetActivePrograms(std::vector<Program *> programs) { mActivePrograms = std::move(programs); }

        /// @brief false 면 Draw 가 BeginFrame(clear) 대신 BindTarget - 선행 SkyboxPass 가 sceneFB clear 책임(background-first).
        /// @return *this - fluent Builder(생성 직후 설정+포인터 캡처 체이닝용). 프로젝트 Setter 컨벤션(Material::SetPass 등).
        WorldPass &SetClearsTarget(bool v) { mClearsTarget = v; return *this; }

      private:
        /// @brief Actor 트리 DFS - 활성 + 가시(cullingMask) 노드의 MeshRenderer 만 Submit(IRenderable*).
        void CollectFromActor(const Scene::Actor &actor, const glm::mat4 &viewMat, uint64_t cullingMask);

        RenderableProcessor   mProc;            ///< IRenderable flat 큐 보유/정렬/draw 발행.
        LightUboUploader      mUploader;        ///< 광원 -> 공유 LightBlock UBO 패킹 + Program 결속.
        std::vector<Program *> mActivePrograms; ///< D-1 push - 외부 주입 Program 집합.
        Scene::Camera        *mCamera = nullptr;///< 렌더 대상 World Camera(비소유).
        bool mClearsTarget = true; ///< false 면 clear 스킵(선행 SkyboxPass 가 clear). 기본 true=standalone 안전.
    };

    /**
     * @brief skybox 를 sceneFB 에 *맨 먼저* clear+그리는 background-first 역할 Pass (D5/D8, Task 3.2).
     * @details
     *  구 WorldPass 의 queue-2500 skybox draw 를 독립 Pass 로 분리. 동작:
     *  - worldCam RT(=sceneFB)에 @c BeginFrame(clear color+depth+stencil) -> sceneFB clear 책임 인수.
     *  - skybox ROP 적용 후 skybox IRenderable 의 자가발행 Render(구 queue-2500 leaf 와 동일 호출) -> 픽셀 동일.
     *  - skybox 는 DepthWrite off + z=1.0(.xyww) 라 화면을 배경으로 채우고, 후행 WorldPass 가 위에 그린다.
     * @note WorldPass 는 @c SetClearsTarget(false) 로 clear 를 스킵하고 skybox 를 재수집하지 않는다(중복 방지).
     */
    class SkyboxPass : public IPassable
    {
      public:
        /// @param skybox skybox MeshRenderer(IRenderable, 비소유). nullptr 시 Draw warn+skip.
        /// @param cam    sceneFB(RT) + view/proj 출처 World Camera(비소유). nullptr 시 warn+skip.
        SkyboxPass(IRenderable *skybox, Scene::Camera *cam) : mSkybox(skybox), mCamera(cam) {}

        /// @brief sceneFB clear + skybox draw(background-first).
        void Draw(DeviceContext &rec, const Texture *before) override;

        /// @brief 이 Pass 의 출력 = sceneFB color attachment(공유 RT, D6). 없으면 nullptr.
        const Texture *GetPassResult() const override;

      private:
        IRenderable   *mSkybox = nullptr; ///< skybox MeshRenderer(IRenderable, 비소유).
        Scene::Camera *mCamera = nullptr; ///< sceneFB(RT)+view/proj 출처(비소유).
    };

    /**
     * @brief High-level Orchestrator - SceneContext Camera 컬렉션 순회 + 1패스 렌더 위임.
     * @deprecated [DEAD-PHASE5] 완전 미사용(dead) - world 는 @c WorldPass(3.1), screen/PostFX 는
     *             per-effect @c PostFxPass(3.5a) 가 흡수 완료. screenCam CameraStage 제거(3.5a)로 호출처 0.
     *             삭제는 *모든 Task 종료 후 Phase 5* 판정(@c GameSystems::mScenesRender 멤버/accessor 동반 제거).
     *             신규 코드에서 절대 의존 금지. grep: [DEAD-PHASE5].
     * @details
     *  per-frame 호출 흐름 (multi-pass):
     *  1. @c SceneContext::GetCameras() 직접 조회 - DFS 폐기 (Cocos2D @c Scene::_cameras 정통).
     *  2. @c addCamera 호출 순서 그대로 렌더 (@c std::sort 폐기) - Cocos2D @c addChild 정통.
     *  3. 각 Camera 마다 @c RenderWithCamera 1패스:
     *     - @c Camera::GetTargetRenderTarget() 강제 - nullptr 이면 warn+skip.
     *     - @c DeviceContext::BeginFrame(target) + Actor 트리 MeshRenderer/PassComponent 수집 + 큐 Flush.
     *
     *  내부 멤버:
     *  - @c mProcessor - IRenderable flat 큐 보유/정렬/GL draw 발행 (Task 2.4 RenderableProcessor).
     *  - @c mUploader - 수집된 Light 를 공유 LightBlock UBO 로 패킹 + Program 에 결속/loose 송신.
     *  - @c mLastSceneOutput - 이번 프레임 PassComponent 체인의 마지막 출력 FB 추적.
     */
    class SceneRenderer : public IPassable
    {
    public:
        /// @brief SceneContext 의 Camera 컬렉션을 순회하며 직렬 렌더 후 PassComponent 체인 실행.
        /// @param defaultTarget IPassable 인터페이스 준수용 파라미터.
        ///        내부에서는 각 Camera 의 전용 RenderTarget 만 사용 - 이 값은 무시된다.
        /// @deprecated CameraStage + IPassable 스테이지 컬렉션 사용 권장 (4-엔진 정통).
        [[deprecated("Use CameraStage + IPassable stages 컬렉션 - Application 이 출차 책임 (4-엔진 정통)")]]
        void Draw(DeviceContext& rec, const Texture* before) override;
        /// @brief 이 Pass 의 출력 - 이번 단계 nullptr (Phase 3.1 WorldPass 에서 정련).
        const Texture *GetPassResult() const override { return nullptr; }

        /// @brief ScreenQuad mesh 지정 - PassComponent ScreenQuad 처리 시 사용.
        /// @param mesh nullptr 이면 ScreenQuad 처리가 skip 됨.
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
        /// @brief Actor 트리 DFS - 활성 노드에서 MeshRenderer(IRenderable*)/PassComponent 를 Submit/SubmitScreenQuad 로 변환.
        /// @param actor      현재 순회 노드.
        /// @param viewMat    이번 패스의 View 행렬 (depth 계산용).
        /// @param cullingMask 카메라 CullingMask - Actor layer 비트와 AND 로 가시성 판정.
        void CollectFromActor(const Scene::Actor& actor, const glm::mat4& viewMat, uint64_t cullingMask);

        RenderableProcessor    mProcessor;          ///< IRenderable flat 큐 보유/정렬/GL draw 발행 (Task 2.4).
        LightUboUploader       mUploader;           ///< 수집된 Light 를 공유 LightBlock UBO 패킹 + Program 에 결속/송신 (D-LUD O4).

        std::vector<Program*>  mActivePrograms;     ///< D-1 push -- 외부 주입 Program 집합 (rr pull 대체).
        const Framebuffer*     mLastSceneOutput = nullptr;  ///< 이번 프레임 마지막 PassComponent 출력 FB.
    };

    /**
     * @brief 단일 PostFX 효과(셰이더 1개)를 InputFB -> OutputFB 로 blit 하는 per-effect 역할 Pass (Task 3.5).
     * @details
     *  구 RenderableProcessor screen 루프의 *한 단계* 를 독립 Pass 로 분리 - PassComponent 하나당 PostFxPass 하나.
     *  - 입력: 현재는 @c pc->InputFB (BuildPostFXChain 사전배선). 향후 PassIterator(D2) 가 @p before 로 override 가능.
     *  - 출력: @c pc->OutputFB. @c GetPassResult() 가 그 color attachment 반환(다음 Pass 의 before 입력 - D2 hook).
     *  - @c PassComponent::Enabled=false 면 bypass(passthrough) blit (구 동작 보존).
     * @note 최종 backbuffer 합성은 @c ScreenQuadStage(present) 가 담당 - 본 Pass 는 FBO 간 효과 blit 만.
     */
    class PostFxPass : public IPassable
    {
      public:
        /// @param pc     이 Pass 가 그릴 PostFX 단계(비소유). InputFB/OutputFB/material/Enabled 보유.
        /// @param quad   풀스크린 blit 용 screen quad mesh(비소유).
        /// @param bypass disabled 효과 passthrough material(비소유).
        PostFxPass(Scene::PassComponent *pc, Mesh *quad, Material *bypass)
            : mPc(pc), mQuad(quad), mBypass(bypass) {}

        /// @brief InputFB -> OutputFB blit (Enabled?effect:bypass). 구 RenderableProcessor screen 루프 1단계 이식.
        void Draw(DeviceContext &rec, const Texture *before) override;

        /// @brief 이 효과의 출력 = OutputFB color attachment (다음 Pass 의 before 입력, D2). 없으면 nullptr.
        const Texture *GetPassResult() const override;

      private:
        Scene::PassComponent *mPc     = nullptr; ///< PostFX 단계 배선(비소유).
        Mesh                 *mQuad   = nullptr; ///< 풀스크린 blit mesh(비소유).
        Material             *mBypass = nullptr; ///< disabled passthrough material(비소유).
    };

    /// @brief FBO color attachment 를 backbuffer 로 합성하는 최종 렌더 스테이지.
    /// @note [3.5 정정] per-effect 효과 blit 은 @c PostFxPass 가 담당하고, 본 클래스는 *최종 present*
    ///       (체인 마지막 FBO -> backbuffer 합성) 역할로 유지된다. Phase 5 에서 PresentPass 로 개명 검토.
    /// @details
    ///   - `SetSources` 로 등록된 FBO 들을 순서대로 backbuffer 에 합성.
    ///   - 첫 소스: depth test OFF + blend OFF (replace). 2+ 소스: alpha blend ON.
    ///   - passthrough Program 의 sampler 이름 컨벤션 = `uScene` (migrate_demo SP4 정통).
    ///   - **Effekseer / Box2D VAO-EBO 오염 방어**: 매 프레임 @c ebo->Bind() 재핀.
    class ScreenQuadStage : public IPassable
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

        /// @brief IPassable - sources 를 mBackbuffer 에 합성.
        /// @note sources 가 비어 있으면 no-op (backbuffer 변경 없음).
        void Draw(DeviceContext& rec, const Texture* before) override;
        /// @brief 이 Pass 의 출력 - backbuffer 직접 출력이라 텍스처 결과 없음.
        const Texture *GetPassResult() const override { return nullptr; }
        /// @brief 최종 출력 backbuffer(=Application DefaultRenderTarget) 주입. resize 마다 재생성되므로 매 프레임 주입 권장.
        void SetBackbuffer(RenderTarget *bb) { mBackbuffer = bb; }

        /// @brief Window resize 시 no-op (소스 FBO 크기는 호출자 책임).
        void OnResize(int /*w*/, int /*h*/) override {}

      private:
        Program &mProgram;                         ///< passthrough 셰이더 (비소유).
        Mesh &mMesh;                               ///< screen quad (비소유).
        std::vector<const Framebuffer *> mSources; ///< 합성 소스 FBO 목록 (비소유 포인터).
        RenderTarget *mBackbuffer = nullptr;        ///< 최종 출력 backbuffer (비소유, main 이 매 프레임 주입).
    };

    /**
     * @brief 단일 @c Camera 를 @c IPassable 로 래핑하는 구체 스테이지.
     * @deprecated [DEAD-PHASE5] 완전 미사용(dead) - worldCam 은 @c WorldPass(3.1), screenCam 은
     *             per-effect @c PostFxPass(3.5a) 로 대체되어 사용자 0. 삭제는 모든 Task 종료 후 Phase 5 판정.
     *             신규 코드에서 절대 의존 금지. grep: [DEAD-PHASE5].
     * @details
     *  Application 이 @c mStages 에 @c CameraStage 를 원하는 순서로 push 하여 카메라 렌더 순서를
     *  *명시적으로* 제어. @c SceneContext.GetCameras() 자동 순회를 사용하지 않으므로,
     *  월드 카메라 -> 스크린 카메라 순서를 Application 이 직접 배열 가능 (Unity URP ScriptableRenderPass 정통).
     *
     * @note @c Draw(rec, before) 의 두 파라미터는 이 단계 미사용 -
     *       Camera 가 자기 @c SetTargetRenderTarget() 으로 FBO/backbuffer 를 독립 지정.
     */
    class CameraStage : public IPassable
    {
      public:
        /// @brief 렌더러 + 카메라 주입.
        /// @param renderer 씬 렌더 위임 대상 (비소유). @c nullptr 시 @c Draw 호출 무시.
        /// @param camera   렌더할 카메라 (비소유). @c nullptr 시 @c Draw 호출 무시.
        CameraStage(SceneRenderer* renderer, Scene::Camera* camera);

        /// @brief @c SceneRenderer::RenderWithCamera(*mCamera) 위임.
        void Draw(DeviceContext& rec, const Texture* before) override;
        /// @brief 이 Pass 의 출력 - 이번 단계 nullptr (Phase 3.1 정련).
        const Texture *GetPassResult() const override { return nullptr; }

      private:
        SceneRenderer* mRenderer; ///< 씬 렌더 위임 대상 (비소유).
        Scene::Camera* mCamera;   ///< 렌더할 카메라 (비소유).
    };

} // namespace SJH

#endif // __SJH_RENDER_PASSABLE_IMPLS_H__
