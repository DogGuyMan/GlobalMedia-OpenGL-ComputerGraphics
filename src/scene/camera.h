#ifndef __SJH_SCENE_CAMERA_H__
#define __SJH_SCENE_CAMERA_H__

#include "scene/actor.h"  // Component + Actor::GetWorldMatrix
#include "scene/layer.h"  // Layer, ToBits (SP5 Task 3)
#include <cstdint>        // uint64_t for cullingMask (SP5 Task 3)
#include <vmath.h>

namespace SJH
{
	class Framebuffer;
} // namespace SJH

namespace SJH::Scene
{
	/// @brief Cocos cc::Camera / Unity Camera 정통 — view/proj 행렬 계산 컴포넌트.
	/// @details
	///   ### Transform 강제 의존 (Compound Actor 컨벤션)
	///   Camera 는 *반드시* Actor 에 부착되어야 한다. `Scene::CreateCameraActor(...)` factory 가
	///   유일한 정상 생성 경로. owner Transform 의 EulerRot/Translate 가 view 의 진실의 원천.
	///   forward = owner.WorldMatrix 의 -Z 컬럼.
	///
	///   ### TargetLock — Unity Cinemachine Composer / Unreal SpringArm 정통
	///   `TargetLock(targetActor)` 호출 시 GetViewMatrix() 가 *owner.Translate -> target.WorldPos*
	///   를 lookat 하는 행렬 반환. owner EulerRot 무시 (CameraController 의 yaw/pitch 갱신은
	///   *상태로만 보존* 되고 시각 효과는 lock 동안 무효). `TargetRelease()` 로 해제 시 마지막
	///   EulerRot 으로 자유 시점 복귀.
	///
	///   ### Projection
	///   Projection 은 항상 자체 파라미터 (FovYDeg / Aspect / NearZ / FarZ).
	///   aspect 는 사용자가 매 프레임 갱신 책임 (DeviceContext 의 window 크기 기반).
	class Camera : public Component
	{
	  public:
		// Projection 파라미터.
		float FovYDeg = 45.0f;
		float Aspect = 16.0f / 9.0f;
		float NearZ = 0.1f;
		float FarZ = 100.0f;

		int Depth = 0;                                                // Unity Camera.depth — 작은 값 먼저.
		uint64_t CullingMask = SJH::Scene::ToBits(SJH::Scene::Layer::All); // Unity Camera.cullingMask — 기본 모든 layer.

		/// @brief 비트마스크 직접 주입 (옛 호환).
		void SetCullingMask(uint64_t mask)             { CullingMask = mask; }
		/// @brief type-safe Layer overload (SP5 Task 3).
		void SetCullingMask(SJH::Scene::Layer l)       { CullingMask = SJH::Scene::ToBits(l); }

		Camera() = default;
		Camera(float fovYDeg, float aspect, float nearZ, float farZ)
		    : FovYDeg(fovYDeg), Aspect(aspect), NearZ(nearZ), FarZ(farZ)
		{
		}

		/// @brief view 행렬 — owner Transform 기반. Lock 중이면 lookat(owner, target).
		/// @note owner 미부착 시 assert. `Scene::CreateCameraActor` 로 생성하면 자동 충족.
		vmath::mat4 GetViewMatrix() const;

		/// @brief perspective(fovY, aspect, near, far).
		vmath::mat4 GetProjectionMatrix() const;

		// ── TargetLock — Unity Cinemachine Composer 정통 ─────────────────────
		/// @brief 특정 Actor 를 바라보도록 카메라 lock. owner EulerRot 시각 효과 일시 무효.
		/// @param target 바라볼 대상 Actor. nullptr 이면 TargetRelease 와 동등.
		void TargetLock(const Actor *target)
		{
			mLockTarget = target;
		}

		/// @brief Lock 해제 — owner Transform 의 EulerRot 다시 view 도출에 사용.
		void TargetRelease()
		{
			mLockTarget = nullptr;
		}

		bool IsLocked() const
		{
			return mLockTarget != nullptr;
		}
		const Actor *GetLockTarget() const
		{
			return mLockTarget;
		}

		// ── SP4 multi-pass ─────────────────────────────────────────────────────
		/// @brief 렌더 대상 FBO 지정 — Unity Camera.targetTexture 정통.
		/// @details nullptr = default backbuffer. SceneRenderer 이 BeginFrame 시 자동 사용.
		void SetTargetFramebuffer(Framebuffer *fb)
		{
			mTargetFB = fb;
		}
		Framebuffer *GetTargetFramebuffer() const
		{
			return mTargetFB;
		}

		virtual void OnEnter() override
		{
		}
		virtual void OnExit() override
		{
		}
		virtual void Update(float dt) override
		{
		}

	  private:
		/// @brief affine 4x4 (R|t) 행렬의 역행렬 — 회전 transpose + translate negate.
		/// @details 일반 inverse 아님. scale 1 가정. sb7 vmath 가 inverse 미제공이라 자작.
		static vmath::mat4 InverseAffine(const vmath::mat4 &m);

		// SP4 multi-pass — 렌더 대상.
		Framebuffer *mTargetFB = nullptr; // 비소유 — owner 는 App/Chapter (Option C).

		// TargetLock 상태 — 비소유 (target Actor 의 lifetime 책임 외부).
		const Actor *mLockTarget = nullptr;
	};
} // namespace SJH::Scene

#endif // __SJH_SCENE_CAMERA_H__
