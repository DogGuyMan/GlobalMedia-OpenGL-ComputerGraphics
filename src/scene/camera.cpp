/**
 * @file camera.cpp
 * @brief @c Camera Component 행렬 연산 및 SceneContext 등록 구현.
 *
 * @details
 *  ### 책임
 *  - @c GetViewMatrix() - Free(@c glm::affineInverse) / TargetLock(@c glm::lookAt) 분기.
 *  - @c GetProjectionMatrix() - Perspective(@c glm::perspective) / Ortho(@c glm::ortho).
 *  - @c GetInverseProjectionMatrix() - @c glm::inverse 로 역투영 행렬 산출.
 *  - @c OnEnter / @c OnExit - SceneContext Camera 컬렉션 자동 등록/해제.
 *
 *  ### 비-책임
 *  - [X] 렌더 실행 - @c SceneRenderer 가 담당.
 */
#include "scene/camera.h"
#include "scene/actor.h"
#include "scene/scene.h" // SP-SceneContext+ProgramRegistry - Director::Get().GetContext() 접근.
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // glm::lookAt / glm::perspective / glm::ortho
#include <glm/gtc/matrix_inverse.hpp>   // glm::affineInverse (view = 카메라 월드 변환의 affine 역)

namespace SJH::Scene
{
	// CLAUDE_ASSIST
	glm::mat4 Camera::GetViewMatrix() const
	{
		auto *owner = GetOwner();
		assert(owner && "Camera must be attached to an Actor (use Scene::CreateCameraActor)");

		const auto ownerW = owner->GetWorldMatrix();

		// Lock 모드 - owner.Translate 에서 target.WorldPos 로 lookat.
		if (mLockTarget)
		{
			const auto targetW = mLockTarget->GetWorldMatrix();
			const glm::vec3 eye(ownerW[3]);    // 컬럼3 = translation (vec4 -> vec3, w 절삭)
			const glm::vec3 target(targetW[3]);
			return glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
		}

		// Free 모드 - view = 카메라 월드 변환의 affine 역행렬. 회전 transpose + translate negate 를
		// glm::affineInverse 가 처리 (scale=1 가정이 깨져도 3x3 역으로 정확).
		return glm::affineInverse(ownerW);
	}

	glm::mat4 Camera::GetProjectionMatrix() const
	{
		if (IsOrthographic)
		{
			// 대칭 ortho - OrthoSize/Aspect 로 화면 범위 산출. glm::ortho 가 동일 column-major 행렬 생성.
			const float halfW = OrthoSize * Aspect;
			return glm::ortho(-halfW, halfW, -OrthoSize, OrthoSize, NearZ, FarZ);
		}
		// glm::perspective 의 fovy 는 *radian* (구 vmath 는 degree 였음).
		return glm::perspective(glm::radians(FovYDeg), Aspect, NearZ, FarZ);
	}

	glm::mat4 Camera::GetInverseProjectionMatrix() const
	{
		// 투영 행렬의 일반 역행렬. perspective 는 비-affine(w-row != [0,0,0,1]) 이라
		// affineInverse 가 아니라 glm::inverse 사용 - GetProjectionMatrix 가 무엇을 내든 항상 참 역행렬.
		return glm::inverse(GetProjectionMatrix());
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

} // namespace SJH::Scene
