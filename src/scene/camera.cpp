#include "scene/camera.h"

namespace SJH::Scene
{
    vmath::mat4 Camera::GetViewMatrix() const
    {
        // 하이브리드 — Actor 부착 시 그 Transform 따라감, 아니면 standalone lookat.
        if (auto* owner = GetOwner())
            return InverseAffine(owner->GetWorldMatrix());
        return vmath::lookat(mEye, mTarget, mUp);
    }

    vmath::mat4 Camera::GetProjectionMatrix() const
    {
        return vmath::perspective(mFovYDeg, mAspect, mNearZ, mFarZ);
    }

    vmath::mat4 Camera::InverseAffine(const vmath::mat4& m)
    {
        // vmath 는 column-major — m[col][row]. m[0..2] 가 3x3 회전, m[3] 이 translate.
        // affine inverse: 회전부 transpose + translate 부 = -R^T * t.
        vmath::mat4 r = vmath::mat4::identity();

        // 회전 transpose: r[i][j] = m[j][i] (i,j ∈ {0,1,2})
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r[i][j] = m[j][i];

        // -R^T * t — t = m[3] 의 xyz, R^T 는 r 의 좌상단 3x3 (방금 transpose 한 값).
        const vmath::vec3 t(m[3][0], m[3][1], m[3][2]);
        r[3][0] = -(r[0][0] * t[0] + r[1][0] * t[1] + r[2][0] * t[2]);
        r[3][1] = -(r[0][1] * t[0] + r[1][1] * t[1] + r[2][1] * t[2]);
        r[3][2] = -(r[0][2] * t[0] + r[1][2] * t[1] + r[2][2] * t[2]);
        // r[3][3] = 1 (identity 로 초기화됨)

        return r;
    }
} // namespace SJH::Scene
