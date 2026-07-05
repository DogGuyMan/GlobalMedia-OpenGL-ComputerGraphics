/**
 * @file camera.h
 * @brief View / Projection 행렬 계산 Component - @c Camera.
 *
 * @details
 *  ### 책임
 *  - owner @c Actor Transform 으로부터 view 행렬 도출 (Free 모드 / TargetLock 모드).
 *  - Perspective / Orthographic projection 행렬 + 닫힌 해 역행렬 제공.
 *  - @c OnEnter / @c OnExit 시점에 @c SceneContext 에 자동 등록/해제
 *    (SP-SceneContext+ProgramRegistry 2026-05-26).
 *  - @c CullingMask 비트마스크로 SceneRenderer 의 per-camera layer 필터 지원.
 *
 *  ### 비-책임
 *  - [X] 렌더 순서 정렬 (@c Camera::Depth 폐기 - @c addChild 순서가 렌더 순서).
 *  - [X] 렌더 실행 - @c SceneRenderer 가 담당.
 *  - [X] 독립 생성 - 반드시 Actor 에 부착. 정상 생성 경로 = @c CreateCameraActor().
 *
 * @note Ortho 행렬은 OpenGL spec 기준으로 본 파일에서 직접 column-major 구현
 *       (@c glm::ortho 로 대체 가능 - 부호/규약 검산 가시성 위해 자작 유지).
 */

#ifndef __SJH_SCENE_CAMERA_H__
#define __SJH_SCENE_CAMERA_H__

#include "scene/actor.h" // Component + Actor::GetWorldMatrix
#include "scene/layer.h" // Layer, ToBits (SP5 Task 3)
#include <cstdint>       // uint64_t for cullingMask (SP5 Task 3)
#include <glm/glm.hpp>

namespace SJH
{
	class RenderTarget;
} // namespace SJH

namespace SJH::Scene
{
	/**
	 * @brief Cocos @c cc::Camera / Unity @c Camera 정통 - view/proj 행렬 계산 Component.
	 * @details
	 *  ### Transform 강제 의존 (Compound Actor 컨벤션)
	 *  Camera 는 *반드시* Actor 에 부착되어야 한다. @c Scene::CreateCameraActor(...) factory 가
	 *  유일한 정상 생성 경로. owner @c Transform 의 EulerRot/Translate 가 view 의 진실의 원천.
	 *  forward = owner.WorldMatrix 의 -Z 컬럼.
	 *
	 *  ### TargetLock - Unity Cinemachine Composer / Unreal SpringArm 정통
	 *  @c TargetLock(targetActor) 호출 시 @c GetViewMatrix() 가 @e owner.Translate -> @e target.WorldPos
	 *  를 lookat 하는 행렬 반환. owner EulerRot 무시 (CameraController 의 yaw/pitch 갱신은
	 *  *상태로만 보존* 되고 시각 효과는 lock 동안 무효). @c TargetRelease() 로 해제 시 마지막
	 *  EulerRot 으로 자유 시점 복귀.
	 *
	 *  ### Projection
	 *  Projection 은 항상 자체 파라미터 (@c FovYDeg / @c Aspect / @c NearZ / @c FarZ).
	 *  @c Aspect 는 사용자가 매 프레임 갱신 책임 (@c DeviceContext 의 window 크기 기반).
	 */
	class Camera : public Component
	{
	  public:
		/// @brief true 이면 Orthographic 투영 (Screen Camera 전용). false = Perspective.
		bool  IsOrthographic = false;
		/// @brief Ortho 투영 반높이 - 가시 범위 @c [-OrthoSize*Aspect, OrthoSize*Aspect] (가로) x @c [-OrthoSize, OrthoSize] (세로).
		float OrthoSize      = 1.0f;
		/// @brief true 이면 @c SceneRenderer 가 이 Camera 렌더 진입 시 @c BeginFrame(clear) 대신 BindTarget+state 만 수행.
		/// @details ScreenCamera 전용 - WorldCamera 출력을 보존한 채 PostFX 합성에 사용.
		bool  NoClear        = false;
		/// @brief Perspective 수직 시야각 (degree).
		float FovYDeg = 45.0f;
		/// @brief Aspect ratio (width / height). 매 프레임 @c DeviceContext 기반으로 갱신 권장.
		float Aspect = 16.0f / 9.0f;
		/// @brief Near clipping plane 거리.
		float NearZ = 0.1f;
		/// @brief Far clipping plane 거리.
		float FarZ = 100.0f;

