# audio_demo — FMOD Studio + ImGui 파라미터 데모
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(${CHAPTER_NAME} main.cpp)

# project_deps (sb7 + GLFW + GL + SJH::imgui) + game_deps (fmod + fmodstudio 등)
target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps)

target_include_directories(${CHAPTER_NAME} PRIVATE ${CMAKE_CURRENT_DIR})

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
