/**
 * @file scene_context.h
 * @brief Camera / Light Component 의 비소유 컬렉션 Aggregate Root - @c SceneContext.
 *
 * @details
 *  ### 책임
 *  - 씬에 활성된 @c Camera / @c DirLight / @c PointLight / @c SpotLight 의
 *    *비소유 raw 포인터* 컬렉션 보관.
 *  - @c Component::OnEnter / @c OnExit 시점에 자동 push/pop
 *    (SP-SceneContext+ProgramRegistry 2026-05-26 - Cocos2D @c addChild 정통).
 *  - @c SceneRenderer 가 매 프레임 DFS traverse 하던 책임을 *등록 시점에 흡수*.
 *
 *  ### 비-책임
 *  - [X] Program / Material / Mesh 보유 - @c ResourceRegistry / Material owner 영역.
 *  - [X] 활성/비활성 filter - 모두 보관, render-time filter 는 @c SceneRenderer 책임.
 *  - [X] Camera 우선순위 정렬 - @c Camera::Depth 폐기, @c addChild 순서 정통.
 *
 * @note @c DirLight 는 셰이더 @c dirLight 단일 uniform 컨벤션으로 *단일 슬롯*. 두 번째
 *       @c AddLight(DirLight*) 호출은 경고 후 거부됨 (@c scene_context.cpp).
 */

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
	/**
	 * @brief Cocos2D-x v4 @c Scene::_cameras / @c Scene::_lights 정통 Aggregate Root.
	 * @details
	 *  ### Lifetime 가정
	 *  모든 Component 의 owner 는 @c Actor - @c Actor 의 @c OnEnter / @c OnExit 가
	 *  Component lifecycle 을 보장. @c SceneContext 가 보관하는 raw 포인터는 항상 유효.
	 *
	 *  ### DirLight 단일 슬롯 컨벤션
	 *  셰이더 @c dirLight uniform 이 단일이므로 두 번째 @c AddLight(DirLight*) 는 경고 후 거부.
	 *
	 *  ### PointLight / SpotLight 상한
	 *  @c Const::MAX_POINT_LIGHTS = 16, @c Const::MAX_SPOT_LIGHTS = 16 초과 등록 시 경고 후 거부.
	 */
	class SceneContext
	{
	  public:
		// -- Camera (Cocos2D `Scene::_cameras` 정통) -----------------------
		/// @brief Camera Component 를 컬렉션에 등록. 중복 등록 시 assert.
		void AddCamera(Camera *cam);
		/// @brief Camera Component 를 컬렉션에서 제거.
		void RemoveCamera(Camera *cam);
		/// @brief 등록된 Camera 컬렉션 (addChild 순서 = 렌더 순서). 읽기 전용.
		const std::vector<Camera *> &GetCameras() const
		{
			return mCameras;
		}

		// -- DirLight 단일 슬롯 (셰이더 컨벤션: dirLight 1개) ---------------
		/// @brief DirLight Component 를 단일 슬롯에 등록. 이미 있으면 경고 후 거부.
		void AddLight(SJH::DirLight *light);
		/// @brief DirLight Component 를 단일 슬롯에서 해제.
		void RemoveLight(SJH::DirLight *light);
		/// @brief 현재 등록된 DirLight (비소유). 없으면 nullptr.
		SJH::DirLight *GetDirLight() const
		{
			return mDirLight;
		}

		// -- PointLight vector (max 16 = Const::MAX_POINT_LIGHTS) ----------
		/// @brief PointLight Component 를 벡터에 등록. MAX_POINT_LIGHTS 초과 시 경고 후 거부.
		void AddLight(SJH::PointLight *light);
		/// @brief PointLight Component 를 벡터에서 제거.
		void RemoveLight(SJH::PointLight *light);
		/// @brief 등록된 PointLight 목록 (읽기 전용, 최대 16).
		const std::vector<SJH::PointLight *> &GetPointLights() const
		{
			return mPointLights;
		}

		// -- SpotLight vector (max 16 = Const::MAX_SPOT_LIGHTS) ------------
		/// @brief SpotLight Component 를 벡터에 등록. MAX_SPOT_LIGHTS 초과 시 경고 후 거부.
		void AddLight(SJH::SpotLight *light);
		/// @brief SpotLight Component 를 벡터에서 제거.
		void RemoveLight(SJH::SpotLight *light);
		/// @brief 등록된 SpotLight 목록 (읽기 전용, 최대 16).
		const std::vector<SJH::SpotLight *> &GetSpotLights() const
		{
			return mSpotLights;
		}

		/// @brief 생성자 - @c mPointLights / @c mSpotLights 를 각 상한으로 @c reserve.
		SceneContext();

	  private:
		std::vector<Camera *>          mCameras;              ///< Camera 컬렉션 (addChild 순서).
		SJH::DirLight                 *mDirLight = nullptr;   ///< DirLight 단일 슬롯.
		std::vector<SJH::PointLight *> mPointLights;          ///< PointLight 목록 (max=16).
		std::vector<SJH::SpotLight *>  mSpotLights;           ///< SpotLight 목록 (max=16).
	};
} // namespace SJH::Scene

#endif // __SJH_SCENE_CONTEXT_H__
