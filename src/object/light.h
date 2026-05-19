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
 *  ### 변경 이력
 *  - **Phase 10 (306d9f4)**: Context 가 직접 라이팅 멤버 보유 (`mLightPos` / `mLightColor` / `mAmbientStrength` 등 8개).
 *  - **Phase 12 (02bd90e)**: 본 클래스로 분리 — Phong 3항을 색상 vec3 로 표현.
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
     */
    class DirLight
    {
    public:
        /// @brief Ambient 항 색상 (RGB, 0~1). 셰이더 uniform `light.ambient`.
        /// @details 광원과 무관한 기본 밝기. 일반적으로 매우 작은 값 (예: @c (0.1, 0.1, 0.1)) 으로 그림자 영역에도 약간의 색.
        /// @note 방향은 `SceneNodeId::DirLight` 노드의 Transform(EulerRot) 이 소유 — WorldForward() 가 산출.
        vmath::vec3 Ambient{vmath::vec3(0.1f, 0.1f, 0.1f)};

        /// @brief Diffuse 항 색상 (RGB, 0~1). 셰이더 uniform `light.diffuse`.
        /// @details Lambertian 항의 광원 색. 광원의 *주된 색상* — 일반적으로 흰색 근처.
        vmath::vec3 Diffuse{vmath::vec3(0.5f, 0.5f, 0.5f)};

        /// @brief Specular 항 색상 (RGB, 0~1). 셰이더 uniform `light.specular`.
        /// @details 하이라이트 색. 일반적으로 흰색 — 금속이 아닌 표면은 광원 색을 그대로 반사.
        vmath::vec3 Specular{vmath::vec3(1.0f, 1.0f, 1.0f)};
    };

    /**
     * @brief 점 광원 — 위치 + 거리 감쇠 + Phong 3항.
     * @details
     *  거리 감쇠(attenuation)를 적용하는 점 광원. 셰이더 구조체 `PointLight` 와 1:1 매핑.
     *  감쇠 계수(Kc, Kl, Kq)는 @ref GetAttenuationCoeff 가 도달 거리(@c Distance)에서 자동 도출.
     * @note 배열로 다수 인스턴스화 가능 — 셰이더 `pointLights[i].*` 의 `i` 와 배열 인덱스를 동기.
     */
    class PointLight
    {
    public:
        /// @brief 거리 감쇠 산출 기준 도달 거리. 셰이더 uniform `pointLights[i].attenuation`(vec3) 는 @ref GetAttenuationCoeff 로 도출.
        /// @note 위치는 `SceneNodeId::PointLight*` 노드의 Transform(Translate) 이 소유 — World() 가 산출.
        float Distance{32.0f};

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

    /// @brief 스포트라이트 — 위치 + 콘 축 방향 + inner/outer 컷오프 + 거리 감쇠 + Phong 3항.
    /// @details PointLight 에 방향(@ref Direction)과 콘 컷오프(@ref CutoffAngleDeg, @ref OuterCutoffAngleDeg)
    ///          가 추가된 형태. 콘 안쪽은 fully lit, 바깥쪽은 fully dark,
    ///          inner~outer 구간은 부드럽게 감쇠(soft edge) 시키는 데 사용.
    class SpotLight
    {
    public:
        /// @note 위치/방향은 `SceneNodeId::SpotLight` 노드의 Transform 이 소유 —
        ///       위치는 World(), 방향은 WorldForward() 가 산출.

        /// @brief 콘 안쪽 컷오프 각도 (degree). 이 각도 이내는 fully lit.
        /// @details degree 로 보관 — 셰이더 송신 시 `cosf(vmath::radians(mCutoffAngleDeg))` 변환 후 push.
        float CutoffAngleDeg{12.5f};

        /// @brief 콘 바깥쪽 컷오프 각도 (degree). 이 각도 바깥은 fully dark.
        /// @details CutoffAngleDeg ~ OuterCutoffAngleDeg 구간은 smoothstep 으로 부드럽게 감쇠.
        ///          현재 셰이더(lighting.fs) 는 `light.cutoff` 단일 float 만 사용 — outer 는 향후 확장용.
        float OuterCutoffAngleDeg{17.5f};

        /// @brief 거리 감쇠 산출 기준 도달 거리.
        /// @details 셰이더 uniform `light.attenuation`(vec3 = Kc, Kl, Kq) 는 @ref GetAttenuationCoeff 로 도출.
        float Distance{32.0f};

        /// @brief Ambient 항 색상 (RGB, 0~1). 셰이더 uniform `light.ambient`.
        vmath::vec3 Ambient{vmath::vec3(0.1f, 0.1f, 0.1f)};

        /// @brief Diffuse 항 색상 (RGB, 0~1). 셰이더 uniform `light.diffuse`.
        vmath::vec3 Diffuse{vmath::vec3(0.5f, 0.5f, 0.5f)};

        /// @brief Specular 항 색상 (RGB, 0~1). 셰이더 uniform `light.specular`.
        vmath::vec3 Specular{vmath::vec3(1.0f, 1.0f, 1.0f)};
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
            8.4523112e-05, 4.4712582e+00, -1.8516388e+00, 3.3955811e+01);
        const auto quad_coeff = vmath::vec4(
            -7.6103583e-04, 9.0120201e+00, -1.1618500e+01, 1.0000464e+02);

        float kc = 1.0f;
        float d = 1.0f / distance;
        auto dvec = vmath::vec4(1.0f, d, d * d, d * d * d);
        float kl = vmath::dot(linear_coeff, dvec);
        float kq = vmath::dot(quad_coeff, dvec);

        return vmath::vec3(kc, vmath::max(kl, 0.0f), vmath::max(kq * kq, 0.0f));
    }

}
#endif // __SJH_LIGHT_H__
