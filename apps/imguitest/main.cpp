// ImGui 통합 smoke test — ImGui::ShowDemoWindow() 가 정상 표시되는지 확인.
// 프로젝트 정책상 GL 4.1 Core / GLSL 410 강제 (macOS 한계 + wine 호환).
//
// 사용 ImGui 버전: v1.53 (2018-02). 본 프로젝트의 GLFW(3.0.4) 와 호환되는
// 마지막 ImGui 태그. v1.54+ 는 imgui_impl_glfw 가 GLFW 3.1+ cursor API를
// unconditional 하게 사용해 빌드 불가. sb7code 는 절대 변경 금지.
//
// v1.53 의 backend 는 GLFW + OpenGL3 결합형: ImGui_ImplGlfwGL3_*.
// ImGui_ImplGlfwGL3_Init 안에서 io.RenderDrawListsFn 을 설정하므로
// 사용자 코드는 ImGui::Render() 만 호출하면 자동 렌더링됨.
#include <sb7.h>

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include <cstdio>

class imguitest_application : public sb7::application
{
    ImGuiContext* mImGuiCtx = nullptr;

    void init() override
    {
        sb7::application::init();
        // 프로젝트 정책 — GL 4.1 Core / GLSL 410 강제.
        // sb7 base 의 macOS 기본값(3.2)을 4.1 로 강제 (Forward Compat + Core 는 base 가 이미 설정).
        info.majorVersion = 4;
        info.minorVersion = 1;
        std::snprintf(info.title, sizeof(info.title), "ImGui Integration Smoke Test (v1.53)");
    }

    void startup() override
    {
        // v1.53 의 CreateContext 는 ImGuiContext* 반환 — DestroyContext 에 전달 필요.
        // IMGUI_CHECKVERSION 매크로는 v1.53 에 없음.
        mImGuiCtx = ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // sb7::application 의 protected 멤버 GLFWwindow* window 를 그대로 사용.
        // install_callbacks=true 로 ImGui 가 GLFW 콜백을 가로채게 함 — sb7 의 키/마우스
        //   콜백은 imguitest 자체에서 사용하지 않으므로 충돌 없음.
        ImGui_ImplGlfwGL3_Init(window, true);
    }

    void render(double /*currentTime*/) override
    {
        static const GLfloat clearColor[] = {0.15f, 0.15f, 0.18f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, clearColor);

        ImGui_ImplGlfwGL3_NewFrame();

        // Demo window — ImGui 위젯/입력 통합 검증
        ImGui::ShowDemoWindow();

        // v1.53 은 io.RenderDrawListsFn 콜백을 통해 자동 렌더링.
        ImGui::Render();
    }

    void shutdown() override
    {
        ImGui_ImplGlfwGL3_Shutdown();
        ImGui::DestroyContext(mImGuiCtx);
    }
};

DECLARE_MAIN(imguitest_application);
