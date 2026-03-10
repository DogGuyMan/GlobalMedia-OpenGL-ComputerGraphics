# ______ 챕터 타겟 이름: 디렉토리명 사용 ______
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

if(WIN32)
    add_executable(${CHAPTER_NAME} WIN32 main.cpp)
    target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps)
    target_include_directories(${CHAPTER_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # ______ MSVC 전용 매크로 ______
    if(MSVC)
        target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    endif()
endif()