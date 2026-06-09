/**
 * @file compound_actor.h
 * @brief PreBuilt Actor 팩토리 free function 모음 - Compound Actor 컨벤션.
 *
 * @details
 *  ### 책임
 *  - 자주 쓰이는 Actor + Component 조합을 한 줄 호출로 생성하는 free factory.
 *    Camera Actor / ScreenCamera / Skybox Actor / Dir/Point/SpotLight Actor.
 *  - Compound Actor 컨벤션 일관성 보장 - @c class XxxActor : public Actor 형태 금지.
 *
 *  ### 비-책임
 *  - [X] Actor 생명주기 관리 - 반환된 @c unique_ptr 의 소유권은 호출자(@c Director::Root().AddChild).
 *  - [X] 렌더링 - @c MeshRenderer Component 가 담당.
 *
 * @note 반환된 @c unique_ptr 을 @c Director::Root().AddChild 에 넘겨야 씬 트리에 편입되고
 *       OnEnter 가 호출된다 (Pure factory - 씬 편입 책임 호출자).
 */

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
    /**
     * @brief PreBuilt Actor 팩토리 - Unreal "Actor Class" 영감 + Cocos @c Camera::createPerspective 정통.
     * @details
     *  ### Compound Actor 컨벤션 (project memory: @c compound_actor_pattern)
     *  - Actor 는 *비상속*. @c class XxxActor : public Actor 형태 금지.
     *  - 특수 속성은 *Component* 부착으로만 부여 - ECS Bundle(Bevy) / Cocos Component 정통.
     *  - 자주 쓰이는 Actor + Component 조합을 한 줄 호출로 생성하는 free factory 모음.
     *
     *  ### 공통 반환 계약
     *  반환된 @c unique_ptr 을 @c Director::Root().AddChild 에 넘겨야 씬 트리에 편입되고
     *  @c OnEnter 가 호출된다 (Pure factory - 씬 편입 책임 호출자).
     */

    // -- Camera -------------------------------------------------------------
    /// @brief CameraActor 생성 - Actor + Transform(기본) + Camera 컴포넌트.
    /// @details Camera 는 owner Actor 의 Transform 으로 view 도출 (standalone 불허).
    ///          @c actor->AddComponent\<Camera\>() 직접 호출 *대신* 이 함수 사용 권장 -
    ///          Compound Actor 컨벤션 일관성 보장.
    /// @param name  Actor 이름.
    /// @param fovYDeg 수직 시야각 (degree).
    /// @param aspect  화면 비율 (width / height).
    /// @param nearZ   Near clipping 거리.
    /// @param farZ    Far clipping 거리.
    /// @return 비편입 Actor @c unique_ptr - 호출자가 @c AddChild 책임.
    std::unique_ptr<Actor> CreateCameraActor(
        std::string name,
        float fovYDeg = 45.0f,
        float aspect = 16.0f / 9.0f,
        float nearZ = 0.1f,
        float farZ = 100.0f);

    /// @brief PostFX 2-Camera 패턴의 Orthographic ScreenCamera Actor 생성.
    /// @details @c IsOrthographic=true, @c OrthoSize=1.0, @c NoClear=true,
    ///          @c CullingMask(UI|Screen), @c SetTargetRenderTarget(sceneFB).
    ///          기존 @c main.cpp 의 @c CreateAndRegisterScreenCamera() 22줄 보일러 추출.
    /// @param name     Actor 이름.
    /// @param aspect   화면 비율.
    /// @param sceneFB  WorldCamera 출력 FBO (비소유 - @c NoClear 합성 대상).
    /// @return 비편입 Actor @c unique_ptr - 호출자가 @c Director::Root().AddChild 책임.
    std::unique_ptr<Actor> CreateScreenCameraActor(
        std::string name,
        float aspect,
        Framebuffer* sceneFB);

    /// @brief Skybox Actor 생성 - @c Mesh + 큰 @c scale + @c MeshRenderer.
    /// @details 카메라 추적은 *셰이더* 측(vert shader view matrix translation 제거)으로 자동 처리.
    ///          @c SyncSkyboxToCamera 자유 함수 불필요.
    /// @param name       Actor 이름.
    /// @param skyboxMesh 큐브맵 메시 (비소유).
    /// @param skyboxMat  스카이박스 머티리얼 (비소유).
    /// @param scale      스케일 (world unit). 카메라 far plane 보다 크게 설정 권장.
    /// @return 비편입 Actor @c unique_ptr.
    std::unique_ptr<Actor> CreateSkyboxActor(
        std::string name,
        Mesh* skyboxMesh,
        Material* skyboxMat,
        float scale = 50.0f);

    // -- Lights -------------------------------------------------------------
    /// @brief DirLightActor 생성 - Actor + Transform(방향 매핑) + DirLight 컴포넌트.
    /// @details @c DirLight::GetWorldDirection() 결과가 입력 @p direction 과 일치하도록
    ///          @c Transform.EulerRot 자동 도출 (@c yaw=atan2(x,-z), @c pitch=asin(y)).
    /// @param name      Actor 이름.
    /// @param direction 광원이 비추는 방향 (-Z = forward 정통). 내부에서 EulerRot 으로 변환.
    /// @return 비편입 Actor @c unique_ptr.
    std::unique_ptr<Actor> CreateDirLightActor(
        std::string name,
        vmath::vec3 direction = vmath::vec3(0.0f, -1.0f, 0.0f));

    /// @brief PointLightActor 생성 - Actor + Transform(위치 매핑) + PointLight 컴포넌트.
    /// @param name     Actor 이름.
    /// @param position 광원 월드 좌표.
    /// @param distance 거리 감쇠 산출 기준 도달 거리 (@c PointLight::Distance).
    /// @return 비편입 Actor @c unique_ptr.
    std::unique_ptr<Actor> CreatePointLightActor(
        std::string name,
        vmath::vec3 position,
        float distance = 32.0f);

    /// @brief SpotLightActor 생성 - Actor + Transform(위치 + 방향) + SpotLight 컴포넌트.
    /// @param name          Actor 이름.
    /// @param position      광원 월드 좌표.
    /// @param direction     콘 축 방향 (-Z 정통).
    /// @param innerCutoffDeg 안쪽 컷오프 각도 (fully lit).
    /// @param outerCutoffDeg 바깥쪽 컷오프 각도 (fully dark, inner~outer 부드러운 감쇠).
    /// @return 비편입 Actor @c unique_ptr.
    std::unique_ptr<Actor> CreateSpotLightActor(
        std::string name,
        vmath::vec3 position,
        vmath::vec3 direction = vmath::vec3(0.0f, -1.0f, 0.0f),
        float innerCutoffDeg = 12.5f,
        float outerCutoffDeg = 17.5f);
} // namespace SJH::Scene

#endif // __SJH_SCENE_COMPOUND_ACTOR_H__
