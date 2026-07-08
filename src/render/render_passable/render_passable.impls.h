/**
 * @file render_passable.impls.h
 * @brief @c IPassable 구현체 통합 - @c WorldPass / @c SkyboxPass / @c PostFxPass(per-effect) / @c ScreenQuadStage(present).
 *
 * @details
 *  ### 거주지 (render_passable 서브폴더로 통합)
 *  역할 Pass 들이 한 TU 에 모여 render-core 의존을 공유한다.
 *  단일 @c sjhopengl_render 라이브러리에 소속 - 별도 라이브러리로 쪼개지 않는다.
 *  (최상위 추상 @c IPassable 의 선언/vtable 은 같은 폴더의 @c render_passable.{h,cpp} - 파일 그대로 유지.)
 *
 *  | 클래스 | 상태 | 비고 |
 *  |--------|------|------|
 *  | @c WorldPass / @c SkyboxPass | keeper | 역할 Pass (worldCam / skybox) |
 *  | @c PostFxPass      | keeper | per-effect PostFX blit (3.5a) |
 *  | @c ScreenQuadStage | keeper(present) | 최종 FBO -> backbuffer 합성. Phase 5 에서 PresentPass 개명 검토 |
 */
#ifndef __SJH_RENDER_PASSABLE_IMPLS_H__
#define __SJH_RENDER_PASSABLE_IMPLS_H__

#include "render/light_ubo_uploader.h"
#include "render/renderable_processor.h"
#include "render/render_passable/render_passable.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

namespace SJH::Scene { class Actor; class Camera; }
namespace SJH { class RenderTarget; class RenderTexture; class Mesh; class Material; class Program; class Texture; class DeviceContext; }

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

        /// @brief Pass 조회 키(PassIterator::Find) - 고정 역할명.
        std::string GetPassKey() const override { return "World"; }

        /// @brief 라이트 uniform 송신 대상 Program 집합을 외부에서 주입(D-1 push). 매 프레임 main 이 호출.
        /// @param programs 활성 Program 포인터 스냅샷(owner=caller, 본 클래스는 복사 보유).
        void SetActivePrograms(std::vector<Program *> programs) { mActivePrograms = std::move(programs); }

        /// @brief false 면 Draw 가 BeginFrame(clear) 대신 BindTarget - 선행 SkyboxPass 가 sceneFB clear 책임(background-first).
        /// @return *this - fluent Builder(생성 직후 설정+포인터 캡처 체이닝용). 프로젝트 Setter 컨벤션(Material::SetPass 등).
        WorldPass &SetClearsTarget(bool v) { mClearsTarget = v; return *this; }

        /// @brief World draw 를 queueLayer [min,max) 로 제한 - mProc 에 전달(골든 캡처 격리용, 기본 무영향).
        void SetQueueFilter(int minQueue, int maxQueue);

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

        /// @brief Pass 조회 키(PassIterator::Find) - 고정 역할명.
        std::string GetPassKey() const override { return "Skybox"; }

      private:
        IRenderable   *mSkybox = nullptr; ///< skybox MeshRenderer(IRenderable, 비소유).
        Scene::Camera *mCamera = nullptr; ///< sceneFB(RT)+view/proj 출처(비소유).
    };

    /**
     * @brief 단일 PostFX 효과를 before(입력) -> output(결과 FBO) 로 blit 하는 per-effect 역할 Pass.
     * @details
     *  PassComponent 의존 제거(2026-06-24 모듈 제거). 4필드(InputFB/OutputFB/material/Enabled)를 분해:
     *  - 입력: @p before (PassIterator 가 흘려주는 직전 활성 Pass 결과 텍스처, design B).
     *  - 출력: @c mOutput (비소유 FBO). nullptr 이면 @c mBackbuffer 합성(present, DB-1).
     *  - Enabled 토글: @c IPassable::Enabled (PassIterator 가 skip - 본 Pass 내부 분기 아님).
     * @note 최종 backbuffer 합성은 output=nullptr 인 본 Pass 인스턴스(present)가 흡수 - ScreenQuadStage 폐기.
     */
    class PostFxPass : public IPassable
    {
      public:
        /// @param effect 효과 머티리얼(uScene sampler + 셰이더, 비소유). nullptr 시 Draw skip.
        /// @param output 결과 FBO(비소유). nullptr 이면 backbuffer 합성(present).
        /// @param quad   풀스크린 blit 용 screen quad mesh(비소유).
        /// @param key    PassIterator::Find 조회 키(효과명, 예: "grayscale_vignetting"). 토글/디버그 식별용.
        PostFxPass(Material *effect, RenderTexture *output, Mesh *quad, std::string key)
            : mEffect(effect), mOutput(output), mQuad(quad), mKey(std::move(key)) {}

        /// @brief before -> output blit. output=nullptr 이면 backbuffer present.
        void Draw(DeviceContext &rec, const Texture *before) override;

        /// @brief 이 효과의 출력 = output color attachment (다음 Pass 의 before 입력, D6). present(nullptr)는 결과 없음.
        const Texture *GetPassResult() const override;

        /// @brief Pass 조회 키(PassIterator::Find) - 생성 시 주입한 효과명.
        std::string GetPassKey() const override { return mKey; }

        /// @brief output==nullptr(present) 일 때 backbuffer 주입. resize 마다 재생성 -> 매 프레임 주입 권장.
        void SetBackbuffer(RenderTarget *bb) { mBackbuffer = bb; }

      private:
        Material     *mEffect     = nullptr; ///< 효과 머티리얼(비소유).
        RenderTexture  *mOutput     = nullptr; ///< 결과 FBO(비소유). nullptr=backbuffer present.
        Mesh         *mQuad       = nullptr; ///< 풀스크린 blit mesh(비소유).
        RenderTarget *mBackbuffer = nullptr; ///< present 시 출력 backbuffer(비소유, main 매 프레임 주입).
        std::string   mKey;                  ///< PassIterator::Find 조회 키(효과명).
    };

} // namespace SJH

#endif // __SJH_RENDER_PASSABLE_IMPLS_H__
