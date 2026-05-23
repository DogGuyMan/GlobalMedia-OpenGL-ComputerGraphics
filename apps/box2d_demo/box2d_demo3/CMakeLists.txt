get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(${CHAPTER_NAME} main.cpp)

target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps box2d_common)

target_include_directories(${CHAPTER_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()
