/**
 * @file main.cpp
 * @brief tweeny_demo — 11 easings ping-pong planes (Task 3: single plane sanity).
 *        Tweeny 도입 전 SJH API (Mesh + Program + Uniforms) 통합 동작 확인.
 */

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <sb7.h>
#include <cstring>

#include "object/mesh.h"
#include "program/program.h"
#include "program/program_uniforms.h"

#include <vmath.h>

#include <tweeny/tweeny.h>

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace
{
    struct EasingRow
    {
        const char*          name;
        tweeny::tween<float> tween;
        vmath::vec4          color;
        float                y;
        bool                 forward = true;
    };

    constexpr int   kRowCount        = 11;
    constexpr float kTweenFromX      = -0.85f;
    constexpr float kTweenToX        =  0.85f;
    constexpr int   kTweenDurationMs = 1000;
} // namespace

class tweeny_demo_app : public sb7::application
{
public:
    void init() override
    {
        sb7::application::init();
        info.majorVersion = 4;
        info.minorVersion = 1; // GLSL 410 (project policy)
        info.windowWidth  = 800;
        info.windowHeight = 600;
        // sb7::APPINFO::title 은 char[128] 배열 — 안전한 strncpy 로 복사.
#ifdef _WIN32
        strncpy_s(info.title, sizeof(info.title), "tweeny_demo - 11 easings ping-pong", _TRUNCATE);
#else
        std::strncpy(info.title, "tweeny_demo - 11 easings ping-pong", sizeof(info.title) - 1);
        info.title[sizeof(info.title) - 1] = '\0';
#endif
    }

    void startup() override
    {
        mQuad    = SJH::Mesh::CreateScreenQuad();
        mProgram = SJH::Program::CreateWithVSFS(
            "./shader/simple.vs",
            "./shader/simple.fs");

        std::mt19937                          rng{42u};
        std::uniform_real_distribution<float> distColor(0.3f, 1.0f);

        auto makeRow = [&](const char* name, auto&& easing) {
            EasingRow row;
            row.name  = name;
            row.tween = tweeny::from(kTweenFromX)
                            .to(kTweenToX)
                            .during(kTweenDurationMs)
                            .via(easing);
            row.color = vmath::vec4(distColor(rng), distColor(rng), distColor(rng), 1.0f);
            return row;
        };

        mRows.reserve(kRowCount);
        mRows.push_back(makeRow("linear",           tweeny::easing::linear));
        mRows.push_back(makeRow("quadraticInOut",   tweeny::easing::quadraticInOut));
        mRows.push_back(makeRow("cubicInOut",       tweeny::easing::cubicInOut));
        mRows.push_back(makeRow("quarticInOut",     tweeny::easing::quarticInOut));
        mRows.push_back(makeRow("quinticInOut",     tweeny::easing::quinticInOut));
        mRows.push_back(makeRow("sinusoidalInOut",  tweeny::easing::sinusoidalInOut));
        mRows.push_back(makeRow("exponentialInOut", tweeny::easing::exponentialInOut));
        mRows.push_back(makeRow("circularInOut",    tweeny::easing::circularInOut));
        mRows.push_back(makeRow("bounceInOut",      tweeny::easing::bounceInOut));
        mRows.push_back(makeRow("elasticInOut",     tweeny::easing::elasticInOut));
        mRows.push_back(makeRow("backInOut",        tweeny::easing::backInOut));

        // 11행을 [+0.9 .. -0.9] NDC 에 균등 분포 — i=0 위, i=10 아래.
        for (std::size_t i = 0; i < static_cast<std::size_t>(kRowCount); ++i)
        {
            float t    = (kRowCount == 1) ? 0.5f : static_cast<float>(i) / static_cast<float>(kRowCount - 1);
            mRows[i].y = 0.9f + (-0.9f - 0.9f) * t; // lerp(0.9, -0.9, t)
        }
    }

    void render(double currentTime) override
    {
        // delta-time (ms) — sb7 의 currentTime 은 초 단위 double.
        // tweeny step 오버로드 주의 — step(int32_t) 는 ms, step(float) 는 [0..1] 진행률 비율.
        // float 로 ms 를 넘기면 1600%/frame 진행 → 양 끝 즉시 도달 + 토글 폭주. int32_t 강제.
        static double prevTime = currentTime;
        int32_t       dtMs     = static_cast<int32_t>((currentTime - prevTime) * 1000.0);
        prevTime               = currentTime;
        if (dtMs < 0) dtMs = 0; // 첫 프레임 안전.

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(mProgram->GetProgramAddr());
        glBindVertexArray(mQuad->GetVAO());

        for (auto& row : mRows)
        {
            const float x = row.forward ? row.tween.step(dtMs) : row.tween.step(-dtMs);

            // 방향 토글 — 양 끝 도달 시 다음 프레임부터 반대 방향.
            if      ( row.forward && row.tween.progress() >= 1.0f) row.forward = false;
            else if (!row.forward && row.tween.progress() <= 0.0f) row.forward = true;

            SJH::Uniforms::SetVec2(*mProgram, "uOffset",   vmath::vec2(x, row.y));
            SJH::Uniforms::SetVec2(*mProgram, "uScale",    vmath::vec2(0.05f, 0.035f));
            SJH::Uniforms::SetVec4(*mProgram, "baseColor", row.color);

            glDrawElements(mQuad->GetPrimitiveType(),
                           mQuad->GetIndexCount(),
                           GL_UNSIGNED_INT,	
                           nullptr);
        }
    }

    void shutdown() override
    {
        mProgram.reset();
        mQuad.reset();
    }

private:
    SJH::MeshUPtr          mQuad;
    SJH::ProgramUPtr       mProgram;
    std::vector<EasingRow> mRows;
};

DECLARE_MAIN(tweeny_demo_app)
