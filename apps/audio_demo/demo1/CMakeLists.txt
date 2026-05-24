# 부모(audio_demo) + 현재(demo1) 조합으로 전역 고유 cmake 타겟명 생성.
# OUTPUT_NAME 으로 바이너리 파일명은 "demo1" 유지 -> CMakeExecute.sh 호환.
get_filename_component(_PARENT_DIR  ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(_PARENT_NAME ${_PARENT_DIR} NAME)
get_filename_component(_DEMO_NAME   ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(CHAPTER_NAME "${_PARENT_NAME}_${_DEMO_NAME}")

# === ImGui v1.53 — client-side 직접 통합 (Core Module 아님) ===
# 정책: "imGui는 Core Module이 아닌 Client 코드 개념으로".
# v1.53 은 GLFW(3.0.4) 호환 마지막 태그 — 다른 버전 핀 금지.
set(IMGUI_ROOT ${CMAKE_SOURCE_DIR}/extern/imgui)
set(IMGUI_GL3  ${IMGUI_ROOT}/examples/opengl3_example)
set(IMGUI_SRC
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_GL3}/imgui_impl_glfw_gl3.cpp
)

add_executable(${CHAPTER_NAME} main.cpp ${IMGUI_SRC})
set_target_properties(${CHAPTER_NAME} PROPERTIES OUTPUT_NAME "${_DEMO_NAME}")

target_include_directories(${CHAPTER_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${IMGUI_ROOT}
    ${IMGUI_GL3}
)

target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps SJH::common)

# ImGui v1.53 의 imgui.cpp / imgui_demo.cpp 는 sprintf/strcpy/memcpy 등 deprecation
# 경고가 많아 Debug 의 -Werror 와 충돌. 본 챕터 한정 -Werror 침묵.
if(NOT MSVC)
    target_compile_options(${CHAPTER_NAME} PRIVATE
        -Wno-error -Wno-deprecated-declarations -Wno-format-security)
endif()

# 챕터 리소스(`resources/banks/*.bank`) POST_BUILD copy
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)

# FMOD 동적 라이브러리 — 실행 파일 옆으로 (doc/FMOD_Setup.md §6)
if(TARGET fmod)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmod> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
endif()
if(TARGET fmodstudio)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmodstudio> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
endif()

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()
