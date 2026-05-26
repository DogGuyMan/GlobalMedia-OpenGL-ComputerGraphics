#include "scene/scene_context.h"
#include "common/constants.h"
#include "object/light.h"
#include "scene/camera.h"
#include <algorithm>
#include <cassert>
#include <spdlog/spdlog.h>

namespace SJH::Scene
{
	SceneContext::SceneContext()
	{
		mPointLights.reserve(static_cast<std::size_t>(Const::MAX_POINT_LIGHTS));
		mSpotLights.reserve(static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS));
	}

	// ── Camera ────────────────────────────────────────────────────────────
	void SceneContext::AddCamera(Camera *cam)
	{
		assert(cam && "SceneContext::AddCamera — nullptr");
		assert(std::find(mCameras.begin(), mCameras.end(), cam) == mCameras.end()
			   && "SceneContext::AddCamera — 중복 등록 (Component::OnEnter contract 위반)");
		mCameras.push_back(cam);
	}

	void SceneContext::RemoveCamera(Camera *cam)
	{
		mCameras.erase(std::remove(mCameras.begin(), mCameras.end(), cam), mCameras.end());
	}

	// ── DirLight (단일 슬롯) ──────────────────────────────────────────────
	void SceneContext::AddLight(SJH::DirLight *light)
	{
		assert(light && "SceneContext::AddLight(DirLight) — nullptr");
		if (mDirLight != nullptr)
		{
			spdlog::warn("SceneContext::AddLight(DirLight) — 이미 등록됨. 추가 등록 거부 (셰이더 dirLight 단일).");
			return;
		}
		mDirLight = light;
	}

	void SceneContext::RemoveLight(SJH::DirLight *light)
	{
		if (mDirLight == light)
			mDirLight = nullptr;
	}

	// ── PointLight (vector, max 16) ──────────────────────────────────────
	void SceneContext::AddLight(SJH::PointLight *light)
	{
		assert(light && "SceneContext::AddLight(PointLight) — nullptr");
		if (mPointLights.size() >= static_cast<std::size_t>(Const::MAX_POINT_LIGHTS))
		{
			spdlog::warn("SceneContext::AddLight(PointLight) — MAX_POINT_LIGHTS={} 초과. 등록 거부.",
						 Const::MAX_POINT_LIGHTS);
			return;
		}
		mPointLights.push_back(light);
	}

	void SceneContext::RemoveLight(SJH::PointLight *light)
	{
		mPointLights.erase(std::remove(mPointLights.begin(), mPointLights.end(), light),
						   mPointLights.end());
	}

	// ── SpotLight (vector, max 16) ───────────────────────────────────────
	void SceneContext::AddLight(SJH::SpotLight *light)
	{
		assert(light && "SceneContext::AddLight(SpotLight) — nullptr");
		if (mSpotLights.size() >= static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS))
		{
			spdlog::warn("SceneContext::AddLight(SpotLight) — MAX_SPOT_LIGHTS={} 초과. 등록 거부.",
						 Const::MAX_SPOT_LIGHTS);
			return;
		}
		mSpotLights.push_back(light);
	}

	void SceneContext::RemoveLight(SJH::SpotLight *light)
	{
		mSpotLights.erase(std::remove(mSpotLights.begin(), mSpotLights.end(), light),
						  mSpotLights.end());
	}
} // namespace SJH::Scene
