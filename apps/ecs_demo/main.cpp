#include <sb7.h>
#include <iostream>
#include "render/render_context.h"
#include "render/render_system.h"
#include "scene/scene.h"
#include "scene/components.h"
#include "program/program.h"
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
        // 1) Program / Mesh / Material 리소스 생성
        mProgram = SJH::Program::CreateWithVSFS(
            "resources/shaders/simple.vs",
            "resources/shaders/simple.fs");
        if (!mProgram) { std::cerr << "셰이더 로드 실패\n"; std::exit(1); }

        mBoxMesh = SJH::Mesh::CreateBox();
        mBoxMat  = SJH::Material::Create();
        mBoxMat->SetProgram(mProgram.get());

        // 2) Scene 에 Box Actor 추가
        auto& director = SJH::Scene::Director::Get();
        auto box = std::make_unique<SJH::Scene::Actor>("Box");
        box->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
        box->AddComponent<SJH::Scene::MeshRenderer>(mBoxMesh.get(), mBoxMat.get());
        director.Root().AddChild(std::move(box));

        // 3) Scene 진입
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

        const float aspect = static_cast<float>(info.windowWidth) / static_cast<float>(info.windowHeight);
        const auto view = vmath::lookat(vmath::vec3(0,0,5), vmath::vec3(0), vmath::vec3(0,1,0));
        const auto proj = vmath::perspective(45.0f, aspect, 0.1f, 100.0f);

        mRenderSys.Render(view, proj);
    }

    void shutdown() override
    {
        SJH::Scene::Director::Get().Exit();
    }

private:
    // TODO(SP3.5/SP4): 자원 객체 owner 는 SJH::ResourceRegistry 로 위탁이 정통.
    //   - Material 은 *지금도* ResourceRegistry::CreateMaterial(key) 로 위탁 가능.
    //   - Program / Mesh 는 SP3 시점 ResourceRegistry 미지원 → 임시로 챕터 보유.
    //     CreateProgram(key, vs, fs) + RegisterMesh(key, MeshUPtr) 추가 후 위탁.
    //   - 자세한 컨벤션: .claude/architecture.md §11.3 (챕터 자원 보유 컨벤션).
    SJH::ProgramUPtr  mProgram;   // ⚠️  ResourceRegistry 확장 전까지 임시 보유
    SJH::MeshUPtr     mBoxMesh;   // ⚠️  ResourceRegistry 확장 전까지 임시 보유
    SJH::MaterialUPtr mBoxMat;    // ⚠️  ResourceRegistry::CreateMaterial 로 위탁 가능 (즉시 적용 후보)
    SJH::RenderSystem mRenderSys; // 시스템 — app 멤버 OK (자체 mQueue 보유)
    double            mLastTime = 0.0;   // 시간 상태 — app 한정 OK
};

DECLARE_MAIN(ecs_demo_app);
