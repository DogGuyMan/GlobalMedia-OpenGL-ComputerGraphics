# 챕터 타겟 이름을 현재 CMakeLists.txt source 디렉토리 위치로 사용할 것
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 클라이언트 사이드 스크립트 전체 — MyApp::Client INTERFACE 우산 노출.
# 소스 소유권 규율(2026-07-26): src/ 하위 .cpp 는 *전부* 각 서브디렉토리 CMakeLists 가 소유한다.
#   본 파일은 소스 경로/서드파티 경로/경고 완화 플래그를 하드코딩하지 않는다 —
#   UI 레이어(MyApp::UI)·vendored imgui 백엔드·PostFX 제어(MyApp::Render) 모두
#   src/CMakeLists.txt 를 통해 인계되고, include 경로와 컴파일 옵션은 각 타겟의
#   usage requirement 로 전파된다.
add_subdirectory(src)

# 실행파일 — 진입점 TU 하나만 소유.
# (골든 캡처는 SJH::Diagnostics frame_capture/pass_capture 로 이관 - CAPTURE_SRC 제거.)
add_executable(${CHAPTER_NAME} main.cpp)

# 라이브러리 추가
# MyApp::Physics 는 MyApp::Client umbrella 가 이미 포함 — 중복 link 제거 (M3 follow-up).
target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    game_deps
    SJH::engine          # M1 — SJH::sprite 등 코어 모듈 우산 link
    MyApp::Client        # 클라이언트 사이드 스크립트 (Algebraic / Entity / Input / Physics / UI / Render)
    # imgui::imgui (v1.53 코어) 는 game_deps 경유. GLFW 백엔드 + <imgui_impl_glfw_gl3.h>
    # include 경로는 MyApp::UI 가 PUBLIC 으로 전파 (src/UI/CMakeLists.txt).
)

# 학습용 코드 토글 (멘탈모델 커리큘럼 doc/비어버린멘탈모델/) — 정의 시 PassDebugLayer 등의
#   학습 전용 블록(라디오 토글 / 진단 로그)이 컴파일된다. 학습 종료 후 이 정의만 지우면
#   학습 코드가 빌드에서 빠진다 (코드 삭제 불필요, 주석 처리 대체 — #ifdef MENTAL_MODEL_PHASE_2 로 가드).
target_compile_definitions(${CHAPTER_NAME} PRIVATE MENTAL_MODEL_PHASE_1 MENTAL_MODEL_PHASE_2)

# 골든 캡처 빌드 (루트 option, 프리셋 ninja-golden 이 ON) — main.cpp 의 진입점 별칭이
#   game_application -> capture_application 으로 바뀐다. 씬/파이프라인 코드는 무변경.
if(SJH_GOLDEN_CAPTURE)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE SJH_GOLDEN_CAPTURE)
    message(STATUS "${CHAPTER_NAME}: 골든 캡처 빌드 (진입점 = capture_application)")
endif()

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()

# Apple ld: duplicate library 경고 silencing.
if(APPLE)
    target_link_options(${CHAPTER_NAME} PRIVATE "-Wl,-no_warn_duplicate_libraries")
endif()

# ______ Slang 셰이더 컴파일 - 책임 분리 (셰이더 디렉토리 자체 CMakeLists, 2026-06-21) ______
# shaders_slang/CMakeLists.txt 가 simple.slang/phong.slang 컴파일 + overlay 책임 일괄 보유.
# 본 분기는 SLANGC_EXECUTABLE 부재 시 sub-dir 진입 안 함 (graceful skip).
if(SLANGC_EXECUTABLE)
    add_subdirectory(shaders_slang)
endif()

# ______ 리소스/셰이더 복사 (챕터별) ______
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources
        COMMENT "${CHAPTER_NAME} 빌드 후 리소스를 build 디렉터리에 복사")
endif()