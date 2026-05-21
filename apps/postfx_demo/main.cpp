#include <sb7.h>
#include <iostream>
#include "buffer/framebuffer.h"
#include "render/render_context.h"
#include "render/render_system.h"
#include "resource_registry/resource_registry.h"
#include "scene/scene.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/components.h"
#include "object/mesh.h"
#include "material/material.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libgen.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#endif

class postfx_demo_app : public sb7::application
{
public:
    void init() override
    {
        sb7::application::init();
        info.majorVersion = 4;
        info.minorVersion = 1;

#ifdef __APPLE__
        // macOS GLFW _GLFW_USE_CHDIR fix — ecs_demo 와 동일 패턴.
        char exePath[PATH_MAX] = {};
        uint32_t exeSize = static_cast<uint32_t>(sizeof(exePath));
        if (_NSGetExecutablePath(exePath, &exeSize) == 0)
        {
            // dirname 은 경로를 in-place 수정할 수 있으므로 복사본 사용
            char exePathCopy[PATH_MAX] = {};
            strncpy(exePathCopy, exePath, PATH_MAX - 1);
            chdir(dirname(exePathCopy));
        }
#endif
    }

    void startup() override
    {
        auto& reg = SJH::ResourceRegistry::Get();

        // 1) Scene shader (Box 그리기) + PostFX shader (Quad 반전).
        auto* sceneProg = reg.CreateProgram("scene",
                                            "resources/shaders/simple.vs",
                                            "resources/shaders/simple.fs");
        if (!sceneProg) { std::cerr << "scene shader 로드 실패\n"; std::exit(1); }

        auto* postfxProg = reg.CreateProgram("postfx_invert",
                                             "resources/shaders/postprocess/postprocess.vs",
                                             "resources/shaders/postprocess/invert.fs");
        if (!postfxProg) { std::cerr << "postfx shader 로드 실패\n"; std::exit(1); }

        // SP6 — manual glUniform1i(uScene, 0) 제거. Material 의 properties bag (Textures["uScene"])
        // 에 unit 0 와 함께 store -> MaterialApplier::Apply 가 sampler slot + 텍스처 바인딩 일괄.

        // 2) Mesh — Box (3D) + ScreenQuad (NDC).
        auto* boxMesh  = reg.RegisterMesh("box",         SJH::Mesh::CreateBox());
        auto* quadMesh = reg.RegisterMesh("screen_quad", SJH::Mesh::CreateScreenQuad());

        // 3) Material — Box 용 (단색) + PostFX 용 (sampler 로 SceneFB color 읽기).
        auto* boxMat = reg.CreateMaterial("box");
        boxMat->SetProgram(sceneProg);

        auto* postfxMat = reg.CreateMaterial("postfx_invert");
        postfxMat->SetProgram(postfxProg);

        // 4) Framebuffer — App 보유 (SP4 D-11: Option C 정통).
        mSceneFB = SJH::Framebuffer::Create(info.windowWidth, info.windowHeight);
        if (!mSceneFB) { std::cerr << "SceneFB 생성 실패\n"; std::exit(1); }

        // 5) SP6 — Material properties bag 에 sampler 슬롯 직접 store (Unity 정통).
        //    invert.fs 의 uniform sampler2D uScene 와 1:1 매핑.
        //    Apply 시점에 UniformCache 교집합 검사 + glBindTexture + SetInt(uScene, 0) 일괄.
        postfxMat->Textures["uScene"] = { mSceneFB->GetColorAttachment().get(), /*unit*/ 0 };

        // 6) Scene 구성 — layer + cullingMask 로 카메라별 가시성 분리 (SP4 D-15).
        //    Box.layer = 1 (비트 0), Quad.layer = 2 (비트 1).
        //    SceneCamera.mask = 1 (Box 만), PostFXCamera.mask = 2 (Quad 만).
        auto& dir = SJH::Scene::Director::Get();

        constexpr uint32_t LAYER_SCENE  = 1u;  // 비트 0
        constexpr uint32_t LAYER_POSTFX = 2u;  // 비트 1

        // Box Actor — SceneCamera 가 SceneFB 에 그림.
        auto box = std::make_unique<SJH::Scene::Actor>("Box");
        box->SetLayer(LAYER_SCENE);
        box->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
        box->AddComponent<SJH::Scene::MeshRenderer>(boxMesh, boxMat);
        dir.Root().AddChild(std::move(box));

        // ScreenQuad Actor — PostFXCamera 가 backbuffer 에 그림 (SceneFB sampler 로 읽음).
        auto quad = std::make_unique<SJH::Scene::Actor>("PostFXQuad");
        quad->SetLayer(LAYER_POSTFX);
        quad->AddComponent<SJH::Scene::MeshRenderer>(quadMesh, postfxMat);
        dir.Root().AddChild(std::move(quad));

        // SceneCamera (depth=0, target=SceneFB, mask=LAYER_SCENE).
        auto sceneCam = std::make_unique<SJH::Scene::Actor>("SceneCamera");
        sceneCam->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 5.0f);
        const float aspect = static_cast<float>(info.windowWidth) /
                             static_cast<float>(info.windowHeight);
        auto* sc = sceneCam->AddComponent<SJH::Scene::Camera>(45.0f, aspect, 0.1f, 100.0f);
        sc->Depth = 0;
        sc->SetTargetFramebuffer(mSceneFB.get());
        sc->CullingMask = LAYER_SCENE;
        dir.Root().AddChild(std::move(sceneCam));

        // PostFXCamera (depth=1, target=nullptr=backbuffer, mask=LAYER_POSTFX).
        auto fxCam = std::make_unique<SJH::Scene::Actor>("PostFXCamera");
        auto* fc = fxCam->AddComponent<SJH::Scene::Camera>(45.0f, aspect, 0.1f, 100.0f);
        fc->Depth = 1;
        // SetTargetFramebuffer 안 부름 -> nullptr = default backbuffer.
        fc->CullingMask = LAYER_POSTFX;
        dir.Root().AddChild(std::move(fxCam));

        dir.Enter();
        mLastTime = 0.0;
    }

    void render(double currentTime) override
    {
        const float dt = static_cast<float>(currentTime - mLastTime);
        mLastTime = currentTime;

        SJH::Scene::Director::Get().Update(dt);

        // RenderContext default target 의 size 갱신.
        SJH::RenderContext::Get().SetDefaultTargetSize(info.windowWidth, info.windowHeight);

        // SP3.5 의 인자 없는 overload — 모든 Camera 자동 직렬 렌더.
        mRenderSys.Render();
    }

    void shutdown() override
    {
        SJH::Scene::Director::Get().Exit();
    }

private:
    // App/Chapter 보유 자원 — SP4 D-11: Framebuffer 는 사용자 컨텐츠라 ResourceRegistry 안 씀.
    SJH::FramebufferUPtr mSceneFB;
    SJH::RenderSystem    mRenderSys;
    double               mLastTime = 0.0;
};

DECLARE_MAIN(postfx_demo_app);
