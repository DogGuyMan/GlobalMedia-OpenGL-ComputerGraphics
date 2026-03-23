# 챕터 타겟 이름을 현재 CMakeLists.txt source 디렉토리 위치로 사용할 것
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 실행파일
if(WIN32)
    add_executable(${CHAPTER_NAME} WIN32 main.cpp)
else()
    add_executable(${CHAPTER_NAME} main.cpp)
endif()

# 라이브러리 추가
target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps)

# 인클루드 디렉토리 추가
target_include_directories(${CHAPTER_NAME} PRIVATE ${CMAKE_CURRENT_DIR})

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
endif()