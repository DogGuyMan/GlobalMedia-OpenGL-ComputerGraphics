/**
 * @file transform.h
 * @brief 로컬 TRS 값 객체 + UV 변환. 계층/월드 합성은 SceneNode 가 담당.
 *
 * @details
 *  ### 책임
 *  - @ref SJH::Transform - TRS(Translate/Rotate/Scale) 값 + 로컬 모델 행렬 산출 + 6 방향 벡터 추출.
 *  - @ref SJH::UVTransform - UV 오프셋/스케일/회전을 묶은 경량 구조체.
 *
 *  ### 비-책임
 *  - [X] 계층 구조 / 월드 행렬 합성 - @c SceneNode 가 담당.
 *  - [X] 렌더링 / GL uniform 전송 - @c Context::Render 가 담당.
 *
 * @note 회전은 오일러 각(degree, X->Y->Z 순서) 표현 - 짐벌락 주의.
 *       쿼터니언 도입 전까지 이 제약이 유지된다.
 */

#ifndef __SJH_TRANSFORM_H__
#define __SJH_TRANSFORM_H__

#include <spdlog/spdlog.h>
#include <vmath.h>

namespace SJH
{
    /**
     * @brief 로컬 TRS 값 객체.
     * @details 회전은 오일러 각(degree, X->Y->Z 순서) - 짐벌락 주의. 계층/월드 합성은
     *          @ref SceneNode 가 담당하며 본 클래스는 로컬 변환만 안다. 모든 멤버 public
     *          (Camera/Light 와 같은 POD-like 컨벤션).
     */
    class Transform
    {
    public:
        vmath::vec3 Translate = vmath::vec3(0.0f, 0.0f, 0.0f); ///< 이동량 (로컬 공간 offset).
        vmath::vec3 EulerRot  = vmath::vec3(0.0f, 0.0f, 0.0f); ///< 오일러 회전각 (degree, XYZ 순서).
        vmath::vec3 Scale     = vmath::vec3(1.0f, 1.0f, 1.0f); ///< 스케일 팩터.


        /**
         * @brief 로컬 모델 행렬 산출 - T,Rz,Ry,Rx,S 순서.
         * @return 부모를 고려하지 않은 로컬 변환 행렬.
         */
        vmath::mat4 GetLocalMatrix() const
        {
            // vmath::rotate 는 degree 직접 수용 (내부에서 radian 변환)
            return vmath::translate(Translate) *
                   GetRotationMatrix() *
                   vmath::scale(Scale);
        }

        /**
         * @brief Translate/Scale 무시한 *로컬 회전 행렬* - Rz,Ry,Rx.
         * @details 6 방향 벡터 추출 / 부모-자식 회전 합성 등 *방향만* 필요할 때 사용.
         */
        vmath::mat4 GetRotationMatrix() const
        {
            return vmath::rotate(EulerRot[2], 0.0f, 0.0f, 1.0f) *
                   vmath::rotate(EulerRot[1], 0.0f, 1.0f, 0.0f) *
                   vmath::rotate(EulerRot[0], 1.0f, 0.0f, 0.0f);
        }

        // -- 6 방향 벡터 - OpenGL 오른손 좌표계 정통 -------------------------
        // EulerRot=(0,0,0) 기본 상태:
        //   Right   = (+1,  0,  0)  - +X
        //   Up      = ( 0, +1,  0)  - +Y
        //   Forward = ( 0,  0, -1)  - -Z  (OpenGL 카메라 응시 방향 정통)
        //   Left/Down/Back = 반대 부호.
        // EulerRot 적용 후의 회전된 축. Scale 영향 없음.

        /// @brief 회전된 +X 축 (right). 행렬의 0번 컬럼.
        vmath::vec3 GetRight() const
        {
            const auto R = GetRotationMatrix();
            return vmath::vec3(R[0][0], R[0][1], R[0][2]);
        }

        /// @brief 회전된 +Y 축 (up). 행렬의 1번 컬럼.
        vmath::vec3 GetUp() const
        {
            const auto R = GetRotationMatrix();
            return vmath::vec3(R[1][0], R[1][1], R[1][2]);
        }

        /// @brief 회전된 -Z 축 (forward, OpenGL 정통). 행렬의 2번 컬럼의 음수.
        vmath::vec3 GetForward() const
        {
            const auto R = GetRotationMatrix();
            return vmath::vec3(-R[2][0], -R[2][1], -R[2][2]);
        }

        /// @brief 회전된 -X 축 (left). @ref GetRight 의 반전.
        vmath::vec3 GetLeft() const { return -GetRight(); }
        /// @brief 회전된 -Y 축 (down). @ref GetUp 의 반전.
        vmath::vec3 GetDown() const { return -GetUp(); }
        /// @brief 회전된 +Z 축 (back, OpenGL 정통). @ref GetForward 의 반전.
        vmath::vec3 GetBack() const { return -GetForward(); }

        /**
         * @brief Translate / EulerRot / Scale 을 한 번에 설정하는 fluent 빌더.
         * @param translate 이동량 (로컬 공간 offset). 기본값 @c (0,0,0).
         * @param rotate    오일러 회전각 (degree, XYZ 순서). 기본값 @c (0,0,0).
         * @param scal      스케일 팩터. 기본값 @c (1,1,1).
         * @return @c *this - 메서드 체이닝 지원.
         */
	Transform& SetTransformWithVectors(
		vmath::vec3 translate = vmath::vec3(0.0f, 0.0f, 0.0f),
		vmath::vec3 rotate = vmath::vec3(0.0f, 0.0f, 0.0f),
		vmath::vec3 scal = vmath::vec3(1.0f, 1.0f, 1.0f)
	)
	{
		Translate = translate;
		EulerRot = rotate;
		Scale = scal;
		return *this;
	}

        /// @brief Translate / EulerRot / Scale 을 spdlog::info 로 출력 - 디버그 전용.
	void PrintTransform() {
		spdlog::info("Translate {}, {}, {}", Translate[0], Translate[1], Translate[2]);
		spdlog::info("Rotate {}, {}, {}", EulerRot[0], EulerRot[1], EulerRot[2]);
		spdlog::info("Scale {}, {}, {}", Scale[0], Scale[1], Scale[2]);
	}
    };

    /**
     * @brief UV 좌표계 변환 - 오프셋 + 스케일 + 회전을 묶은 경량 POD 구조체.
     * @details 셰이더 @c uv uniform 전송 전에 적용. 스크롤/타일링/회전 효과를 CPU 측에서 계산.
     */
    class UVTransform
    {
    public:
        vmath::vec2 Offset{0.0f, 0.0f}; ///< UV 오프셋 - 텍스처 스크롤 효과.
        vmath::vec2 Scale{1.0f, 1.0f};  ///< UV 스케일 - 타일링 배수.
        float RotationDeg = 0.0f;       ///< UV 회전각 (degree).
    };
}
#endif //__SJH_TRANSFORM_H__
