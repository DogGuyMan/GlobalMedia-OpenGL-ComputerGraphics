/**
 * @file camera_stage.cpp
 * @brief CameraStage 구현 - SceneRenderer::RenderWithCamera 단순 위임.
 *
 * @details
 *  ### 책임
 *  - @c Render(target) 호출 시 @c mRenderer / @c mCamera nullptr 가드 후
 *    @c SceneRenderer::RenderWithCamera(*mCamera) 위임.
 *
 *  ### 비-책임
 *  - [X] GL 상태 직접 조작 - SceneRenderer / Camera / DeviceContext 가 담당.
 *  - [X] @c RenderTarget 바인딩 - Camera 가 자기 @c SetTargetRenderTarget() 으로 처리.
 */
#include "render/camera_stage.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include <spdlog/spdlog.h>

namespace SJH
{
	CameraStage::CameraStage(SceneRenderer* renderer, Scene::Camera* camera)
	    : mRenderer(renderer), mCamera(camera)
	{
	}

	void CameraStage::Render(RenderTarget& /*target*/)
	{
		if (!mRenderer || !mCamera)
		{
			spdlog::warn("CameraStage::Render - renderer/camera nullptr - skip.");
			return;
		}
		mRenderer->RenderWithCamera(*mCamera);
	}
}
