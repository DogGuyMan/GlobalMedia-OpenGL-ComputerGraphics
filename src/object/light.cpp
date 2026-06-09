/**
 * @file light.cpp
 * @brief DirLight / PointLight / SpotLight 의 worldPos / worldDir 헬퍼 구현 +
 *        SP-SceneContext+ProgramRegistry OnEnter / OnExit lifecycle 구현.
 * @details
 *  ### 책임
 *  - `GetWorldDirection` / `GetWorldPosition` - Owner Actor 의 worldMatrix 에서 방향/위치 추출.
 *  - `OnEnter` / `OnExit` - Actor 트리 부착/해제 시 `SceneContext::AddLight` / `RemoveLight` 자동 호출.
 *
 *  ### 헤더/cpp 분리 이유
 *  - `light.h` 는 데이터 + `Scene::Component` 상속만 보유 - `Actor::GetWorldMatrix()` 정의 호출은
 *    link 의존을 유발하므로 cpp 로 격리. `SJH::object -> SJH::scene` 의존은 이 파일 한정.
 *  - 헤더는 `actor.h` 의 `Component` base 만 사용 (모두 inline -> link 의존 0).
 *
 * @note `GetWorldDirection` 은 worldMatrix 의 `-Z 컬럼` 을 forward 로 정의 -
 *       `Transform::GetForward()` / `Camera::GetViewMatrix` 와 일관 (EulerRot=0 기본 시 (0,0,-1)).
 */
#include "object/light.h"
#include "scene/actor.h" // Actor::GetWorldMatrix definition
#include "scene/scene.h" // SP-SceneContext+ProgramRegistry - Director::Get().GetContext() 접근.

namespace SJH
{
    vmath::vec3 DirLight::GetWorldDirection() const
    {
        // worldMatrix 의 -Z 컬럼 = forward (OpenGL 카메라 응시 방향 정통).
        // Transform::GetForward() / Camera::GetViewMatrix 와 일관 - EulerRot=(0,0,0) 기본 시 (0,0,-1).
        if (auto* owner = GetOwner())
        {
            const auto m = owner->GetWorldMatrix();
            vmath::vec3 forward(-m[2][0], -m[2][1], -m[2][2]);
            return vmath::normalize(forward);
        }
        return vmath::vec3(0.0f, 0.0f, -1.0f);
    }

    vmath::vec3 PointLight::GetWorldPosition() const
    {
        if (auto* owner = GetOwner())
        {
            const auto m = owner->GetWorldMatrix();
            return vmath::vec3(m[3][0], m[3][1], m[3][2]);
        }
        return vmath::vec3(0.0f, 0.0f, 0.0f);
    }

    vmath::vec3 SpotLight::GetWorldPosition() const
    {
        if (auto* owner = GetOwner())
        {
            const auto m = owner->GetWorldMatrix();
            return vmath::vec3(m[3][0], m[3][1], m[3][2]);
        }
        return vmath::vec3(0.0f, 0.0f, 0.0f);
    }

    vmath::vec3 SpotLight::GetWorldDirection() const
    {
        // worldMatrix 의 -Z 컬럼 = forward (DirLight 와 동일 컨벤션).
        if (auto* owner = GetOwner())
        {
            const auto m = owner->GetWorldMatrix();
            vmath::vec3 forward(-m[2][0], -m[2][1], -m[2][2]);
            return vmath::normalize(forward);
        }
        return vmath::vec3(0.0f, 0.0f, -1.0f);
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
