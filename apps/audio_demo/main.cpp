// audio_demo — FMOD Studio + ImGui 파라미터 데모 (single-file chapter).
// Task 3: FMOD Studio System 초기화/종료.
#include <sb7.h>

#include <fmod/fmod_common.h>
#include <fmod/fmod_studio_common.h>
#include <fmod/fmod_studio.hpp>
#include <fmod/fmod_errors.h>

#include <cstdio>
#include <cstdlib>

namespace {
// 학습 챕터 — 첫 실패에서 즉시 종료 (graceful degradation 안 함).
void ck(FMOD_RESULT r, const char* where) {
    if (r != FMOD_OK) {
        std::fprintf(stderr, "[FMOD] %s: %s\n", where, FMOD_ErrorString(r));
        std::exit(1);
    }
}
} // anon

class audio_demo_application : public sb7::application
{
    FMOD::Studio::System* mSystem = nullptr;

    void init() override
    {
        sb7::application::init();
        info.majorVersion = 4;
        info.minorVersion = 1;
        std::snprintf(info.title, sizeof(info.title), "FMOD Studio + ImGui Audio Demo");
    }

    void startup() override
    {
        ck(FMOD::Studio::System::create(&mSystem),                              "System::create");
        ck(mSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr),
                                                                                "System::initialize");
        std::fprintf(stderr, "[audio_demo] FMOD Studio system initialized.\n");
    }

    void render(double /*currentTime*/) override
    {
        if (mSystem) {
            ck(mSystem->update(), "System::update");
        }
        static const GLfloat clearColor[] = {0.10f, 0.10f, 0.12f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, clearColor);
    }

    void shutdown() override
    {
        if (mSystem) {
            ck(mSystem->release(), "System::release");
            mSystem = nullptr;
        }
    }
};

DECLARE_MAIN(audio_demo_application);
