/**
 * @file compound_actor.cpp
 * @brief Compound Actor 팩토리 free function 구현 - Camera / Light / Skybox / ScreenCamera.
 *
 * @details
 *  ### 책임
 *  - @c CreateCameraActor / @c CreateScreenCameraActor / @c CreateSkyboxActor 구현.
 *  - @c CreateDirLightActor / @c CreatePointLightActor / @c CreateSpotLightActor 구현.
 *  - 파일-스코프 익명 네임스페이스 @c DirectionToEulerDeg - direction -> EulerRot(deg) 변환.
 *
 *  ### 비-책임
 *  - [X] 씬 트리 편입 - 반환된 @c unique_ptr 의 소유권 및 @c AddChild 는 호출자 책임.
 *
 * @note @c DirectionToEulerDeg: OpenGL forward(-Z) + ZYX Euler 컨벤션.
 *       pitch = @c asin(d.y), yaw = @c atan2(d.x, -d.z), roll = 0.
 */
#include "scene/compound_actor.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/layer.h"
#include "object/light.h"
#include "object/transform.h"
#include "render/mesh_renderer.h"  // SJH::Scene::MeshRenderer (render 헤더에 있지만 Scene namespace)
#include "object/mesh.h"           // SJH::Mesh 완전 타입
#include "material/material.h"     // SJH::Material 완전 타입
#include "buffer/framebuffer.h"    // SJH::Framebuffer 완전 타입 (SetTargetRenderTarget 인자)
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vmath.h>

namespace SJH::Scene
{
    namespace
    {
        /// @brief direction vector  Transform.EulerRot (degree).
        /// @details OpenGL 정통 - Forward = -Z, Up = +Y. EulerRot=(pitch, yaw, 0) 의 ZYX 합성:
        ///   - pitch = asin(d.y)
        ///   - yaw   = atan2(d.x, -d.z)
        ///   - roll  = 0
        /// EulerRot=(0,0,0) 인 Transform 의 GetForward() = (0,0,-1) 와 일관.
        vmath::vec3 DirectionToEulerDeg(const vmath::vec3& dir)
        {
            constexpr float RAD2DEG = 57.295779513f;
            const auto d = vmath::normalize(dir);
            const float pitchRad = asinf(d[1]);
            const float yawRad   = atan2f(d[0], -d[2]);
            return vmath::vec3(pitchRad * RAD2DEG, yawRad * RAD2DEG, 0.0f);
        }
    }

    std::unique_ptr<Actor> CreateCameraActor(
        std::string name, float fovYDeg, float aspect, float nearZ, float farZ)
    {
        auto actor = std::make_unique<Actor>(std::move(name));
        actor->AddComponent<Camera>(fovYDeg, aspect, nearZ, farZ);
        return actor;
    }

    std::unique_ptr<Actor> CreateDirLightActor(std::string name, vmath::vec3 direction)
    {
        auto actor = std::make_unique<Actor>(std::move(name));
        actor->GetTransform().EulerRot = DirectionToEulerDeg(direction);
        actor->AddComponent<DirLight>();
        return actor;
    }

    std::unique_ptr<Actor> CreatePointLightActor(
        std::string name, vmath::vec3 position, float distance)
    {
        auto actor = std::make_unique<Actor>(std::move(name));
        actor->GetTransform().Translate = position;
        auto* light = actor->AddComponent<PointLight>();
        light->Distance = distance;
        return actor;
    }

    std::unique_ptr<Actor> CreateSpotLightActor(
        std::string name, vmath::vec3 position, vmath::vec3 direction,
        float innerCutoffDeg, float outerCutoffDeg)
    {
        auto actor = std::make_unique<Actor>(std::move(name));
        actor->GetTransform().Translate = position;
        actor->GetTransform().EulerRot = DirectionToEulerDeg(direction);
        auto* light = actor->AddComponent<SpotLight>();
        light->CutoffAngleDeg       = innerCutoffDeg;
        light->OuterCutoffAngleDeg  = outerCutoffDeg;
        return actor;
    }

    std::unique_ptr<Actor> CreateScreenCameraActor(
        std::string name,
        float aspect,
        Framebuffer* sceneFB)
    {
        // 기존 main.cpp::CreateAndRegisterScreenCamera 22줄 이주.
        auto screenCamActor = CreateCameraActor(std::move(name), 45.0f, aspect, -1.0f, 1.0f);
        auto* camera = screenCamActor->GetComponent<Camera>();
        camera->IsOrthographic = true;
        camera->OrthoSize      = 1.0f;
        camera->NoClear        = true; // WorldCamera 출력 보존 - clear 없이 합성
        camera
            ->SetCullingMask(Layer::UI | Layer::Screen)
            .SetTargetRenderTarget(sceneFB);
        return screenCamActor;
    }

    std::unique_ptr<Actor> CreateSkyboxActor(
        std::string name,
        Mesh* skyboxMesh,
        Material* skyboxMat,
        float scale)
    {
        auto skyboxActor = std::make_unique<Actor>(std::move(name));
        // 스카이박스 모델이 카메라 클리핑 범위를 벗어나지 않게 넉넉한 크기로 스케일.
        skyboxActor->GetTransform().Scale = vmath::vec3(scale, scale, scale);
        skyboxActor->AddComponent<MeshRenderer>(skyboxMesh, skyboxMat);
        return skyboxActor;
    }
} // namespace SJH::Scene
