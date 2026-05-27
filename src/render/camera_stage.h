#ifndef __SJH_CAMERA_STAGE_H__
#define __SJH_CAMERA_STAGE_H__

#include "render/render_stage.h"

namespace SJH::Scene { class Camera; }

namespace SJH
{
	class SceneRenderer;

	/// @brief 단일 Camera 를 IRenderStage 로 래핑.
	/// @details Application 이 카메라 명시 순서를 결정 — Unity URP `ScriptableRenderPass`
	///          상속 표현 정통. SceneContext.GetCameras() 자동 순회를 사용하지 않는다.
	/// @note    target 인자는 사용하지 않음 — Camera 가 자기 SetTargetRenderTarget() 사용.
	class CameraStage : public IRenderStage
	{
	  public:
		/// @param renderer 위임 대상 (비소유). nullptr 시 Render 호출 무시.
		/// @param camera   렌더할 카메라 (비소유). nullptr 시 Render 호출 무시.
		CameraStage(SceneRenderer* renderer, Scene::Camera* camera);

		/// @brief SceneRenderer::RenderWithCamera(*mCamera) 위임.
		void Render(RenderTarget& target) override;

	  private:
		SceneRenderer* mRenderer;
		Scene::Camera* mCamera;
	};
}

#endif // __SJH_CAMERA_STAGE_H__
