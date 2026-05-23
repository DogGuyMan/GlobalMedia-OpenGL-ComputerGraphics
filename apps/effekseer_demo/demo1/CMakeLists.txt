get_filename_component(_PARENT_DIR  ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(_PARENT_NAME ${_PARENT_DIR} NAME)
get_filename_component(_DEMO_NAME   ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(CHAPTER_NAME "${_PARENT_NAME}_${_DEMO_NAME}")

add_executable(${CHAPTER_NAME} main.cpp)

add_subdirectory(src)

set_target_properties(${CHAPTER_NAME} PROPERTIES OUTPUT_NAME "${_DEMO_NAME}")

target_link_libraries(${CHAPTER_NAME} PRIVATE 
    project_deps 
    game_deps
    SJH::engine
)

target_include_directories(${CHAPTER_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()

# effekseer_demo/Resources/ -> 빌드 출력 디렉토리로 복사
# CMAKE_CURRENT_SOURCE_DIR = apps/effekseer_demo/demo1
# ../Resources             = apps/effekseer_demo/Resources
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources")
