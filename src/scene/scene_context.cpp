/**
 * @file scene_context.cpp
 * @brief @c SceneContext Camera / Light 컬렉션 등록/해제 구현.
 *
 * @details
 *  ### 책임
 *  - @c AddCamera / @c RemoveCamera - Camera 컬렉션 push/erase.
 *  - @c AddLight(DirLight*) - 단일 슬롯 등록 (중복 시 spdlog::warn 후 거부).
 *  - @c AddLight(PointLight*) / @c AddLight(SpotLight*) - 벡터 등록 (상한 초과 시 warn 후 거부).
 *  - 생성자 - @c mPointLights / @c mSpotLights 를 @c Const::MAX_*_LIGHTS 로 @c reserve.
 *
 *  ### 비-책임
 *  - [X] Component 소유권 - 모두 비소유 raw 포인터. 수명은 @c Actor 가 보장.
 *
 * @note @c AddCamera 중복 등록은 @c assert (Component::OnEnter contract 위반 = 버그).
 *       @c AddLight 한도 초과는 @c spdlog::warn (런타임 경고 - 무시하고 계속 동작).
 */
#include "scene/scene_context.h"
#include "common/constants.h"
#include "scene/light.h"
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

	// -- Camera ------------------------------------------------------------
	void SceneContext::AddCamera(Camera *cam)
	{
		assert(cam && "SceneContext::AddCamera - nullptr");
		assert(std::find(mCameras.begin(), mCameras.end(), cam) == mCameras.end()
			   && "SceneContext::AddCamera - 중복 등록 (Component::OnEnter contract 위반)");
		mCameras.push_back(cam);
	}

	void SceneContext::RemoveCamera(Camera *cam)
	{
		mCameras.erase(std::remove(mCameras.begin(), mCameras.end(), cam), mCameras.end());
	}

	// -- DirLight (단일 슬롯) ----------------------------------------------
	void SceneContext::AddLight(SJH::DirLight *light)
	{
		assert(light && "SceneContext::AddLight(DirLight) - nullptr");
		if (mDirLight != nullptr)
		{
			spdlog::warn("SceneContext::AddLight(DirLight) - 이미 등록됨. 추가 등록 거부 (셰이더 dirLight 단일).");
			return;
		}
		mDirLight = light;
	}

	void SceneContext::RemoveLight(SJH::DirLight *light)
	{
		if (mDirLight == light)
			mDirLight = nullptr;
	}

	// -- PointLight (vector, max 16) --------------------------------------
	void SceneContext::AddLight(SJH::PointLight *light)
	{
		assert(light && "SceneContext::AddLight(PointLight) - nullptr");
		if (mPointLights.size() >= static_cast<std::size_t>(Const::MAX_POINT_LIGHTS))
		{
			spdlog::warn("SceneContext::AddLight(PointLight) - MAX_POINT_LIGHTS={} 초과. 등록 거부.",
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

	// -- SpotLight (vector, max 16) ---------------------------------------
	void SceneContext::AddLight(SJH::SpotLight *light)
	{
		assert(light && "SceneContext::AddLight(SpotLight) - nullptr");
		if (mSpotLights.size() >= static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS))
		{
			spdlog::warn("SceneContext::AddLight(SpotLight) - MAX_SPOT_LIGHTS={} 초과. 등록 거부.",
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
