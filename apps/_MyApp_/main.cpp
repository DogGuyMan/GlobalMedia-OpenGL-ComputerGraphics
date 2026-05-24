// M1 — Topdown Shooter (sprite billboard 1장 정적 표시)
//
// 본 main.cpp 는 SJH::sprite 모듈(UniformAtlas) + SJH::Shader/Program/Uniforms 를 묶어
// atlas 의 frame 0 을 카메라 정면 빌보드로 1장만 그려본다. ECS / 게임 루프 / 입력 / 물리
// 는 후속 마일스톤(M2~) 단계. spec §B.5, §10.

#include <sb7.h>
#include <vmath.h>

#include "sprite/uniform_atlas.h"
#include "shader/shader.h"
#include "program/program.h"
#include "program/program_uniforms.h"

#include <spdlog/spdlog.h>

#include <cstring>

#ifdef __APPLE__
#include <cstdint>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#endif

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
            info.flags.debug = 1;   // sb7 가 core profile + forward compat 는 unconditional 설정 — debug context 만 명시
            static const char title[] = "M1 — Topdown Shooter (sprite billboard)";
            std::memcpy(info.title, title, sizeof(title));

#ifdef __APPLE__
            // GLFW 3.0.4 의 cocoa_init.m 가 glfwInit 시 Contents/Resources 로 chdir 하므로
            // bundle 이 아닌 일반 binary 실행 시 CWD 가 엉뚱한 곳으로 옮겨진다.
            // 실행 파일 디렉터리로 명시적 복귀 (migrate_demo 와 동일 패턴).
            char exePath[PATH_MAX] = {};
            uint32_t exeSize = static_cast<uint32_t>(sizeof(exePath));
            if (_NSGetExecutablePath(exePath, &exeSize) == 0)
            {
                char exePathCopy[PATH_MAX] = {};
                std::strncpy(exePathCopy, exePath, PATH_MAX - 1);
                chdir(dirname(exePathCopy));
            }
#endif
        }

        void startup() override
        {
            // === 1. Atlas 로드 (TestPattern.png, 128px tile) ============================
            //  - resources/texture/TestPattern.png 는 POST_BUILD 단계에서 실행 파일 옆 resources/ 로 복사된다 (Task 8).
            if (!mAtlas.LoadFromPNG("resources/texture/TestPattern.png", 128))
            {
                spdlog::error("[_MyApp_] UniformAtlas::LoadFromPNG failed");
            }

            // === 2. Shader 컴파일 + Program 링크 (SJH::Shader / SJH::Program) ==========
            auto vs = SJH::Shader::CreateFromFile("resources/shaders/billboard_atlas.vert", GL_VERTEX_SHADER);
            auto fs = SJH::Shader::CreateFromFile("resources/shaders/billboard_atlas.frag", GL_FRAGMENT_SHADER);
            if (!vs || !fs)
            {
                spdlog::error("[_MyApp_] shader compile failed (vs={}, fs={})",
                              static_cast<void*>(vs.get()), static_cast<void*>(fs.get()));
                return;
            }
            std::vector<SJH::ShaderPtr> shaders;
            shaders.emplace_back(std::move(vs));
            shaders.emplace_back(std::move(fs));
            mProgram = SJH::Program::Create(shaders);
            if (!mProgram)
            {
                spdlog::error("[_MyApp_] Program::Create failed");
                return;
            }

            // === 3. quad VAO/VBO 생성 (2 triangles = 6 vertices, interleaved a_quad + a_uv) ==
            //   a_quad: (-0.5, -0.5) ~ (0.5, 0.5)
            //   a_uv  : (0, 0)       ~ (1, 1)         — atlas sub-rect 변환은 vertex shader 에서 u_uvRect 합성
            const float quadVerts[] = {
                // a_quad.xy    a_uv.xy
                -0.5f, -0.5f,  0.0f, 1.0f,   // bottom-left  (V flipped: stbi 가 V=0 을 위로 둠)
                 0.5f, -0.5f,  1.0f, 1.0f,   // bottom-right
                 0.5f,  0.5f,  1.0f, 0.0f,   // top-right

                -0.5f, -0.5f,  0.0f, 1.0f,   // bottom-left
                 0.5f,  0.5f,  1.0f, 0.0f,   // top-right
                -0.5f,  0.5f,  0.0f, 0.0f,   // top-left
            };

            glGenVertexArrays(1, &mVao);
            glBindVertexArray(mVao);

            glGenBuffers(1, &mVbo);
            glBindBuffer(GL_ARRAY_BUFFER, mVbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

            // location 0 — a_quad (vec2), stride = 4 floats
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
            // location 1 — a_uv (vec2), offset = 2 floats
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

            glBindVertexArray(0);

            // === 4. 일회성 GL 상태 ===
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);   // alpha-test 만 사용 (frag discard)
            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

            spdlog::info("[M1] startup complete");
        }

        void render(double /*currentTime*/) override
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (!mProgram || mAtlas.TextureId() == 0)
                return;

            glUseProgram(mProgram->GetProgramAddr());

            // === 카메라 — Y축 위, +Z 후방에서 원점 응시 ===
            const vmath::mat4 proj = vmath::perspective(45.0f, static_cast<float>(info.windowWidth) / static_cast<float>(info.windowHeight), 0.1f, 100.0f);
            const vmath::mat4 view = vmath::lookat(
                vmath::vec3(0.0f, 5.0f, 5.0f),     // eye
                vmath::vec3(0.0f, 0.0f, 0.0f),     // center
                vmath::vec3(0.0f, 1.0f, 0.0f));    // up

            // === Uniform 송신 (SJH::Uniforms 자유 함수 family) ===
            SJH::Uniforms::SetMat4(*mProgram, "u_view", view);
            SJH::Uniforms::SetMat4(*mProgram, "u_proj", proj);
            SJH::Uniforms::SetVec3(*mProgram, "u_billboardCenter", vmath::vec3(0.0f, 0.0f, 0.0f));
            SJH::Uniforms::SetVec2(*mProgram, "u_billboardSize",   vmath::vec2(1.5f, 1.5f));
            SJH::Uniforms::SetFloat(*mProgram, "u_flipX", 1.0f);
            SJH::Uniforms::SetVec4(*mProgram, "u_uvRect", mAtlas.GetUVRect(0));   // frame 0
            SJH::Uniforms::SetVec4(*mProgram, "u_tint",   vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            SJH::Uniforms::SetInt (*mProgram, "u_atlas", 0);

            // === Atlas 텍스처 unit 0 바인딩 + draw ===
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mAtlas.TextureId());

            glBindVertexArray(mVao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glUseProgram(0);
        }

        void shutdown() override
        {
            if (mVbo) { glDeleteBuffers(1, &mVbo); mVbo = 0; }
            if (mVao) { glDeleteVertexArrays(1, &mVao); mVao = 0; }
            mProgram.reset();   // SJH::Program::~Program 가 glDeleteProgram
            mAtlas.Release();   // SJH::Texture::~Texture 가 glDeleteTextures
        }

    private:
        SJH::Sprite::UniformAtlas mAtlas;
        SJH::ProgramUPtr          mProgram;
        GLuint                    mVao = 0;
        GLuint                    mVbo = 0;
    };

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
