/**
 * @file light.h
 * @brief 광원 순수 데이터 - @c Light POD (Phong 3항 색상 + 위치) + @c GetAttenuationCoeff 자유 함수.
 *
 * @details
 *  ### 책임
 *  - 광원 *위치* 와 Phong 라이팅 모델의 *3개 항 색상* 보관 (@c Light POD).
 *  - 도달 거리에서 거리 감쇠 계수 (Kc, Kl, Kq) 도출 (@c GetAttenuationCoeff 자유 함수).
 *
 *  ### Phong 3항의 직관
 *  - **ambient**: 광원과 무관한 *기본 밝기* - 그림자 영역도 완전히 검지 않게.
 *  - **diffuse**: 표면 normal 과 광원 방향의 cos 으로 감쇠 - *주된* 밝기 항.
 *  - **specular**: 시선 방향과 반사 벡터의 cos^shininess - *하이라이트*.
 *
 *  ### 비-책임
 *  - [X] 셰이더 uniform 전송 - @c LightUniformDispatcher (render) 담당.
 *  - [X] 광원 *컴포넌트* (DirLight/PointLight/SpotLight) - @c scene/light.h 로 이주 (2026-06-11 E1 사이클 해소).
 *    이 헤더는 순수 데이터/함수만 보유해 object -> scene 역의존을 끊는다.
 */

#ifndef __SJH_LIGHT_H__
#define __SJH_LIGHT_H__
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
		/// @details Lambertian 항의 광원 색. 광원의 *주된 색상* - 일반적으로 흰색 근처.
		vmath::vec3 Diffuse{vmath::vec3(0.5f, 0.5f, 0.5f)};

		/// @brief Specular 항 색상 (RGB, 0~1). 셰이더 uniform `light.specular`.
		/// @details 하이라이트 색. 일반적으로 흰색 - 금속이 아닌 표면은 광원 색을 그대로 반사.
		vmath::vec3 Specular{vmath::vec3(1.0f, 1.0f, 1.0f)};
	};

	/**
	 * @brief 도달 거리에서 감쇠 계수 (Kc, Kl, Kq) 를 3차 다항식으로 도출.
	 * @param distance 빛이 유의미하게 도달하는 최대 거리 (world unit). 권장 범위 @c 7 ~ @c 600.
	 * @return @c vmath::vec3(Kc, Kl, Kq^2) - @c Kc=1 (상수항 고정), @c Kl (선형 감쇠), @c Kq^2 (이차 감쇠 회귀값의 *제곱* @c kq*kq).
	 * @note 계수 다항식은 Ogre3D / LearnOpenGL 거리 테이블에서 회귀 도출. Kl 은 음수 방지 클램프, Kq 항은 제곱(@c kq*kq)이라 항상 양수.
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
