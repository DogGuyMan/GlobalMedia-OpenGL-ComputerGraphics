# 챕터 타겟 이름을 현재 CMakeLists.txt source 디렉토리 위치로 사용할 것
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 클라이언트 사이드 스크립트 (Algebraic / Entity / Input) — MyApp::Client INTERFACE 우산 노출.
# add_executable 이전에 호출해도 무방하나, main.cpp 의 include 순서 가독성을 위해 먼저.
add_subdirectory(src)

# 실행파일
# (진단용) Windows에서 WIN32 서브시스템 대신 CONSOLE로 빌드해서
#  sb7/GLFW가 stderr로 출력하는 에러를 볼 수 있게 함
add_executable(${CHAPTER_NAME} main.cpp)

# 라이브러리 추가
target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    game_deps
    SJH::engine          # M1 — SJH::sprite 등 코어 모듈 우산 link
    MyApp::Client        # 클라이언트 사이드 스크립트 (Algebraic / Entity / Input)
    MyApp::Physics       # M3 — Box2D 물리 시스템 (Client 한정)
)

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
