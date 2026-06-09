/**
 * @file camera.cpp
 * @brief @c Camera Component 행렬 연산 및 SceneContext 등록 구현.
 *
 * @details
 *  ### 책임
 *  - @c GetViewMatrix() - Free(InverseAffine) / TargetLock(lookat) 분기.
 *  - @c GetProjectionMatrix() - Perspective(@c vmath::perspective) / Ortho(직접 구현).
 *  - @c GetInverseProjectionMatrix() - 닫힌 해(closed-form) 역투영 행렬.
 *  - @c OnEnter / @c OnExit - SceneContext Camera 컬렉션 자동 등록/해제.
 *  - @c InverseAffine() - 회전 transpose + translate negate 로 view 행렬 도출.
 *
 *  ### 비-책임
 *  - [X] 렌더 실행 - @c SceneRenderer 가 담당.
 *
 * @note @c vmath::ortho 는 @c m[3][2] 부호 버그 (sb7 미수정 정책) -
 *       Ortho 행렬은 OpenGL spec 기준으로 직접 column-major 구현.
 */
#include "scene/camera.h"
#include "scene/actor.h"
#include "scene/scene.h" // SP-SceneContext+ProgramRegistry - Director::Get().GetContext() 접근.
#include <cassert>
#include <cmath> // std::tan (GetInverseProjectionMatrix)
#include <vmath.h>

namespace SJH::Scene
{
	// CLAUDE_ASSIST
	vmath::mat4 Camera::GetViewMatrix() const
	{
		auto *owner = GetOwner();
		assert(owner && "Camera must be attached to an Actor (use Scene::CreateCameraActor)");

		const auto ownerW = owner->GetWorldMatrix();

		// Lock 모드 - owner.Translate 에서 target.WorldPos 로 lookat.
		if (mLockTarget)
		{
			const auto targetW = mLockTarget->GetWorldMatrix();
			vmath::vec3 eye(ownerW[3][0], ownerW[3][1], ownerW[3][2]);
			vmath::vec3 target(targetW[3][0], targetW[3][1], targetW[3][2]);
			return vmath::lookat(eye, target, vmath::vec3(0.0f, 1.0f, 0.0f));
		}

		return InverseAffine(ownerW);
	}

	vmath::mat4 Camera::GetProjectionMatrix() const
	{
		if (IsOrthographic)
		{
			// vmath::ortho 는 m[3][2] 부호 버그 (sb7 미수정 정책) - OpenGL 표준 spec 으로 직접 구현 (column-major)
			const float l = -OrthoSize * Aspect;
			const float r =  OrthoSize * Aspect;
			const float b = -OrthoSize;
			const float t =  OrthoSize;
			const float n =  NearZ;
			const float f =  FarZ;
			vmath::mat4 m = vmath::mat4::identity();
			m[0][0] =  2.0f / (r - l);
			m[1][1] =  2.0f / (t - b);
			m[2][2] = -2.0f / (f - n);
			m[3][0] = -(r + l) / (r - l);
			m[3][1] = -(t + b) / (t - b);
			m[3][2] = -(f + n) / (f - n);
			return m;
		}
		return vmath::perspective(FovYDeg, Aspect, NearZ, FarZ);
	}

	// CLAUDE_ASSIST
	vmath::mat4 Camera::GetInverseProjectionMatrix() const
	{
		// GetProjectionMatrix() 의 역행렬을 닫힌 해로 구성 (cofactor 일반 inverse 불필요).
		// vmath 는 column-major - r[col][row]. 0 으로 초기화 후 비-zero 성분만 채움.
		vmath::mat4 r(0.0f);

		if (IsOrthographic)
		{
			// ortho 는 diag(scale) + translate(affine). 역행렬도 diag(1/scale) + 보정 translate.
			// l=-OrthoSize*Aspect, r=+, b=-OrthoSize, t=+, n=NearZ, f=FarZ 대입 시 (r+l)=(t+b)=0 으로 소거.
			r[0][0] = OrthoSize * Aspect;            // (right-left)/2
			r[1][1] = OrthoSize;                     // (top-bottom)/2
			r[2][2] = -(FarZ - NearZ) / 2.0f;        // -(f-n)/2
			r[3][2] = -(FarZ + NearZ) / 2.0f;        // col3,row2 : -(f+n)/2
			r[3][3] = 1.0f;
			return r;
		}

		// perspective(vmath::perspective) 의 닫힌 해 역행렬. 검산: M*M^-1 = I.
		//   A=q/aspect, q=1/tan(fovy/2), B=(n+f)/(n-f), C=2nf/(n-f).
		const float t = std::tan(vmath::radians(0.5f * FovYDeg));   // = 1/q
		r[0][0] = Aspect * t;                                        // = 1/A
		r[1][1] = t;                                                 // = 1/q
		r[2][3] = (NearZ - FarZ) / (2.0f * NearZ * FarZ);           // col2,row3 : 1/C
		r[3][2] = -1.0f;                                             // col3,row2
		r[3][3] = (NearZ + FarZ) / (2.0f * NearZ * FarZ);           // col3,row3 : B/C
		return r;
	}

	// SP-SceneContext+ProgramRegistry (2026-05-26) - Component lifecycle hook.
	// Actor 가 씬에 부착될 때 자동으로 SceneContext 에 등록 (Cocos2D `addChild` 정통).
	// 매 프레임 Scene DFS 로 Camera 를 수집하던 SceneRenderer 폐기.
	void Camera::OnEnter()
	{
		Director::Get().GetContext().AddCamera(this);
	}

	void Camera::OnExit()
	{
		Director::Get().GetContext().RemoveCamera(this);
	}

	// CLAUDE_ASSIST
	vmath::mat4 Camera::InverseAffine(const vmath::mat4 &m)
	{
		// vmath 는 column-major - m[col][row]. m[0..2] 가 3x3 회전, m[3] 이 translate.
		// affine inverse: 회전부 transpose + translate 부 = -R^T * t.
		vmath::mat4 r = vmath::mat4::identity();

		// 회전 transpose: r[i][j] = m[j][i] (i,j in {0,1,2})
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				r[i][j] = m[j][i];
		
		// -R^T * t - t = m[3] 의 xyz, R^T 는 r 의 좌상단 3x3 (방금 transpose 한 값).
		const vmath::vec3 t(m[3][0], m[3][1], m[3][2]);
		r[3][0] = -(r[0][0] * t[0] + r[1][0] * t[1] + r[2][0] * t[2]);
		r[3][1] = -(r[0][1] * t[0] + r[1][1] * t[1] + r[2][1] * t[2]);
		r[3][2] = -(r[0][2] * t[0] + r[1][2] * t[1] + r[2][2] * t[2]);
		// r[3][3] = 1 (identity 로 초기화됨)

		return r;
	}
} // namespace SJH::Scene
