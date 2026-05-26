#ifndef __SJH_LIGHT_UNIFORM_DISPATCHER_H__
#define __SJH_LIGHT_UNIFORM_DISPATCHER_H__

/**
 * @file light_uniform_dispatcher.h
 * @brief 활성 Light 데이터를 모든 Program 에 일괄 송신.
 *
 * @details
 *  ### 존재 의의
 *  SceneRenderer 에서 Light uniform 송신 책임을 분리 (Phase 2).
 *  SceneRenderer 의 책임: Camera 수집/순회 + 위임.
 *  LightUniformDispatcher 의 책임: program 순회 + uniform 값 쓰기.
 *
 *  ### 4 엔진 정통 (Context7 검증 2026-05-27)
 *  - Unity URP   : LightLoop 가 _MainLightColor / _AdditionalLightsBuffer 에 일괄 업로드
 *  - Unreal      : FDeferredShadingSceneRenderer 가 FDeferredLightUniformStruct 로 일괄 바인딩
 *  - Cocos2d-x   : Mesh::setLightUniforms 가 씬 순회 후 배열 일괄 송신
 *  - Godot       : RenderingServer 내부가 RID 기반으로 Light 파라미터 일괄 전달
 *  ->Light 컴포넌트는 데이터 보유만 — GPU 전송은 외부 시스템(이 클래스)이 전담.
 */

#include <vector>
#include <vmath.h>

namespace SJH
{
	class Program;
	class DirLight;
	class PointLight;
	class SpotLight;

	/// @brief 수집된 Light 목록을 등록된 모든 Program 에 일괄 송신.
	/// @details SceneRenderer::RenderWithCamera 가 SceneContext 에서 Light 를 수집한 뒤 위임.
	///          상태 없음 — 매 호출이 독립적 (동일 인스턴스 재사용 안전).
	class LightUniformDispatcher
	{
	  public:
		/// @brief 수집된 Light 세트를 @p programs 전체에 uniform 송신.
		/// @param programs  ResourceRegistry::GetAllPrograms() 스냅샷.
		/// @param dir       활성 DirLight (없으면 nullptr — dirLightEnabled=0 전송).
		/// @param points    활성 PointLight 목록 (MAX_POINT_LIGHTS 초과분 warn+무시).
		/// @param spots     활성 SpotLight 목록 (MAX_SPOT_LIGHTS 초과분 warn+무시).
		/// @param viewPos   카메라 월드 위치 (Phong specular viewPos uniform).
		/// @note  lighting sentinel: UNI_VIEW_POS 가 없는 program 은 lighting 미사용으로 판정해 skip.
		void Dispatch(const std::vector<Program *> &programs,
		              DirLight *dir,
		              const std::vector<PointLight *> &points,
		              const std::vector<SpotLight *> &spots,
		              const vmath::vec3 &viewPos);
	};

} // namespace SJH

#endif // __SJH_LIGHT_UNIFORM_DISPATCHER_H__
