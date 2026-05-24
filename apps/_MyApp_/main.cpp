/**
 * @file main.cpp
 * @brief M1.5 — SJH::Mesh::CreatePlane + SJH::Scene::Director/Camera + spherical billboard.
 *        Q1 (빌보드 정면) + Q4 (Mesh::CreatePlane 의존) 통합 fix.
 */

#include <sb7.h>
#include <GL/gl3w.h>
#include <vmath.h>
#include <spdlog/spdlog.h>

// macOS GLFW chdir workaround (migrate_demo 정통)
#ifdef __APPLE__
#include <cstdint>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#endif

#include "sprite/uniform_atlas.h"
#include "shader/shader.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "object/mesh.h"
#include "scene/scene.h"
#include "scene/actor.h"
#include "scene/camera.h"

#include <cstring>
#include <memory>
#include <vector>

namespace TopdownShooter
{

class game_application : public sb7::application
{
public:
    void init() override
    {
        sb7::application::init();
        info.majorVersion = 4;
        info.minorVersion = 1;
        info.flags.debug = 1;
        static const char title[] = "M1.5 — Billboard + Mesh::CreatePlane + Scene::Camera";
        std::memcpy(info.title, title, sizeof(title));

#ifdef __APPLE__
        // macOS GLFW 3.0.4 의 chdir 복귀 — resources/ 상대 경로 정합
        char exePath[PATH_MAX] = {};
        uint32_t exeSize = static_cast<uint32_t>(sizeof(exePath));
        if (_NSGetExecutablePath(exePath, &exeSize) == 0) {
            char exePathCopy[PATH_MAX] = {};
            std::strncpy(exePathCopy, exePath, PATH_MAX - 1);
            chdir(dirname(exePathCopy));
        }
#endif
    }

    void startup() override
    {
        // === 1. atlas 로드 (M1 그대로) ===
        if (!mAtlas.LoadFromPNG("resources/texture/TestPattern.png", /*tilePx=*/128)) {
            spdlog::error("[M1.5] atlas load failed");
            return;
        }

        // === 2. 셰이더 ===
        auto vs = SJH::Shader::CreateFromFile("resources/shaders/billboard_atlas.vert", GL_VERTEX_SHADER);
        auto fs = SJH::Shader::CreateFromFile("resources/shaders/billboard_atlas.frag", GL_FRAGMENT_SHADER);
        if (!vs || !fs) {
            spdlog::error("[M1.5] shader compile failed");
            return;
        }
        std::vector<SJH::ShaderPtr> shaders;
        shaders.emplace_back(std::move(vs));
        shaders.emplace_back(std::move(fs));
        mProgram = SJH::Program::Create(shaders);
        if (!mProgram) {
            spdlog::error("[M1.5] program link failed");
            return;
        }

        // === 3. Mesh — Mesh::CreatePlane (XY 평면 1x1 quad, indexed) ===
        mPlane = SJH::Mesh::CreatePlane();
        if (!mPlane) {
            spdlog::error("[M1.5] Mesh::CreatePlane failed");
            return;
        }

        // === 4. Scene::Director + Camera Component 셋업 ===
        // Camera 는 Actor 의 Component 라 root 의 child Actor 생성 후 부착.
        auto& director = SJH::Scene::Director::Get();
        auto& root     = director.Root();
        auto  camActor = std::make_unique<SJH::Scene::Actor>("MainCameraActor");

        // 카메라 위치/방향 — Transform 의 Translate 가 진실의 원천.
        // EulerRot 으로 pitch 약간 (위에서 내려다보는 탑다운 시점)
        auto& camTransform = camActor->GetTransform();
        camTransform.Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
        camTransform.EulerRot  = vmath::vec3(-45.0f, 0.0f, 0.0f);   // pitch -45° (위에서 내려다봄)

        // Camera Component 부착 — projection 파라미터 직접 설정
        auto* cam = camActor->AddComponent<SJH::Scene::Camera>();
        cam->FovYDeg = 45.0f;
        cam->Aspect  = static_cast<float>(info.windowWidth) / static_cast<float>(info.windowHeight);
        cam->NearZ   = 0.1f;
        cam->FarZ    = 100.0f;

        // Director 의 활성 카메라 슬롯 + root 의 child 등록
        mCameraActor = root.AddChild(std::move(camActor));
        mCamera      = cam;
        director.SetActiveCamera(cam);
        director.Enter();   // root + child + Camera 의 OnEnter 캐스케이드

        // === 5. 일회성 GL 상태 ===
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

        spdlog::info("[M1.5] startup complete");
    }

    void render(double currentTime) override
    {
        (void)currentTime;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!mProgram || mAtlas.TextureId() == 0 || !mPlane || !mCamera) return;

        // === Camera 의 view/proj 가져오기 ===
        // Aspect 매 프레임 갱신 (resize 안전)
        mCamera->Aspect = static_cast<float>(info.windowWidth) / static_cast<float>(info.windowHeight);
        const vmath::mat4 view = mCamera->GetViewMatrix();
        const vmath::mat4 proj = mCamera->GetProjectionMatrix();

        glUseProgram(mProgram->GetProgramAddr());

        // D1 — uModel 이 빌보드 center (Translate) + size (Scale) 흡수.
        // Actor 부착 전 M1.5 단계라 임시 identity 송신: center=(0,0,0), scale=(1,1) 동치.
        SJH::Uniforms::SetMat4(*mProgram, "uModel", vmath::mat4::identity());
        SJH::Uniforms::SetMat4(*mProgram, "uView", view);
        SJH::Uniforms::SetMat4(*mProgram, "uProj", proj);
        SJH::Uniforms::SetFloat(*mProgram, "uFlipX", 1.0f);

        const vmath::vec4 uvRect = mAtlas.GetUVRect(/*frameIdx=*/0);
        SJH::Uniforms::SetVec4(*mProgram, "uUvRect", uvRect);
        SJH::Uniforms::SetVec4(*mProgram, "uTint", vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // === atlas 텍스처 ===
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mAtlas.TextureId());
        SJH::Uniforms::SetInt(*mProgram, "uAtlas", 0);

        // === draw — Mesh 의 VAO + indexed draw ===
        glBindVertexArray(mPlane->GetVAO());
        glDrawElements(GL_TRIANGLES, mPlane->GetIndexCount(), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        glUseProgram(0);
    }

    void shutdown() override
    {
        // Director 트리 lifecycle 종료 (Camera Component OnExit 등)
        SJH::Scene::Director::Get().SetActiveCamera(nullptr);
        SJH::Scene::Director::Get().Exit();
        mCamera = nullptr;
        mCameraActor = nullptr;   // root.~Actor 가 child 소멸 처리

        mPlane.reset();
        mProgram.reset();
        mAtlas.Release();
    }

private:
    SJH::Sprite::UniformAtlas        mAtlas;
    SJH::ProgramUPtr                 mProgram;
    SJH::MeshUPtr                    mPlane;
    SJH::Scene::Actor*               mCameraActor = nullptr;   // 비소유 — Director root 가 child 로 owns
    SJH::Scene::Camera*              mCamera      = nullptr;   // 비소유 — camera Actor 가 Component owns
};

}  // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
