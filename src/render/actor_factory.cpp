/**
 * @file actor_factory.cpp
 * @brief render-결합 Compound Actor 팩토리 구현 - Skybox / ScreenCamera.
 *
 * @details
 *  ### 책임
 *  - @c CreateScreenCameraActor - Orthographic ScreenCamera (PostFX 2-Camera 합성용).
 *  - @c CreateSkyboxActor - 큰 scale Mesh + MeshRenderer 조립.
 *
 *  ### 거주지 (2026-06-11 E2 사이클 해소로 scene -> render 이주)
 *  두 팩토리는 MeshRenderer(render)/Framebuffer(buffer) 상위 자원에 결합 - render 모듈 소속.
 *  Camera 조립은 scene 의 @c CreateCameraActor(compound_actor.h) 를 재사용한다 (render -> scene 단방향).
 *
 *  ### 비-책임
 *  - [X] 씬 트리 편입 - 반환된 @c unique_ptr 의 소유권 및 @c AddChild 는 호출자 책임.
 */
#include "render/actor_factory.h"
#include "scene/compound_actor.h" // CreateCameraActor 재사용
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/layer.h"
#include "render/mesh_renderer.h"  // SJH::Scene::MeshRenderer (render 헤더에 있지만 Scene namespace)
#include "object/mesh.h"           // SJH::Mesh 완전 타입
#include "material/material.h"     // SJH::Material 완전 타입
#include "buffer/framebuffer.h"    // SJH::Framebuffer 완전 타입 (SetTargetRenderTarget 인자)
#include <memory>
#include <string>
#include <utility>
#include <vmath.h>

namespace SJH::Scene
{
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