		/// @brief 렌더링 대상 layer 비트마스크 (Unity @c Camera.cullingMask 정통).
		/// @details 기본값 @c Layer::All - 모든 layer 렌더. SceneRenderer 가 프레임마다
		///          @c Actor::GetLayer() & @c CullingMask 로 필터.
		///          SP-SceneContext+ProgramRegistry (2026-05-26) - @c int Depth + @c operator< 폐기.
		///          Camera 순서는 @c SceneContext::mCameras 등록 순서(@c addChild 순서)로 자연 보장.
		uint64_t CullingMask = SJH::Scene::ToBits(SJH::Scene::Layer::All);

		/// @brief CullingMask 비트마스크 직접 주입 (옛 호환). Fluent - @c *this 반환.
		Camera& SetCullingMask(uint64_t mask)
		{
			CullingMask = mask;
			return *this;
		}
		/// @brief type-safe @c Layer overload (SP5 Task 3). Fluent - @c *this 반환.
		Camera& SetCullingMask(SJH::Scene::Layer l)
		{
			CullingMask = SJH::Scene::ToBits(l);
			return *this;
		}

		Camera() = default;
		Camera(float fovYDeg, float aspect, float nearZ, float farZ)
		    : FovYDeg(fovYDeg), Aspect(aspect), NearZ(nearZ), FarZ(farZ)
		{
		}

		/// @brief view 행렬 - owner Transform 기반. Lock 중이면 lookat(owner, target).
		/// @note owner 미부착 시 assert. `Scene::CreateCameraActor` 로 생성하면 자동 충족.
		glm::mat4 GetViewMatrix() const;

		/// @brief perspective(fovY, aspect, near, far).
		glm::mat4 GetProjectionMatrix() const;

		/// @brief GetProjectionMatrix() 의 역행렬 - 닫힌 해(closed-form).
		/// @details perspective/ortho 둘 다 sparse 구조라 cofactor 일반 inverse 불필요.
		///   NDC->view 복원(fog 등 deferred 효과)용. GetProjectionMatrix() 와 동일하게
		///   IsOrthographic 분기. 범용 행렬엔 부적합 - 투영 전용.
		glm::mat4 GetInverseProjectionMatrix() const;

		// -- TargetLock - Unity Cinemachine Composer 정통 ---------------------
		/// @brief 특정 Actor 를 바라보도록 카메라를 lock.
		/// @details lock 중에는 @c GetViewMatrix() 가 owner.Translate -> target.WorldPos
		///          lookat 반환. owner EulerRot 의 시각 효과가 일시 무효.
		/// @param target 바라볼 대상 Actor. nullptr 이면 @c TargetRelease() 와 동등.
		void TargetLock(const Actor *target)
		{
			mLockTarget = target;
		}

		/// @brief TargetLock 해제 - owner Transform 의 EulerRot 다시 view 도출에 사용.
		void TargetRelease()
		{
			mLockTarget = nullptr;
		}

		/// @brief 현재 TargetLock 활성 여부.
		bool IsLocked() const
		{
			return mLockTarget != nullptr;
		}
		/// @brief 현재 lock 대상 Actor (비소유). Lock 중이 아니면 nullptr.
		const Actor *GetLockTarget() const
		{
			return mLockTarget;
		}

		// -- SP4 multi-pass -----------------------------------------------------
		/// @brief 렌더 대상 지정 - Unity @c Camera.targetTexture 정통. 의존 역전 - @c RenderTarget 추상.
		/// @details nullptr = default backbuffer. @c SceneRenderer 가 BeginFrame 시 자동 사용.
		///          @c RenderTexture(FBO) / @c DefaultRenderTarget / 미래 ShadowMap/MRT 등 모두 수용.
		/// @param rt 비소유 포인터 - owner 는 App/Chapter.
		void SetTargetRenderTarget(RenderTarget *rt)
		{
			mTargetRT = rt;
		}
		/// @brief 현재 렌더 대상 (비소유). nullptr 이면 default backbuffer.
		RenderTarget *GetTargetRenderTarget() const
		{
			return mTargetRT;
		}

		/// @brief SP-SceneContext+ProgramRegistry (2026-05-26) - @c SceneContext 에 자동 등록.
		/// @details @c Director::Get().GetContext().AddCamera(this) 호출.
		///          매 프레임 Scene DFS 로 Camera 를 수집하던 @c SceneRenderer 로직 폐기 - context 가 진실의 원천.
		virtual void OnEnter() override;
		/// @brief @c SceneContext 에서 자동 해제.
		virtual void OnExit() override;
		/// @brief Camera 는 매 프레임 작업 없음 - no-op.
		virtual void Update(float dt) override
		{
		}

	  private:
		RenderTarget *mTargetRT  = nullptr; ///< 비소유 렌더 대상. nullptr = default backbuffer.
		const Actor  *mLockTarget = nullptr; ///< TargetLock 대상 Actor (비소유).
	};
} // namespace SJH::Scene

#endif // __SJH_SCENE_CAMERA_H__
