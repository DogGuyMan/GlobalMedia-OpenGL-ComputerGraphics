get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# === ImGui v1.53 — client-side 직접 통합 (Core Module 아님) ===
# 정책 (사용자 directive): "imGui는 Core Module이 아닌 Client 코드 개념으로".
# v1.53 는 GLFW(3.0.4) 호환 마지막 태그 — 다른 버전 핀 금지 (memory: imgui_v1_53_glfw_compat).
set(IMGUI_ROOT ${CMAKE_SOURCE_DIR}/extern/imgui)
set(IMGUI_GL3  ${IMGUI_ROOT}/examples/opengl3_example)
set(IMGUI_SRC
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_GL3}/imgui_impl_glfw_gl3.cpp
)

add_executable(${CHAPTER_NAME} main.cpp ${IMGUI_SRC})

add_subdirectory(src)

target_include_directories(${CHAPTER_NAME} PRIVATE
    ${IMGUI_ROOT}
    ${IMGUI_GL3}
)

target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    game_deps
    SJH::engine
)

# ImGui v1.53 의 imgui.cpp / imgui_demo.cpp 는 sprintf/strcpy 등 deprecation 경고가 많아
# Debug 의 -Werror 와 충돌. 본 챕터 한정 -Werror 침묵.
if(NOT MSVC)
    target_compile_options(${CHAPTER_NAME} PRIVATE -Wno-error -Wno-deprecated-declarations -Wno-format-security)
endif()

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)
endif()
