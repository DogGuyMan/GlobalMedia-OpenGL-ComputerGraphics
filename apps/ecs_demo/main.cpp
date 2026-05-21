#include <sb7.h>
#include <iostream>
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
#include <mach-o/dyld.h>   // _NSGetExecutablePath
#include <libgen.h>        // dirname
#include <unistd.h>        // chdir
#include <limits.h>        // PATH_MAX
#include <stdint.h>        // uint32_t
#include <string.h>        // strncpy
#endif

class ecs_demo_app : public sb7::application
{
public:
    void init() override
    {
        sb7::application::init();
        // 프로젝트 정책 — GL 4.1 Core / GLSL 410 강제
        info.majorVersion = 4;
        info.minorVersion = 1;

#ifdef __APPLE__
        // macOS GLFW 3.0.4 는 glfwInit() 시 _GLFW_USE_CHDIR 로 CWD 를
        // 앱 번들 Resources 경로로 변경한다. 셰이더 상대 경로를 살리기 위해
        // 실행 파일 디렉토리로 되돌린다.
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
        // 1) 모든 자원은 ResourceRegistry 에 위탁 — 챕터/app 은 자원 owner 아님.
        //    Cocos cc::Director::TextureCache / Unity Resources.Load 정통.
        //    자세한 컨벤션: .claude/architecture.md §11.3.
        auto& reg = SJH::ResourceRegistry::Get();

        auto* prog = reg.CreateProgram("simple",
                                       "resources/shaders/simple.vs",
                                       "resources/shaders/simple.fs");
        if (!prog) { std::cerr << "셰이더 로드 실패\n"; std::exit(1); }

        auto* mesh = reg.RegisterMesh("box", SJH::Mesh::CreateBox());
        auto* mat  = reg.CreateMaterial("box");
        mat->SetProgram(prog);

        // 2) Scene 에 Box Actor 추가 — 자원은 비소유 핸들만 보유.
        auto& director = SJH::Scene::Director::Get();
        auto box = std::make_unique<SJH::Scene::Actor>("Box");
        box->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
        box->AddComponent<SJH::Scene::MeshRenderer>(mesh, mat);
        director.Root().AddChild(std::move(box));

        // 3) MainCamera Actor — Camera 컴포넌트가 mOwner 따라가 InverseAffine 으로 view 계산.
        //    Translate(0,0,5) → 카메라가 +Z 5 단위에 위치, 원점을 바라봄.
        auto cam = std::make_unique<SJH::Scene::Actor>("MainCamera");
        cam->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 5.0f);
        const float aspect = static_cast<float>(info.windowWidth) / static_cast<float>(info.windowHeight);
        auto* camComp = cam->AddComponent<SJH::Scene::Camera>(45.0f, aspect, 0.1f, 100.0f);
        director.SetActiveCamera(camComp);
        director.Root().AddChild(std::move(cam));

        // 4) Scene 진입
        director.Enter();
        mLastTime = 0.0;
    }

    void render(double currentTime) override
    {
        const float dt = static_cast<float>(currentTime - mLastTime);
        mLastTime = currentTime;

        SJH::Scene::Director::Get().Update(dt);

        auto& rc = SJH::RenderContext::Get();
        rc.SetDefaultTargetSize(info.windowWidth, info.windowHeight);
        rc.BeginFrame(rc.GetDefaultTarget());

        // 윈도우 리사이즈 대응 — 활성 Camera 의 aspect 매 프레임 갱신.
        if (auto* cam = SJH::Scene::Director::Get().GetActiveCamera())
            cam->SetAspect(static_cast<float>(info.windowWidth) / static_cast<float>(info.windowHeight));

        // 활성 Camera 자동 조회 — SP3.5 의 가시적 효과. view/proj 인자 불필요.
        mRenderSys.Render();
    }

    void shutdown() override
    {
        SJH::Scene::Director::Get().Exit();
    }

private:
    // 챕터/app 멤버는 *씬과 시스템만*. 자원은 ResourceRegistry 가 owner.
    // .claude/architecture.md §11.3 컨벤션 적용 완료 (SP3.5).
    SJH::RenderSystem mRenderSys; // 시스템 — 자체 mQueue 보유, app 멤버 OK
    double            mLastTime = 0.0;
};

DECLARE_MAIN(ecs_demo_app);
