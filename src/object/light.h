/**
 * @file light.h
 * @brief 점 광원 — 위치 + Phong 3항 (ambient / diffuse / specular) 색상.
 *
 * @details
 *  ### 책임
 *  - 광원 *위치* 와 Phong 라이팅 모델의 *3개 항 색상* 보관.
 *  - 셰이더 uniform `light.position` / `light.ambient` / `light.diffuse` / `light.specular` 로 전송될 데이터 컨테이너.
 *
 *  ### Phong 3항의 직관
 *  - **ambient**: 광원과 무관한 *기본 밝기* — 그림자 영역도 완전히 검지 않게.
 *  - **diffuse**: 표면 normal 과 광원 방향의 cos 으로 감쇠 — *주된* 밝기 항.
 *  - **specular**: 시선 방향과 반사 벡터의 cos^shininess — *하이라이트*.
 *
 *  ### 비-책임
 *  - ❌ 셰이더 uniform 전송 — @c Context::Render 가 @c Uniforms::SetVec3 로 직접 push.
 *  - ❌ 광원 *타입* (점/방향/스포트) 분기 — 현재 점광원만 지원. 추후 enum + subclass 도입 예정.
 *
 * @note 현재 @c SJH:: 네임스페이스 *외부* 에 정의 — 코드베이스 다른 클래스와 일관성 어긋남.
 *       향후 @c SJH:: 로 이동 + 광원 타입 enum 도입 예정.
 */

#ifndef __SJH_LIGHT_H__
#define __SJH_LIGHT_H__
#include "scene/actor.h" // SP5: Component base + Actor::GetWorldMatrix (모두 inline -> link 의존 0)
#include <vmath.h>

namespace SJH
{
	/// @brief 점 광원 + Phong 3항 색상 컨테이너.
	class Light
	{
	  public:
		/// @brief 점 광원 월드 좌표. 셰이더 uniform `light.position`. ImGui DragFloat3 위젯이 갱신.
		vmath::vec3 Pos{vmath::vec3(3.0f, 3.0f, 3.0f)};

		/// @brief Ambient 항 색상 (RGB, 0~1). 셰이더 uniform `light.ambient`.
		/// @details 광원과 무관한 기본 밝기. 일반적으로 매우 작은 값 (예: @c (0.1, 0.1, 0.1)) 으로 그림자 영역에도 약간의 색.
		vmath::vec3 Ambient{vmath::vec3(0.1f, 0.1f, 0.1f)};

		/// @brief Diffuse 항 색상 (RGB, 0~1). 셰이더 uniform `light.diffuse`.
		/// @details Lambertian 항의 광원 색. 광원의 *주된 색상* — 일반적으로 흰색 근처.
		vmath::vec3 Diffuse{vmath::vec3(0.5f, 0.5f, 0.5f)};

		/// @brief Specular 항 색상 (RGB, 0~1). 셰이더 uniform `light.specular`.
		/// @details 하이라이트 색. 일반적으로 흰색 — 금속이 아닌 표면은 광원 색을 그대로 반사.
		vmath::vec3 Specular{vmath::vec3(1.0f, 1.0f, 1.0f)};
	};

