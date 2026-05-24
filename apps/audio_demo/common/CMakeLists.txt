# audio_demo_imgui — ImGui v1.53 + GLFW/GL3 결합 backend 정적 라이브러리.
#
# 이 프로젝트에서 ImGui 를 쓰는 곳은 audio_demo 가 유일 (구 `src/imgui/` 모듈 폐기).
# box2d_demo/common 과 동일한 데모-로컬 공용 라이브러리 패턴.
#
# ImGui v1.53 핀 사유:
#   - 본 프로젝트의 GLFW(3.0.4, sb7 vendored) 와 호환되는 마지막 ImGui 태그
#   - v1.54+ 는 imgui_impl_glfw 가 GLFW 3.1+ cursor API 를 unconditional 사용
# GL 로더는 sb7 의 gl3w 재사용 — IMGUI_IMPL_OPENGL_LOADER_CUSTOM 매크로로 통보.

set(IMGUI_ROOT     ${CMAKE_SOURCE_DIR}/extern/imgui)
set(IMGUI_BACKEND  ${IMGUI_ROOT}/examples/opengl3_example)

add_library(audio_demo_imgui STATIC
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_BACKEND}/imgui_impl_glfw_gl3.cpp)

# consumer 가 <imgui.h> + <imgui_impl_glfw_gl3.h> 로 include 할 수 있도록.
# SYSTEM include — Debug -Werror 대상에서 ImGui 헤더의 경고 제외.
target_include_directories(audio_demo_imgui SYSTEM PUBLIC
    ${IMGUI_ROOT}
    ${IMGUI_BACKEND})

target_compile_definitions(audio_demo_imgui PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# ImGui v1.53 은 modern clang 의 -Wnontrivial-memcall 에 걸린다 (POD 가정 memcpy/memset).
# 라이브러리 자체 컴파일에만 적용 (consumer 는 SYSTEM include 로 이미 보호됨).
if(NOT MSVC)
    target_compile_options(audio_demo_imgui PRIVATE -Wno-error=nontrivial-memcall)
endif()

# GLFW / OpenGL 심볼은 project_deps 가 가져옴 (sb7 + glfw3 + gl3w).
target_link_libraries(audio_demo_imgui PUBLIC project_deps)
