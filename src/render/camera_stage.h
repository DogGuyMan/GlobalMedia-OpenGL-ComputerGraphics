/**
 * @file camera_stage.h
 * @brief 단일 Camera 를 IRenderStage 로 래핑하는 구체 스테이지.
 *
 * @details
 *  ### 책임
 *  - @c IRenderStage::Render() 호출 시 @c SceneRenderer::RenderWithCamera(*mCamera) 위임.
 *  - Application 이 카메라 명시 순서를 결정 - Unity URP @c ScriptableRenderPass 상속 표현 정통.
 *
 *  ### 비-책임
 *  - [X] @c SceneContext.GetCameras() 자동 순회 - 순서 제어는 Application 의 @c mStages 벡터.
 *  - [X] @c RenderTarget 직접 사용 - Camera 가 자신의 @c SetTargetRenderTarget() 으로 FBO 지정.
 *
 * @note @c target 인자는 이 스테이지에서 사용하지 않음. Camera 내부의 FBO/backbuffer 설정이 우선.
 */
#ifndef __SJH_CAMERA_STAGE_H__
#define __SJH_CAMERA_STAGE_H__

#include "render/render_stage.h"

namespace SJH::Scene { class Camera; }

namespace SJH
{
	class SceneRenderer;

	/**
	 * @brief 단일 @c Camera 를 @c IRenderStage 로 래핑하는 구체 스테이지.
	 * @details
	 *  Application 이 @c mStages 에 @c CameraStage 를 원하는 순서로 push 하여
	 *  카메라 렌더 순서를 *명시적으로* 제어.
	 *  @c SceneContext.GetCameras() 자동 순회를 사용하지 않으므로,
	 *  월드 카메라 -> 스크린 카메라 순서를 Application 이 직접 배열 가능.
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
}

#endif // __SJH_CAMERA_STAGE_H__
