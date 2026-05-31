#include <GL/gl3w.h>  // gl3w 반드시 최우선 — Effekseer 헤더보다 먼저 GL 타입 정의

#include "VFX/ParticleStage.h"

#include "VFX/VFXSystem.h"
#include "render/device_context.h"
#include "render/render_target.h"
#include "scene/camera.h"

#include <spdlog/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::VFX
{
	ParticleStage::ParticleStage(VFXSystem* vfx, SJH::Scene::Camera* worldCam)
	    : mVFX(vfx), mWorldCam(worldCam)
	{
	}

	void ParticleStage::Render(SJH::RenderTarget& /*target*/)
	{
		if (!mVFX || !mWorldCam)
		{
			spdlog::warn("ParticleStage::Render — vfx/worldCam nullptr — skip.");
			return;
		}

		// 진실의 원천 단일화 — worldCam 의 RT 가 sceneFB (D-1).
		auto* rt = mWorldCam->GetTargetRenderTarget();
		if (!rt)
		{
			spdlog::warn("ParticleStage::Render — worldCam.GetTargetRenderTarget() nullptr — skip.");
			return;
		}

		// sceneFB bind (NoClear — WorldCamera 가 이미 그린 결과 보존).
		SJH::DeviceContext::Get().BindTarget(*rt);

		// Effekseer 자체 GL state setup + Draw (D-2 — state 명시 set 하지 않음).
		const vmath::mat4 view = mWorldCam->GetViewMatrix();
		const vmath::mat4 proj = mWorldCam->GetProjectionMatrix();
		mVFX->Draw(&view[0][0], &proj[0][0]);
	}
}
