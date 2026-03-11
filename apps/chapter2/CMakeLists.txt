# ______ 챕터 타겟 이름: 디렉토리명 사용 ______
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# ______ 챕터별 내부 모듈 (src/) ______
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt)
#     add_subdirectory(src)
# endif()

# ______ 실행 파일 (main.cpp + entry.cpp) ______
if(WIN32)
    add_executable(${CHAPTER_NAME} WIN32 main.cpp entry.cpp)
else()
    add_executable(${CHAPTER_NAME} main.cpp entry.cpp)
endif()

target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps SJH)
target_include_directories(${CHAPTER_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# src/ 내부 모듈이 있으면 링크
# if(TARGET ${CHAPTER_NAME}_src)
#     target_link_libraries(${CHAPTER_NAME} PRIVATE ${CHAPTER_NAME}_src)
# endif()

# ______ MSVC 전용 매크로 ______
if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
endif()

# ______ 리소스/셰이더 복사 (챕터별) ______
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources
        COMMENT "${CHAPTER_NAME} 빌드 후 리소스를 build 디렉터리에 복사")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources/shaders
        COMMENT "${CHAPTER_NAME} 빌드 후 셰이더를 resources/shaders 에 복사")
endif()
