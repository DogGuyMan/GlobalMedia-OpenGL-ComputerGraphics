# 챕터 타겟 이름을 현재 CMakeLists.txt source 디렉토리 위치로 사용할 것
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 실행파일
# (진단용) Windows에서 WIN32 서브시스템 대신 CONSOLE로 빌드해서
#  sb7/GLFW가 stderr로 출력하는 에러를 볼 수 있게 함
add_executable(${CHAPTER_NAME} main.cpp)

# 라이브러리 추가
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

# ______ 리소스/셰이더 복사 (챕터별) ______
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources
        COMMENT "${CHAPTER_NAME} 빌드 후 리소스를 build 디렉터리에 복사")
endif()