# ______ 실행 파일 ______
if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 main.cpp)
else()
    add_executable(${PROJECT_NAME} main.cpp)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE project_deps)

# ______ 리소스/셰이더 복사 ______
if(EXISTS ${CMAKE_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
        COMMENT "${PROJECT_NAME} 빌드 후 리소스를 build 디렉터리에 복사")
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/shaders)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
        COMMENT "${PROJECT_NAME} 빌드 후 셰이더를 build 디렉터리에 복사")
endif()
