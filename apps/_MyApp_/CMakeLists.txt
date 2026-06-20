# 챕터 타겟 이름을 현재 CMakeLists.txt source 디렉토리 위치로 사용할 것
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# === ImGui v1.53 — client-side 직접 통합 (Core Module 아님) ===
# migrate_demo 와 동일 패턴. v1.53 는 GLFW(3.0.4) 호환 마지막 태그 (memory: imgui_v1_53_glfw_compat).
set(IMGUI_ROOT ${CMAKE_SOURCE_DIR}/extern/imgui)
set(IMGUI_GL3  ${IMGUI_ROOT}/examples/opengl3_example)
set(IMGUI_SRC
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_GL3}/imgui_impl_glfw_gl3.cpp
)

# 클라이언트 사이드 스크립트 (Algebraic / Entity / Input) — MyApp::Client INTERFACE 우산 노출.
add_subdirectory(src)

# ImGui 의존 UI 레이어 — client-side compile (SJH::engine 모듈 아님)
# PostFXDebugLayer.cpp 는 <imgui.h> 가 필요 ->executable PRIVATE include path 상속을 위해 직접 추가
set(UI_SRC
    src/UI/PostFXDebugLayer.cpp
    src/UI/UiBootstrap.cpp
)

# 실행파일
add_executable(${CHAPTER_NAME} main.cpp ${IMGUI_SRC} ${UI_SRC})

target_include_directories(${CHAPTER_NAME} PRIVATE
    ${CMAKE_CURRENT_DIR}
    ${IMGUI_ROOT}
    ${IMGUI_GL3}
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
)

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

# ______ Slang 셰이더 컴파일 (Phase 1 — simple.slang PoC) ______
if(SLANGC_EXECUTABLE)
    set(SLANG_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders)
    set(SIMPLE_SLANG  ${CMAKE_CURRENT_SOURCE_DIR}/shaders_slang/simple.slang)

    sjh_compile_slang(_SIMPLE_VS   ${SIMPLE_SLANG} vsMain vertex   glsl glsl_410 ${SLANG_GEN_DIR}/simple.vs)
    sjh_compile_slang(_SIMPLE_FS   ${SIMPLE_SLANG} fsMain fragment glsl glsl_410 ${SLANG_GEN_DIR}/simple.fs)
    sjh_compile_slang(_SIMPLE_WVS  ${SIMPLE_SLANG} vsMain vertex   wgsl ""       ${SLANG_GEN_DIR}/simple.vert.wgsl)
    sjh_compile_slang(_SIMPLE_WFS  ${SIMPLE_SLANG} fsMain fragment wgsl ""       ${SLANG_GEN_DIR}/simple.frag.wgsl)
    sjh_reflect_slang(_SIMPLE_REFL ${SIMPLE_SLANG} fsMain fragment ${SLANG_GEN_DIR}/simple.refl.json)

    # Phase 2.5 - phong.slang (라이팅 + LightBlock UBO, import phong_lighting 모듈). PCB 스테이지가 사용.
    set(PHONG_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/shaders_slang/phong.slang)
    sjh_compile_slang(_PHONG_VS   ${PHONG_SLANG} vsMain vertex   glsl glsl_410 ${SLANG_GEN_DIR}/phong.vs)
    sjh_compile_slang(_PHONG_FS   ${PHONG_SLANG} fsMain fragment glsl glsl_410 ${SLANG_GEN_DIR}/phong.fs)
    sjh_reflect_slang(_PHONG_REFL ${PHONG_SLANG} fsMain fragment ${SLANG_GEN_DIR}/phong.refl.json)

    # 생성 셰이더를 실행파일 옆 resources/shaders 로 overlay 를 *본 타겟의 COMMAND* 로 둔다.
    #   custom target 은 매 빌드 실행되므로, 셰이더만 바뀌어(=_MyApp_ relink 없이)도 복사가 보장된다
    #   (POST_BUILD overlay 는 target relink 시에만 실행돼 셰이더-only 변경 시 stale 산출을 남겼다 - 2026-06-21 fix).
    #   _MyApp_ 의 resources 복사(아래 POST_BUILD)는 copy_directory 가 *머지* 라 생성 phong.* 를 지우지 않는다.
    #   dest 는 $<TARGET_FILE_DIR:_MyApp_> 대신 ${CMAKE_CURRENT_BINARY_DIR} (exe 거주지와 동일) -
    #   generator expression 은 _shaders -> _MyApp_ 의존을 만들어 _MyApp_ -> _shaders 와 *사이클* 이 된다.
    add_custom_target(${CHAPTER_NAME}_shaders ALL
        DEPENDS ${_SIMPLE_VS} ${_SIMPLE_FS} ${_SIMPLE_WVS} ${_SIMPLE_WFS} ${_SIMPLE_REFL}
                ${_PHONG_VS} ${_PHONG_FS} ${_PHONG_REFL}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SLANG_GEN_DIR} ${CMAKE_CURRENT_BINARY_DIR}/resources/shaders
        COMMENT "${CHAPTER_NAME} Slang 셰이더 생성 + resources/shaders overlay")
    add_dependencies(${CHAPTER_NAME} ${CHAPTER_NAME}_shaders)
endif()

# ______ 리소스/셰이더 복사 (챕터별) ______
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources
        COMMENT "${CHAPTER_NAME} 빌드 후 리소스를 build 디렉터리에 복사")
endif()