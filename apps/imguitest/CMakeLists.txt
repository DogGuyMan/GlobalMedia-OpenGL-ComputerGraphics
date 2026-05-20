# 챕터 타겟 이름을 현재 CMakeLists.txt source 디렉토리 위치로 사용할 것
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 실행파일 — ImGui Demo 출력 확인을 위해 콘솔 서브시스템 유지
add_executable(${CHAPTER_NAME} main.cpp)

# ImGui 는 project_deps 에 이미 합류 — game_deps 는 불필요
target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps)

# 인클루드 디렉토리 추가
target_include_directories(${CHAPTER_NAME} PRIVATE ${CMAKE_CURRENT_DIR})

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    # WinMain 진입점 유지 + 콘솔 창도 같이 띄우기 (진단용)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()