	/**
	 * @brief 평행광 (태양 등) — 위치 없이 *방향* 만 가짐. 거리 감쇠 없음.
	 * @details
	 *  모든 표면에 동일한 방향에서 평행하게 들어오는 무한원 광원.
	 *  거리 감쇠(attenuation) 없음 — 태양광 모델링에 적합.
	 *  셰이더 구조체 `DirLight` 와 1:1 매핑.
	 *
	 *  SP5 — `Scene::Component` 상속 추가. Owner Actor 의 Transform 이 방향 제공:
	 *  `GetWorldDirection()` = worldMatrix[2] (forward = +Z) 정규화.
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

		/// @brief Owner Actor 의 worldMatrix forward(+Z) 컬럼 정규화. Owner 없을 때 (-Z) fallback.
		vmath::vec3 GetWorldDirection() const;

		virtual void OnEnter() override {
			
		}
		virtual void OnExit() override {
			
		}
		virtual void Update(float dt) override {
			
		}
	};

	/**
	 * @brief 점 광원 — 위치 + 거리 감쇠 + Phong 3항.
	 * @details
	 *  거리 감쇠(attenuation)를 적용하는 점 광원. 셰이더 구조체 `PointLight` 와 1:1 매핑.
	 *  감쇠 계수(Kc, Kl, Kq)는 @ref GetAttenuationCoeff 가 도달 거리(@c Distance)에서 자동 도출.
	 *
	 *  SP5 — `Scene::Component` 상속 추가. Owner Actor 의 Transform 이 위치 제공:
	 *  `GetWorldPosition()` = worldMatrix[3].xyz (translate column).
	 *
	 * @note Actor 가 컴포넌트 map 에 `type_index` 로 저장 — *PointLight 두 개 같은 Actor 에 부착 불가*.
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

		virtual void OnEnter() override {
			
		}
		virtual void OnExit() override {
			
		}
		virtual void Update(float dt) override {
			
		}
	};

	/// @brief 스포트라이트 — 위치 + 콘 축 방향 + inner/outer 컷오프 + 거리 감쇠 + Phong 3항.
	/// @details PointLight 에 방향(@ref Direction)과 콘 컷오프(@ref CutoffAngleDeg, @ref OuterCutoffAngleDeg)
	///          가 추가된 형태. 콘 안쪽은 fully lit, 바깥쪽은 fully dark,
	///          inner~outer 구간은 부드럽게 감쇠(soft edge) 시키는 데 사용.
	///
	///          SP5 — `Scene::Component` 상속. 위치 + 방향 모두 Owner Actor Transform 도출.
	class SpotLight : public Scene::Component
	{
	  public:
		/// @brief 콘 안쪽 컷오프 각도 (degree). 이 각도 이내는 fully lit.
		/// @details degree 로 보관 — 송신 시점 (Uniforms::SetSpotLight) 에 cosf(radians) 변환.
		float CutoffAngleDeg{12.5f};

		/// @brief 콘 바깥쪽 컷오프 각도 (degree). 이 각도 바깥은 fully dark.
		float OuterCutoffAngleDeg{17.5f};

		/// @brief 거리 감쇠 산출 기준 도달 거리.
		float Distance{32.0f};

		vmath::vec3 Ambient{vmath::vec3(0.1f, 0.1f, 0.1f)};
		vmath::vec3 Diffuse{vmath::vec3(0.5f, 0.5f, 0.5f)};
		vmath::vec3 Specular{vmath::vec3(1.0f, 1.0f, 1.0f)};

		vmath::vec3 GetWorldPosition() const;
		vmath::vec3 GetWorldDirection() const;

		virtual void OnEnter() override {
			
		}
		virtual void OnExit() override {
			
		}
		virtual void Update(float dt) override {
			
		}
	};

	/**
	 * @brief 도달 거리에서 감쇠 계수 (Kc, Kl, Kq) 를 3차 다항식으로 도출.
	 * @param distance 빛이 유의미하게 도달하는 최대 거리 (world unit). 권장 범위 @c 7 ~ @c 600.
	 * @return @c vmath::vec3(Kc, Kl, Kq) — @c Kc=1 (상수항 고정), @c Kl (선형 감쇠), @c Kq (이차 감쇠).
	 * @note 계수 다항식은 Ogre3D / LearnOpenGL 거리 테이블에서 회귀 도출. Kl, Kq 는 음수 방지 클램프.
	 */
	static vmath::vec3 GetAttenuationCoeff(float distance)
	{
		const auto linear_coeff = vmath::vec4(
		    8.4523112e-05f, 4.4712582e+00f, -1.8516388e+00f, 3.3955811e+01f);
		const auto quad_coeff = vmath::vec4(
		    -7.6103583e-04f, 9.0120201e+00f, -1.1618500e+01f, 1.0000464e+02f);

		float kc = 1.0f;
		float d = 1.0f / distance;
		auto dvec = vmath::vec4(1.0f, d, d * d, d * d * d);
		float kl = vmath::dot(linear_coeff, dvec);
		float kq = vmath::dot(quad_coeff, dvec);

		return vmath::vec3(kc, vmath::max(kl, 0.0f), vmath::max(kq * kq, 0.0f));
	}

} // namespace SJH
#endif // __SJH_LIGHT_H__
