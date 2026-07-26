# MyApp::UI — ImGui 의존 UI 레이어 (IImGuiLayer / ImGuiLayerStack / ImGuiPass / 각 Layer + UiBootstrap).
#
# 2026-07-26 신설. 이전에는 부모(apps/_MyApp_/CMakeLists.txt) 가 IMGUI_SRC / UI_SRC 변수로
#   소스 경로를 하드코딩해 add_executable 에 직접 나열하고, imgui 백엔드 include 경로와
#   -Wno-* 완화 플래그까지 executable 에 걸었다. 그 3가지를 본 타겟의 usage requirement 로 회수한다.

# === vendored ImGui v1.53 GLFW 백엔드 ===
# 코어(imgui::imgui)는 vcpkg(override 1.53) — game_deps 경유. 백엔드(imgui_impl_glfw_gl3)는
# vcpkg 1.53 port 에 없어 apps/_MyApp_/third_party/imgui 로 vendoring 한다.
# (v1.53 = GLFW 3.0.4 호환 마지막 태그; 신버전은 3.1+ 요구 -> sb7 vendored GLFW 와 충돌.
#  memory: imgui_v1_53_glfw_compat.)
set(MYAPP_IMGUI_BACKEND_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/imgui)

add_library(myapp_ui STATIC
    UiBootstrap.cpp
    ${MYAPP_IMGUI_BACKEND_DIR}/imgui_impl_glfw_gl3.cpp
)
add_library(MyApp::UI ALIAS myapp_ui)

# PUBLIC(src/..) : consumer 가 "UI/ImGuiLayerStack.h" 형식으로 include (main.cpp 가 6종 사용).
# PRIVATE : 자기 디렉토리 — 같은 폴더 헤더를 prefix 없이 include.
target_include_directories(myapp_ui
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# vendored 백엔드 디렉토리 — main.cpp 가 <imgui_impl_glfw_gl3.h> 를 직접 include 하므로 PUBLIC 전파.
# SYSTEM 인 이유 = 서드파티 헤더를 Debug 의 -Wall -Werror 대상에서 제외
#   (cmake/Dependency.cmake 의 game_deps SYSTEM include 와 동일 규율).
# 주의: SYSTEM 은 *타겟명 직후 1회* 만 유효한 전체-호출 플래그다. 위 호출의 중간에 끼워 넣으면
#   CMake 가 "SYSTEM" 을 include 경로 문자열로 먹어 src/UI/SYSTEM 같은 유령 경로가 생긴다
#   (2026-07-26 실측 확인). 그래서 SYSTEM 대상만 별도 호출로 분리한다.
target_include_directories(myapp_ui SYSTEM
    PUBLIC  $<BUILD_INTERFACE:${MYAPP_IMGUI_BACKEND_DIR}>
)

# 헤더 노출 의존 (PUBLIC — main.cpp 가 UI/*.h 를 include 하므로 그대로 전파돼야 함):
#   ImGuiPass.h        -> render/render_passable/render_passable.h, render/device_context.h  = SJH::render
#   PassDebugLayer.h   -> material/material.h, material_property_block.h                      = SJH::material
#                         render/pass_iterator.h                                              = SJH::render
#                         resource_registry/resource_registry.h                               = SJH::resource_registry
#                         Playable/Constants.h (PASS_* 효과명)                                = MyApp::Playable
#                         <spdlog/spdlog.h>                                                   = game_deps
#   PauseButtonLayer.h / StateOverlayLayer.h -> texture/texture.h                             = SJH::texture
#   전 Layer 헤더        -> <imgui.h>                                                          = game_deps (imgui::imgui)
#   백엔드 헤더/소스     -> <GL/gl3w.h>, <GLFW/glfw3.h>                                        = project_deps (sb7 + glfw3)
# .cpp 전용 의존 (PRIVATE):
#   UiBootstrap.cpp    -> texture/image.h (SJH::texture 는 이미 PUBLIC 으로 흡수)
target_link_libraries(myapp_ui
    PUBLIC
        SJH::render
        SJH::material
        SJH::resource_registry
        SJH::texture
        MyApp::Playable
        project_deps   # GL/gl3w.h + GLFW (백엔드 헤더가 GLFWwindow* 노출)
        game_deps      # imgui::imgui (v1.53 코어) + spdlog
)

# ImGui v1.53 은 deprecation / format 경고가 Debug 의 -Wall -Werror 와 충돌한다.
# Why PRIVATE: 원인 TU(백엔드 .cpp + UiBootstrap.cpp)에만 국소화 — 이전처럼 executable
#   전체에 걸면 main.cpp 를 포함한 게임 코드의 -Werror 까지 통째로 무력화된다.
if(NOT MSVC)
    target_compile_options(myapp_ui PRIVATE
        -Wno-error
        -Wno-deprecated-declarations
        -Wno-format-security)
endif()

target_compile_features(myapp_ui PUBLIC cxx_std_17)
