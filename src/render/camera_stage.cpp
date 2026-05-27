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
			spdlog::warn("CameraStage::Render — renderer/camera nullptr — skip.");
			return;
		}
		mRenderer->RenderWithCamera(*mCamera);
	}
}
