#ifndef __SJH_CAMERA_H__
#define __SJH_CAMERA_H__

#include <vmath.h>

namespace SJH
{
    /**
     * @brief 원근 투영 *렌즈* — Fov / 종횡비 / 클리핑 평면 + projection 행렬 산출.
     *
     * @details
     *  ### 책임
     *  - 원근 투영 파라미터 보관 + @ref GetProjMatrix 산출.
     *
     *  ### 비-책임
     *  - ❌ 카메라 *배치* (위치/방향) — `SceneNodeId::Camera` 노드의 @ref Transform 이 소유.
     *    view 행렬은 `inverse(SceneGraph::World(Camera))` 로 산출 (Context).
     *  - ❌ 입력 처리 — 입력 모듈이 카메라 노드를 갱신.
     *
     *  Unity `Camera` 컴포넌트(렌즈)와 GameObject `Transform`(배치) 의 분리와 동형.
     */
    class Camera
    {
    public:
        /// @brief 수직 시야각 (degree).
        float Fov = 60.0f;
        /// @brief 종횡비 (width/height). @ref SetAspect 가 갱신.
        float Aspect = 1.0f;
        /// @brief 가까운 클리핑 평면 (> 0).
        float NearPlane = 0.1f;
        /// @brief 먼 클리핑 평면 (> NearPlane).
        float FarPlane = 1000.0f;

        /// @brief 원근 투영 행렬 — 멤버 @c Aspect 사용.
        /// @note vmath::perspective 는 Fov 를 degree 로 직접 수용.
        vmath::mat4 GetProjMatrix() const
        {
            return vmath::perspective(Fov, Aspect, NearPlane, FarPlane);
        }

        /// @brief 뷰포트 크기로 종횡비 갱신. height==0 (최소화) 이면 무시.
        void SetAspect(float width, float height)
        {
            if (height <= 0.0f)
                return;
            Aspect = width / height;
        }
    };
} // namespace SJH

#endif //__SJH_CAMERA_H__
