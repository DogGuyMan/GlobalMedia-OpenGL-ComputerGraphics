# 챕터 타겟 이름을 현재 CMakeLists.txt source 디렉토리 위치로 사용할 것
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# === ImGui v1.53 — 코어는 vcpkg(override 1.53), GLFW 백엔드만 vendoring ===
# vcpkg imgui 1.53 = 코어(imgui::imgui)만 제공 (find_package 는 cmake/Dependency.cmake).
# v1.53 GLFW 3.0.4 호환 백엔드(imgui_impl_glfw_gl3)는 vcpkg 에 없어 third_party/imgui 로 vendoring.
# (v1.53 = GLFW 3.0.4 호환 마지막 태그; 신버전은 3.1+ 요구 -> sb7 충돌. memory: imgui_v1_53_glfw_compat.)
set(IMGUI_BACKEND_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
set(IMGUI_SRC
    ${IMGUI_BACKEND_DIR}/imgui_impl_glfw_gl3.cpp
)

# 클라이언트 사이드 스크립트 (Algebraic / Entity / Input) — MyApp::Client INTERFACE 우산 노출.
add_subdirectory(src)

# ImGui 의존 UI 레이어 — client-side compile (SJH::engine 모듈 아님)
# <imgui.h> 가 필요 -> executable PRIVATE include path 상속을 위해 직접 추가.
# (PostFXDebugLayer 는 2026-06-24 폐기 - PassComponent 토글 UI 제거)
set(UI_SRC
    src/UI/UiBootstrap.cpp
)

# 실행파일 (골든 캡처는 SJH::Diagnostics frame_capture/pass_capture 로 이관 - CAPTURE_SRC 제거).
add_executable(${CHAPTER_NAME} main.cpp ${IMGUI_SRC} ${UI_SRC})

target_include_directories(${CHAPTER_NAME} PRIVATE
    ${CMAKE_CURRENT_DIR}
    ${IMGUI_BACKEND_DIR}   # imgui_impl_glfw_gl3.h (코어 imgui.h 는 imgui::imgui 가 전파)
)

# ImGui v1.53 deprecation 경고가 -Werror 와 충돌 — 챕터 한정 침묵.
if(NOT MSVC)
    target_compile_options(${CHAPTER_NAME} PRIVATE -Wno-error -Wno-deprecated-declarations -Wno-format-security)
endif()

# 라이브러리 추가
# MyApp::Physics 는 MyApp::Client umbrella 가 이미 포함 — 중복 link 제거 (M3 follow-up).
target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    game_deps
    SJH::engine          # M1 — SJH::sprite 등 코어 모듈 우산 link
    MyApp::Client        # 클라이언트 사이드 스크립트 (Algebraic / Entity / Input / Physics)
    # imgui::imgui (v1.53 코어) 는 game_deps 경유. GLFW 백엔드만 위 IMGUI_SRC 로 vendoring.
)

# 학습용 코드 토글 (멘탈모델 커리큘럼 docs/비어버린멘탈모델/) — 정의 시 PassDebugLayer 등의
#   학습 전용 블록(라디오 토글 / 진단 로그)이 컴파일된다. 학습 종료 후 이 정의만 지우면
#   학습 코드가 빌드에서 빠진다 (코드 삭제 불필요, 주석 처리 대체 — #ifdef MENTAL_MODEL_PHASE_2 로 가드).
target_compile_definitions(${CHAPTER_NAME} PRIVATE MENTAL_MODEL_PHASE_1 MENTAL_MODEL_PHASE_2)

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