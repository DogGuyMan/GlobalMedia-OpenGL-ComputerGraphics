#ifndef __SJH_SCENE_CAMERA_H__
#define __SJH_SCENE_CAMERA_H__

#include "scene/actor.h" // Component + Actor::GetWorldMatrix
#include <cstdint>       // uint32_t for cullingMask (SP4 D-15)
#include <vmath.h>

namespace SJH { class Framebuffer; }   // SetTargetFramebuffer 인자 forward decl (SP4 T2).

namespace SJH::Scene
{
    /// @brief Cocos cc::Camera / Unity Camera 정통 — view/proj 행렬 계산 컴포넌트.
    /// @details *하이브리드 모드*:
    ///   - mOwner 가 있으면 (Actor 에 부착됨) → view = InverseAffine(owner->GetWorldMatrix()).
    ///     Cocos 정통 — 부모 Actor 의 Transform 을 따름 (3rd person / 카메라 부착 시나리오).
    ///   - mOwner 가 없으면 (standalone) → view = vmath::lookat(mEye, mTarget, mUp).
    ///     standalone fallback — 단위 테스트 / 직접 인스턴스화 시.
    ///
    ///   InverseAffine 은 *회전+이동* 만 가정 (스케일 없음). 일반 4x4 inverse 가 아니므로
    ///   Actor 의 scale 이 1 이 아니면 결과 부정확. SP3.5 의 트레이드오프 — 학습 프로젝트에서
    ///   카메라 노드는 통상 scale 안 쓰므로 수용.
    ///
    ///   Projection 은 항상 자체 파라미터 (mFovYDeg / mAspect / mNearZ / mFarZ).
    ///   aspect 는 사용자가 매 프레임 SetAspect 로 갱신 책임 (RenderContext 의 window 크기 기반).
    class Camera : public Component
    {
    public:
        Camera() = default;
        Camera(float fovYDeg, float aspect, float nearZ, float farZ)
            : mFovYDeg(fovYDeg), mAspect(aspect), mNearZ(nearZ), mFarZ(farZ)
        {
        }

        /// @brief view 행렬 — mOwner 가 있으면 InverseAffine, 없으면 lookat (standalone).
        vmath::mat4 GetViewMatrix() const;

        /// @brief perspective(fovY, aspect, near, far).
        vmath::mat4 GetProjectionMatrix() const;

        // Standalone fallback 용 setter — mOwner 가 nullptr 일 때만 의미.
        void SetEye(const vmath::vec3& v) { mEye = v; }
        void SetTarget(const vmath::vec3& v) { mTarget = v; }
        void SetUp(const vmath::vec3& v) { mUp = v; }

        // Projection setter — POD-like, 불변식 없음.
        void SetFovY(float v) { mFovYDeg = v; }
        void SetAspect(float v) { mAspect = v; }
        void SetNearZ(float v) { mNearZ = v; }
        void SetFarZ(float v) { mFarZ = v; }

        // ── SP4 multi-pass ─────────────────────────────────────────────────────
        /// @brief 렌더 대상 FBO 지정 — Unity Camera.targetTexture 정통.
        /// @details nullptr = default backbuffer. RenderSystem 이 BeginFrame 시 자동 사용.
        void SetTargetFramebuffer(Framebuffer* fb) { mTargetFB = fb; }
        Framebuffer* GetTargetFramebuffer() const  { return mTargetFB; }

        /// @brief Camera 정렬 키 — Unity Camera.depth 정통. 작은 값이 먼저 렌더.
        /// @details Multi-pass 에서 SceneCamera(depth=0) → PostFXCamera(depth=1) 직렬.
        void SetDepth(int d) { mDepth = d; }
        int  GetDepth() const { return mDepth; }

        /// @brief 가시 객체 비트마스크 — Unity Camera.cullingMask 정통 (SP4 D-15).
        /// @details RenderSystem 이 (cam.GetCullingMask() & actor.GetLayer()) AND 로 필터.
        ///          기본 ~0u = 모든 layer (SP3.5 호환). postfx_demo 의 SceneFB self-sampling UB 회피.
        void     SetCullingMask(uint32_t mask) { mCullingMask = mask; }
        uint32_t GetCullingMask() const        { return mCullingMask; }

    private:
        /// @brief affine 4x4 (R|t) 행렬의 역행렬 — 회전 transpose + translate negate.
        /// @details 일반 inverse 아님. scale 1 가정. sb7 vmath 가 inverse 미제공이라 자작.
        static vmath::mat4 InverseAffine(const vmath::mat4& m);

        // Projection 파라미터.
        float mFovYDeg = 45.0f;
        float mAspect  = 16.0f / 9.0f;
        float mNearZ   = 0.1f;
        float mFarZ    = 100.0f;

        // Standalone fallback (mOwner 가 nullptr 일 때 사용).
        vmath::vec3 mEye    = vmath::vec3(0, 0, 5);
        vmath::vec3 mTarget = vmath::vec3(0, 0, 0);
        vmath::vec3 mUp     = vmath::vec3(0, 1, 0);

        // SP4 multi-pass — 렌더 대상 + 정렬 키 + 가시 mask.
        Framebuffer* mTargetFB     = nullptr;   // 비소유 — owner 는 App/Chapter (Option C).
        int          mDepth        = 0;         // Unity Camera.depth — 작은 값 먼저.
        uint32_t     mCullingMask  = ~0u;       // Unity Camera.cullingMask — 기본 모든 layer.
    };
} // namespace SJH::Scene

#endif // __SJH_SCENE_CAMERA_H__
