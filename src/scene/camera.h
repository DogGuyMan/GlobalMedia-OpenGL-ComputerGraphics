#ifndef __SJH_SCENE_CAMERA_H__
#define __SJH_SCENE_CAMERA_H__

#include "scene/actor.h" // Component + Actor::GetWorldMatrix
#include "scene/layer.h" // Layer, ToBits (SP5 Task 3)
#include <cstdint>       // uint64_t for cullingMask (SP5 Task 3)
#include <vmath.h>

namespace SJH
{
	class RenderTarget;
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
		bool  IsOrthographic = false;  ///< true 이면 ortho 투영 (Screen Camera 전용)
		float OrthoSize      = 1.0f;  ///< 반높이 — 가시 범위 [-OrthoSize, OrthoSize]
		/// RenderWithCamera 진입 시 BeginFrame(clear) 대신 BindTarget+state 만 수행.
		/// ScreenCamera 전용 — WorldCamera 출력을 보존한 채 합성.
		bool  NoClear        = false;
		float FovYDeg = 45.0f;
		float Aspect = 16.0f / 9.0f;
		float NearZ = 0.1f;
		float FarZ = 100.0f;

		// SP-SceneContext+ProgramRegistry (2026-05-26) — `int Depth` + `operator<` 폐기.
		// Camera 정렬은 SceneContext::mCameras 의 등록 순서로 자연 보장 (Cocos2D `addChild` 정통).
		uint64_t CullingMask = SJH::Scene::ToBits(SJH::Scene::Layer::All); // Unity Camera.cullingMask — 기본 모든 layer.

		/// @brief 비트마스크 직접 주입 (옛 호환).
		void SetCullingMask(uint64_t mask)
		{
			CullingMask = mask;
		}
		/// @brief type-safe Layer overload (SP5 Task 3).
		void SetCullingMask(SJH::Scene::Layer l)
		{
			CullingMask = SJH::Scene::ToBits(l);
		}

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
		/// @brief 렌더 대상 지정 — Unity Camera.targetTexture 정통. 의존 역전 — RenderTarget 추상.
		/// @details nullptr = default backbuffer. SceneRenderer 이 BeginFrame 시 자동 사용.
		///          Framebuffer(FBO) / DefaultRenderTarget / 미래 ShadowMap/MRT 등 모두 수용.
		void SetTargetRenderTarget(RenderTarget *rt)
		{
			mTargetRT = rt;
		}
		RenderTarget *GetTargetRenderTarget() const
		{
			return mTargetRT;
		}

		// SP-SceneContext+ProgramRegistry (2026-05-26) — Cocos cc::Camera 정통 자동 등록.
		// OnEnter 에서 Director::GetContext().AddCamera(this), OnExit 에서 Remove.
		// 매 프레임 Scene DFS 로 Camera 수집하던 SceneRenderer 로직 폐기 — context 가 진실의 원천.
		virtual void OnEnter() override;
		virtual void OnExit() override;
		virtual void Update(float dt) override
		{
		} // Camera 는 매 프레임 작업 없음.

		// SP-SceneContext+ProgramRegistry (2026-05-26) — `operator<` 폐기 (Depth 동반 제거).

	  private:
		/// @brief affine 4x4 (R|t) 행렬의 역행렬 — 회전 transpose + translate negate.
		/// @details 일반 inverse 아님. scale 1 가정. sb7 vmath 가 inverse 미제공이라 자작.
		static vmath::mat4 InverseAffine(const vmath::mat4 &m);

		// SP4 multi-pass — 렌더 대상. 의존 역전 — RenderTarget 추상 (Framebuffer/Default/Shadow 모두 수용).
		RenderTarget *mTargetRT = nullptr; // 비소유 — owner 는 App/Chapter (Option C).

		// TargetLock 상태 — 비소유 (target Actor 의 lifetime 책임 외부).
		const Actor *mLockTarget = nullptr;
	};
} // namespace SJH::Scene

#endif // __SJH_SCENE_CAMERA_H__
