/**
 * @file light.cpp
 * @brief DirLight / PointLight / SpotLight 의 worldPos / worldDir 헬퍼 구현 +
 *        SP-SceneContext+ProgramRegistry OnEnter / OnExit lifecycle 구현.
 * @details
 *  ### 책임
 *  - `GetWorldDirection` / `GetWorldPosition` - Owner Actor 의 worldMatrix 에서 방향/위치 추출.
 *  - `OnEnter` / `OnExit` - Actor 트리 부착/해제 시 `SceneContext::AddLight` / `RemoveLight` 자동 호출.
 *
 *  ### 거주지 (2026-06-11 E1 사이클 해소로 이주)
 *  광원 컴포넌트는 scene 의 Director/SceneContext 를 직접 호출 - 본 모듈(scene) 에 거주.
 *  과거 object/light.cpp 시절 유발하던 object -> scene 역의존을 제거했다.
 *
 * @note `GetWorldDirection` 은 worldMatrix 의 `-Z 컬럼` 을 forward 로 정의 -
 *       `Transform::GetForward()` / `Camera::GetViewMatrix` 와 일관 (EulerRot=0 기본 시 (0,0,-1)).
 */
#include "scene/light.h"
#include "scene/actor.h" // Actor::GetWorldMatrix definition
#include "scene/scene.h" // SP-SceneContext+ProgramRegistry - Director::Get().GetContext() 접근.

namespace SJH
{
    glm::vec3 DirLight::GetWorldDirection() const
    {
        // worldMatrix 의 -Z 컬럼 = forward (OpenGL 카메라 응시 방향 정통).
        // Transform::GetForward() / Camera::GetViewMatrix 와 일관 - EulerRot=(0,0,0) 기본 시 (0,0,-1).
        if (auto* owner = GetOwner())
        {
            const auto m = owner->GetWorldMatrix();
            glm::vec3 forward(-m[2][0], -m[2][1], -m[2][2]);
            return glm::normalize(forward);
        }
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }

    glm::vec3 PointLight::GetWorldPosition() const
    {
        if (auto* owner = GetOwner())
        {
            const auto m = owner->GetWorldMatrix();
            return glm::vec3(m[3][0], m[3][1], m[3][2]);
        }
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }

    glm::vec3 SpotLight::GetWorldPosition() const
    {
        if (auto* owner = GetOwner())
        {
            const auto m = owner->GetWorldMatrix();
            return glm::vec3(m[3][0], m[3][1], m[3][2]);
        }
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }

    glm::vec3 SpotLight::GetWorldDirection() const
    {
        // worldMatrix 의 -Z 컬럼 = forward (DirLight 와 동일 컨벤션).
        if (auto* owner = GetOwner())
        {
            const auto m = owner->GetWorldMatrix();
            glm::vec3 forward(-m[2][0], -m[2][1], -m[2][2]);
            return glm::normalize(forward);
        }
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }

    // -- SP-SceneContext+ProgramRegistry (2026-05-26) - Component lifecycle hook ------
    // 광원이 Actor 트리에 부착되면 자동으로 SceneContext 에 등록 (Cocos2D `addChild` 정통).
    // SceneContext::AddLight 가 DirLight/PointLight/SpotLight 각각 오버로드 - this 그대로 전달.
    void DirLight::OnEnter()
    {
        Scene::Director::Get().GetContext().AddLight(this);
    }

    void DirLight::OnExit()
    {
        Scene::Director::Get().GetContext().RemoveLight(this);
    }

    void PointLight::OnEnter()
    {
        Scene::Director::Get().GetContext().AddLight(this);
    }

    void PointLight::OnExit()
    {
        Scene::Director::Get().GetContext().RemoveLight(this);
    }

    void SpotLight::OnEnter()
    {
        Scene::Director::Get().GetContext().AddLight(this);
    }

    void SpotLight::OnExit()
    {
        Scene::Director::Get().GetContext().RemoveLight(this);
    }
}
