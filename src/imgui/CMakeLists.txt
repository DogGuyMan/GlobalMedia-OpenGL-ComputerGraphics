# SJH::imgui — ocornut/imgui v1.53 의 소스를 인라인 컴파일하는 STATIC 라이브러리.
# 본 프로젝트의 GLFW(3.0.4) 와 호환되는 마지막 ImGui 태그가 v1.53 (2018-02). 그 이후
# 모든 버전은 imgui_impl_glfw.cpp 가 GLFW 3.1+ 의 cursor API(GLFWcursor 등)를
# unconditional 하게 사용해 빌드 불가. sb7code 는 절대 변경 금지.
#
# v1.53 의 backend 는 단일 결합 파일(imgui_impl_glfw_gl3.cpp/.h) 이고
# <GL/gl3w.h> 를 직접 포함 — sb7 의 gl3w 와 그대로 호환.

set(IMGUI_ROOT    ${CMAKE_SOURCE_DIR}/extern/imgui)
set(IMGUI_BACKEND ${IMGUI_ROOT}/examples/opengl3_example)

if(NOT EXISTS ${IMGUI_ROOT}/imgui.cpp)
    message(FATAL_ERROR
        "extern/imgui/ 가 비어 있습니다. 'git submodule update --init --recursive' 를 실행하세요.")
endif()

add_library(SJH_imgui STATIC
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_BACKEND}/imgui_impl_glfw_gl3.cpp)
add_library(SJH::imgui ALIAS SJH_imgui)

# SYSTEM include — 소비자의 Debug -Werror 영향 차단.
# ${CMAKE_SOURCE_DIR}/include 추가 — sb7 gl3w + glfw3 헤더 경로 (IMPORTED 타겟 미노출).
target_include_directories(SJH_imgui SYSTEM PUBLIC
    ${IMGUI_ROOT}
    ${IMGUI_BACKEND}
    ${CMAKE_SOURCE_DIR}/include)

# Backend 가 glfw/GL 심볼을 직접 호출 — PUBLIC 으로 소비자에게도 전파
target_link_libraries(SJH_imgui PUBLIC glfw3 ${OPENGL_LIBRARIES})

# ImGui 자체 컴파일 단위만 경고 침묵 — 소비자 -Werror 는 영향 없음
if(MSVC)
    target_compile_options(SJH_imgui PRIVATE /W0 /utf-8)
else()
    target_compile_options(SJH_imgui PRIVATE -w)
endif()

# project_deps 에 합류 — INTERFACE 타겟이라 사후 추가 가능.
# 이로써 project_deps 를 링크하는 모든 챕터가 ImGui 를 자동으로 끌어옴.
target_link_libraries(project_deps INTERFACE SJH::imgui)
