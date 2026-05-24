# 부모(box2d_demo) + 현재(demo1) 조합으로 전역 고유 cmake 타겟명 생성
# OUTPUT_NAME 으로 바이너리 파일명은 "demo1" 유지 -> CMakeExecute.sh 호환
get_filename_component(_PARENT_DIR  ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(_PARENT_NAME ${_PARENT_DIR} NAME)
get_filename_component(_DEMO_NAME   ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(CHAPTER_NAME "${_PARENT_NAME}_${_DEMO_NAME}")

add_executable(${CHAPTER_NAME} main.cpp)
set_target_properties(${CHAPTER_NAME} PROPERTIES OUTPUT_NAME "${_DEMO_NAME}")

target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps box2d_common SJH::common)

target_include_directories(${CHAPTER_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()
