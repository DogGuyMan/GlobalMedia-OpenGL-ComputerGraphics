#ifndef __SJH_RENDER_SYSTEM_H__
#define __SJH_RENDER_SYSTEM_H__

#include "render/render_queue.h"
#include <vmath.h>

namespace SJH::Scene { class Actor; }
namespace SJH
{
    /// @brief Actor 트리 traverse → MeshRenderer 수집 → DrawCommand → Queue Flush.
    /// @details per-frame 호출:
    ///   1. Scene::Root() 부터 DFS — MeshRenderer 컴포넌트 수집
    ///   2. Actor::GetWorldMatrix() = model, depth = (view × model)[3].z
    ///   3. Queue 정렬 (Multi-stage)
    ///   4. Queue Flush → RenderContext
    ///
    class RenderSystem
    {
    public:
        /// @brief 활성 Camera 자동 조회 — Scene::Director::Get().GetActiveCamera() 사용.
        /// @details Cocos cc::Director::getRunningScene() + 카메라 자동 흐름과 동일 — 챕터/app 이
        ///          view/proj 직접 계산 불필요. CameraComponent 가 view (Actor Transform 따라가는
        ///          InverseAffine 또는 standalone lookat) + proj (perspective) 모두 제공.
        /// @note  활성 Camera 미지정 (Director::SetActiveCamera 안 함) 시 spdlog::warn + early return.
        ///        프레임에 아무것도 그리지 않음 — 화면 검은색이 명시적 실패 신호 (ddd Early Return).
        void Render();

        /// @brief 명시 view/proj — 단위 테스트 + 디버그용 (CameraComponent 우회).
        /// @details 기존 인터페이스 보존 — CameraComponent 없이 임의 view/proj 직접 주입 가능.
        void Render(const vmath::mat4& viewMat, const vmath::mat4& projMat);

    private:
        void CollectFromActor(const Scene::Actor& actor, const vmath::mat4& viewMat);

        RenderQueue mQueue;
    };
}

#endif // __SJH_RENDER_SYSTEM_H__
