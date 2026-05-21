#include "scene/camera.h"
#include "scene/actor.h"
#include <cassert>
#include <vmath.h>

namespace SJH::Scene
{
	vmath::mat4 Camera::GetViewMatrix() const
	{
		auto *owner = GetOwner();
		assert(owner && "Camera must be attached to an Actor (use Scene::CreateCameraActor)");

		const auto ownerW = owner->GetWorldMatrix();

		// Lock 모드 — owner.Translate 에서 target.WorldPos 로 lookat.
		// Cinemachine Composer / Unreal LookAt 정통. owner EulerRot 무시.
		if (mLockTarget)
		{
			const auto targetW = mLockTarget->GetWorldMatrix();
			vmath::vec3 eye(ownerW[3][0], ownerW[3][1], ownerW[3][2]);
			vmath::vec3 target(targetW[3][0], targetW[3][1], targetW[3][2]);
			return vmath::lookat(eye, target, vmath::vec3(0.0f, 1.0f, 0.0f));
		}

		// 통상 모드 — owner Transform 의 EulerRot/Translate 가 진실의 원천.
		return InverseAffine(ownerW);
	}

	vmath::mat4 Camera::GetProjectionMatrix() const
	{
		return vmath::perspective(FovYDeg, Aspect, NearZ, FarZ);
	}

	vmath::mat4 Camera::InverseAffine(const vmath::mat4 &m)
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
