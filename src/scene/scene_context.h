#ifndef __SJH_SCENE_CONTEXT_H__
#define __SJH_SCENE_CONTEXT_H__

#include <vector>

namespace SJH
{
	class DirLight;
	class PointLight;
	class SpotLight;
} // namespace SJH

namespace SJH::Scene
{
	class Camera;
} // namespace SJH::Scene

namespace SJH::Scene
{
	/// @brief Cocos2D-x v4 `Scene::_cameras` / `Scene::_lights` 정통 Aggregate Root.
	/// @details
	///   ### 책임
	///   - 씬에 등록된 Camera / Light Component 의 *비소유 raw 포인터* 컬렉션 보관
	///   - Component 의 `OnEnter` / `OnExit` 시점에 자동 push/pop (Cocos2D `addChild` 정통)
	///   - SceneRenderer 가 매 프레임 DFS traverse 하던 책임을 *등록 시점에 흡수*
	///
	///   ### 비-책임
	///   - Program / Material / Mesh 보유 — ResourceRegistry / Material owner 영역 (X)
	///   - 활성/비활성 filter — 보관은 *모두*, render-time filter 는 SceneRenderer 책임 (X)
	///   - Camera 우선순위 정렬 — `Camera::Depth` 폐기 (Cocos2D `addChild` 순서 정통) (X)
	///
	///   ### Lifetime 가정
	///   - 모든 Component 의 owner 는 Actor — Actor 의 OnEnter/OnExit 가 Component lifecycle 보장.
	class SceneContext
	{
	  public:
		// ── Camera (Cocos2D `Scene::_cameras` 정통) ───────────────────────
		void AddCamera(Camera *cam);
		void RemoveCamera(Camera *cam);
		const std::vector<Camera *> &GetCameras() const
		{
			return mCameras;
		}

		// ── DirLight 단일 슬롯 (셰이더 컨벤션: dirLight 1개) ───────────────
		void AddLight(SJH::DirLight *light);
		void RemoveLight(SJH::DirLight *light);
		SJH::DirLight *GetDirLight() const
		{
			return mDirLight;
		}

		// ── PointLight vector (max 16 = Const::MAX_POINT_LIGHTS) ──────────
		void AddLight(SJH::PointLight *light);
		void RemoveLight(SJH::PointLight *light);
		const std::vector<SJH::PointLight *> &GetPointLights() const
		{
			return mPointLights;
		}

		// ── SpotLight vector (max 16 = Const::MAX_SPOT_LIGHTS) ────────────
		void AddLight(SJH::SpotLight *light);
		void RemoveLight(SJH::SpotLight *light);
		const std::vector<SJH::SpotLight *> &GetSpotLights() const
		{
			return mSpotLights;
		}

		SceneContext();

	  private:
		std::vector<Camera *>          mCameras;
		SJH::DirLight                 *mDirLight = nullptr;
		std::vector<SJH::PointLight *> mPointLights; // reserve(Const::MAX_POINT_LIGHTS) = 16
		std::vector<SJH::SpotLight *>  mSpotLights;  // reserve(Const::MAX_SPOT_LIGHTS) = 16
	};
} // namespace SJH::Scene

#endif // __SJH_SCENE_CONTEXT_H__
