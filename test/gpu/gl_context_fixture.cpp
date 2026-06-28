/**
 * @file gl_context_fixture.cpp
 * @brief C-0 GL 컨텍스트 픽스처 구현 + Catch2 글로벌 리스너 등록.
 */

#include "gl_context_fixture.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <cstdio>

namespace SJH::Test::Gpu
{
    GLFWwindow *GLContextFixture::sWindowPtr   = nullptr;
    bool        GLContextFixture::sInitialized = false;

    bool GLContextFixture::Init()
    {
        if (sInitialized)
        {
            return true;
        }

        if (!glfwInit())
        {
            std::fprintf(stderr, "[gpu] glfwInit 실패 - GL 컨텍스트 생성 불가\n");
            return false;
        }

        // GL 4.1 Core + 숨김 창. GLFW 3.0.4 라 GLFW_TRUE/FALSE 미정의 -> GL_TRUE/FALSE(1/0) 사용.
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // macOS Core 필수.

        // 작은 숨김 창. 모니터/공유 컨텍스트 없음.
        sWindowPtr = glfwCreateWindow(64, 64, "sjh_gpu_tests", nullptr, nullptr);
        if (sWindowPtr == nullptr)
        {
            std::fprintf(stderr, "[gpu] glfwCreateWindow 실패 - GL 4.1 Core 컨텍스트 불가\n");
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(sWindowPtr);

        // 로더 init - gl3wInit 은 0(GL3W_OK) 성공. 엔진과 동일 로더(sb7 정적 라이브러리 거주).
        if (gl3wInit() != 0)
        {
            std::fprintf(stderr, "[gpu] gl3wInit 실패 - GL 함수 포인터 로드 불가\n");
            glfwDestroyWindow(sWindowPtr);
            sWindowPtr = nullptr;
            glfwTerminate();
            return false;
        }

        sInitialized = true;
        return true;
    }

    void GLContextFixture::Shutdown()
    {
        if (!sInitialized)
        {
            return;
        }
        if (sWindowPtr != nullptr)
        {
            glfwDestroyWindow(sWindowPtr);
            sWindowPtr = nullptr;
        }
        glfwTerminate();
        sInitialized = false;
    }

    bool GLContextFixture::IsValid()
    {
        return sInitialized;
    }

    /// @brief 세션 경계에 컨텍스트 init/shutdown 을 거는 Catch2 글로벌 리스너.
    /// @details testRunStarting 에서 1회 init - 실패 시 메시지를 남기되 러너는 계속 진행
    ///          (개별 테스트가 IsValid 가정으로 의미있게 실패하도록).
    class GLContextListener : public Catch::EventListenerBase
    {
    public:
        using Catch::EventListenerBase::EventListenerBase;

        void testRunStarting(Catch::TestRunInfo const &) override
        {
            if (!GLContextFixture::Init())
            {
                std::fprintf(stderr,
                    "[gpu] 경고: GL 컨텍스트 생성 실패 - GPU 테스트는 실패할 것입니다.\n");
            }
        }

        void testRunEnded(Catch::TestRunStats const &) override
        {
            GLContextFixture::Shutdown();
        }
    };
}

// 리스너 등록 (전역). 이름은 진단용.
CATCH_REGISTER_LISTENER(SJH::Test::Gpu::GLContextListener)
