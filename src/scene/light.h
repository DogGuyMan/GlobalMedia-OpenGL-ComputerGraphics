/**
 * @file light.h
 * @brief 광원 *컴포넌트* 3종 - DirLight(평행광) / PointLight(점광) / SpotLight(스포트).
 *
 * @details
 *  ### 거주지 (2026-06-11 E1 사이클 해소로 이주)
 *  세 클래스는 @c Scene::Component 파생 + @c OnEnter/OnExit 가 scene 의 Director(@c SceneContext)
 *  를 호출 - 몸이 이미 scene 쪽이라 본 모듈(@c SJH::scene)에 거주한다. 과거 @c object/light.h 에
 *  동거하던 시절 object -> scene 역의존을 유발했으므로 분리했다.
 *  (순수 데이터 @c Light POD + @c GetAttenuationCoeff 자유 함수는 @c object/light.h 잔존.)
 *
 *  ### 책임
 *  - 광원 Phong 3항 색상(ambient/diffuse/specular) + 타입별 추가 파라미터 보관.
 *  - Owner Actor 의 worldMatrix 에서 월드 방향/위치 도출 (@c GetWorldDirection / @c GetWorldPosition).
 *  - Actor 트리 부착/해제 시 @c SceneContext 자동 등록/해제 (Cocos2D cc::Light 정통).
 *
 *  ### 비-책임
 *  - [X] 셰이더 uniform 전송 - @c LightUniformDispatcher (render) 가 담당.
 *  - [X] 거리 감쇠 계수 계산 - @c GetAttenuationCoeff (@c object/light.h) 자유 함수가 담당.
 */

#ifndef __SJH_SCENE_LIGHT_H__
#define __SJH_SCENE_LIGHT_H__
#include "scene/actor.h" // Component base + Actor::GetWorldMatrix (모두 inline -> link 의존 0)
#include <vmath.h>

namespace SJH
{
	/**
	 * @brief 평행광 (태양 등) - 위치 없이 *방향* 만 가짐. 거리 감쇠 없음.
	 * @details
	 *  모든 표면에 동일한 방향에서 평행하게 들어오는 무한원 광원.
	 *  거리 감쇠(attenuation) 없음 - 태양광 모델링에 적합.
	 *  셰이더 구조체 `DirLight` 와 1:1 매핑.
	 *
	 *  SP5 - `Scene::Component` 상속 추가. Owner Actor 의 Transform 이 방향 제공:
	 *  `GetWorldDirection()` = worldMatrix 의 -Z 컬럼 (forward, = -worldMatrix[2]) 정규화.
	 */
	class DirLight : public Scene::Component
	{
	  public:
		/// @brief Ambient 항 색상 (RGB, 0~1). 셰이더 uniform `light.ambient`.
		vmath::vec3 Ambient{vmath::vec3(0.1f, 0.1f, 0.1f)};

		/// @brief Diffuse 항 색상 (RGB, 0~1). 셰이더 uniform `light.diffuse`.
		vmath::vec3 Diffuse{vmath::vec3(0.5f, 0.5f, 0.5f)};

		/// @brief Specular 항 색상 (RGB, 0~1). 셰이더 uniform `light.specular`.
		vmath::vec3 Specular{vmath::vec3(1.0f, 1.0f, 1.0f)};

		/// @brief Owner Actor 의 worldMatrix forward(-Z 컬럼) 정규화. Owner 없을 때 (-Z) fallback.
		vmath::vec3 GetWorldDirection() const;

		// SP-SceneContext+ProgramRegistry (2026-05-26) - Cocos cc::Light 정통 자동 등록.
		// OnEnter 에서 Director::GetContext().AddLight(this), OnExit 에서 Remove.
		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override
		{
		} // Light 는 매 프레임 작업 없음 (worldPos/Dir 은 GetWorldXxx 매 호출 도출).
	};

