/**
 * @file transform.h
 * @brief 로컬 TRS 값 객체 + UV 변환. 계층/월드 합성은 SceneNode 가 담당.
 *
 * @details
 *  ### 책임
 *  - @ref SJH::Transform — TRS(Translate/Rotate/Scale) 값 + 로컬 모델 행렬 산출.
 *  - @ref SJH::UVTransform — UV 오프셋/스케일/회전을 묶은 경량 구조체.
 *
 *  ### 비-책임
 *  - ❌ 계층 구조 / 월드 합성 — @ref SJH::SceneNode 가 담당.
 *  - ❌ 렌더링 / GL uniform 전송 — Context::Render 가 담당.
 */

#ifndef __SJH_TRANSFORM_H__
#define __SJH_TRANSFORM_H__

#include <vmath.h>

namespace SJH
{
    /**
     * @brief 로컬 TRS 값 객체.
     * @details 회전은 오일러 각(degree, X->Y->Z 순서) — 짐벌락 주의. 계층/월드 합성은
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
         * @brief 로컬 모델 행렬 산출 — T·Rz·Ry·Rx·S 순서.
         * @return 부모를 고려하지 않은 로컬 변환 행렬.
         */
        vmath::mat4 GetLocalMatrix() const
        {
            // vmath::rotate 는 degree 직접 수용 (내부에서 radian 변환)
            return vmath::translate(Translate) *
                   vmath::rotate(EulerRot[2], 0.0f, 0.0f, 1.0f) *
                   vmath::rotate(EulerRot[1], 0.0f, 1.0f, 0.0f) *
                   vmath::rotate(EulerRot[0], 1.0f, 0.0f, 0.0f) *
                   vmath::scale(Scale);
        }
    };

    /// @brief UV 좌표계 변환 (오프셋 + 스케일 + 회전) 경량 구조체.
    class UVTransform
    {
    public:
        vmath::vec2 Offset{0.0f, 0.0f}; ///< UV 오프셋 — 텍스처 스크롤 효과.
        vmath::vec2 Scale{1.0f, 1.0f};  ///< UV 스케일 — 타일링 배수.
        float RotationDeg = 0.0f;       ///< UV 회전각 (degree).
    };
}
#endif //__SJH_TRANSFORM_H__
