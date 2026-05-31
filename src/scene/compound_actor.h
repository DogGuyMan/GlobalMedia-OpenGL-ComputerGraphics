#ifndef __SJH_SCENE_COMPOUND_ACTOR_H__
#define __SJH_SCENE_COMPOUND_ACTOR_H__

#include "scene/actor.h"
#include <memory>
#include <string>
#include <vmath.h>

namespace SJH
{
    class Framebuffer;
    class Mesh;
    class Material;
}

namespace SJH::Scene
{
    /// @brief PreBuilt Actor 팩토리 — Unreal "Actor Class" 영감 + Cocos `Camera::createPerspective` 정통.
    /// @details
    ///   ### Compound Actor 컨벤션 (project memory: compound_actor_pattern)
    ///   - Actor 는 *비상속*. `class XxxActor : public Actor` 형태 금지.
    ///   - 특수 속성은 *Component* 부착으로만 부여 — ECS Bundle (Bevy) / Cocos Component 정통.
    ///   - 자주 쓰이는 Actor + Component 조합을 한 줄 호출로 생성하는 free factory 모음.

    // ── Camera ─────────────────────────────────────────────────────────────
    /// @brief CameraActor 생성 — Actor + Transform(기본) + Camera 컴포넌트.
    /// @details Camera 는 owner Actor 의 Transform 으로 view 도출 (standalone 불허).
    ///          명시: `actor->AddComponent<Camera>()` 직접 호출 *대신* 이 함수 사용 권장 —
    ///          Compound Actor 컨벤션 일관성 보장.
    std::unique_ptr<Actor> CreateCameraActor(
        std::string name,
        float fovYDeg = 45.0f,
        float aspect = 16.0f / 9.0f,
        float nearZ = 0.1f,
        float farZ = 100.0f);

    /// @brief PostFX 2-Camera 패턴의 Orthographic ScreenCamera Actor 생성.
    /// @details IsOrthographic=true + OrthoSize=1.0 + NoClear=true
    ///          + CullingMask(UI|Screen) + SetTargetRenderTarget(sceneFB).
    ///          기존 main.cpp 의 CreateAndRegisterScreenCamera() 22줄 보일러 추출.
    /// @return Actor UPtr — caller 가 Director::Root().AddChild 책임 (Pure factory).
    std::unique_ptr<Actor> CreateScreenCameraActor(
        std::string name,
        float aspect,
        Framebuffer* sceneFB);

    /// @brief Skybox Actor 생성 — Mesh + 큰 scale + MeshRenderer.
    /// @details 카메라 따라가기는 *셰이더 측* (vert shader 의 view matrix translation 제거)
    ///          으로 자동 처리. SyncSkyboxToCamera 자유 함수 불필요.
    /// @return Actor UPtr — caller 가 dir.Root().AddChild 책임.
    std::unique_ptr<Actor> CreateSkyboxActor(
        std::string name,
        Mesh* skyboxMesh,
        Material* skyboxMat,
        float scale = 50.0f);

    // ── Lights ─────────────────────────────────────────────────────────────
    /// @brief DirLightActor 생성 — Actor + Transform(방향 매핑) + DirLight 컴포넌트.
    /// @param direction 광원이 비추는 방향 (-Z = forward 정통). 내부에서 EulerRot 으로 변환.
    /// @details `DirLight::GetWorldDirection()` 결과가 입력 @p direction 과 일치하도록
    ///          Transform.EulerRot 자동 도출 (yaw=atan2(x,-z), pitch=asin(y)).
    std::unique_ptr<Actor> CreateDirLightActor(
        std::string name,
        vmath::vec3 direction = vmath::vec3(0.0f, -1.0f, 0.0f));

    /// @brief PointLightActor 생성 — Actor + Transform(위치 매핑) + PointLight 컴포넌트.
    /// @param position 광원 월드 좌표.
    /// @param distance 거리 감쇠 산출 기준 도달 거리 (PointLight::Distance).
    std::unique_ptr<Actor> CreatePointLightActor(
        std::string name,
        vmath::vec3 position,
        float distance = 32.0f);

    /// @brief SpotLightActor 생성 — Actor + Transform(위치 + 방향) + SpotLight 컴포넌트.
    /// @param position 광원 월드 좌표.
    /// @param direction 콘 축 방향 (-Z 정통).
    /// @param innerCutoffDeg 안쪽 컷오프 각도 (fully lit).
    /// @param outerCutoffDeg 바깥쪽 컷오프 각도 (fully dark, inner~outer 부드러운 감쇠).
    std::unique_ptr<Actor> CreateSpotLightActor(
        std::string name,
        vmath::vec3 position,
        vmath::vec3 direction = vmath::vec3(0.0f, -1.0f, 0.0f),
        float innerCutoffDeg = 12.5f,
        float outerCutoffDeg = 17.5f);
} // namespace SJH::Scene

#endif // __SJH_SCENE_COMPOUND_ACTOR_H__