	/**
	 * @brief 점 광원 - 위치 + 거리 감쇠 + Phong 3항.
	 * @details
	 *  거리 감쇠(attenuation)를 적용하는 점 광원. 셰이더 구조체 `PointLight` 와 1:1 매핑.
	 *  감쇠 계수(Kc, Kl, Kq)는 @c GetAttenuationCoeff (@c object/light.h) 가 도달 거리(@c Distance)에서 자동 도출.
	 *
	 *  SP5 - `Scene::Component` 상속 추가. Owner Actor 의 Transform 이 위치 제공:
	 *  `GetWorldPosition()` = worldMatrix[3].xyz (translate column).
	 *
	 * @note Actor 가 컴포넌트 map 에 `type_index` 로 저장 - *PointLight 두 개 같은 Actor 에 부착 불가*.
	 *       복수 점광원은 Actor 인스턴스를 분리해 구성 (SceneRenderer 이 모두 수집).
	 */
	class PointLight : public Scene::Component
	{
	  public:
		/// @brief 거리 감쇠 산출 기준 도달 거리.
		float Distance{32.0f};

		/// @brief Ambient 항 색상 (RGB, 0~1).
		vmath::vec3 Ambient{vmath::vec3(0.1f, 0.1f, 0.1f)};

		/// @brief Diffuse 항 색상 (RGB, 0~1).
		vmath::vec3 Diffuse{vmath::vec3(0.5f, 0.5f, 0.5f)};

		/// @brief Specular 항 색상 (RGB, 0~1).
		vmath::vec3 Specular{vmath::vec3(1.0f, 1.0f, 1.0f)};

		/// @brief Owner Actor 의 worldMatrix translate column. Owner 없을 때 원점.
		vmath::vec3 GetWorldPosition() const;

		// SP-SceneContext+ProgramRegistry (2026-05-26) - Cocos cc::Light 정통 자동 등록.
		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override
		{
		} // Light 는 매 프레임 작업 없음.
	};

	/// @brief 스포트라이트 - 위치 + 콘 축 방향 + inner/outer 컷오프 + 거리 감쇠 + Phong 3항.
	/// @details PointLight 에 방향(@ref Direction)과 콘 컷오프(@ref CutoffAngleDeg, @ref OuterCutoffAngleDeg)
	///          가 추가된 형태. 콘 안쪽은 fully lit, 바깥쪽은 fully dark,
	///          inner~outer 구간은 부드럽게 감쇠(soft edge) 시키는 데 사용.
	///
	///          SP5 - `Scene::Component` 상속. 위치 + 방향 모두 Owner Actor Transform 도출.
	class SpotLight : public Scene::Component
	{
	  public:
		/// @brief 콘 안쪽 컷오프 각도 (degree). 이 각도 이내는 fully lit.
		/// @details degree 로 보관 - 송신 시점 (LightUniformDispatcher) 에 cosf(radians) 변환.
		float CutoffAngleDeg{12.5f};

		/// @brief 콘 바깥쪽 컷오프 각도 (degree). 이 각도 바깥은 fully dark.
		float OuterCutoffAngleDeg{17.5f};

		/// @brief 거리 감쇠 산출 기준 도달 거리.
		float Distance{32.0f};

		/// @brief Ambient 항 색상 (RGB, 0~1). 셰이더 uniform `light.ambient`.
		vmath::vec3 Ambient{vmath::vec3(0.1f, 0.1f, 0.1f)};
		/// @brief Diffuse 항 색상 (RGB, 0~1). 셰이더 uniform `light.diffuse`.
		vmath::vec3 Diffuse{vmath::vec3(0.5f, 0.5f, 0.5f)};
		/// @brief Specular 항 색상 (RGB, 0~1). 셰이더 uniform `light.specular`.
		vmath::vec3 Specular{vmath::vec3(1.0f, 1.0f, 1.0f)};

		/// @brief Owner Actor 의 worldMatrix translate column. Owner 없을 때 원점.
		vmath::vec3 GetWorldPosition() const;
		/// @brief Owner Actor 의 worldMatrix forward(-Z 컬럼) 정규화. Owner 없을 때 (-Z) fallback.
		vmath::vec3 GetWorldDirection() const;

		// SP-SceneContext+ProgramRegistry (2026-05-26) - Cocos cc::Light 정통 자동 등록.
		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override
		{
		} // Light 는 매 프레임 작업 없음.
	};

} // namespace SJH
#endif // __SJH_SCENE_LIGHT_H__
