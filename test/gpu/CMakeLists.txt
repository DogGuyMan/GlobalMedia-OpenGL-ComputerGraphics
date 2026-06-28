# test/gpu/CMakeLists.txt
# Phase C GPU 테스트 - 숨김 GL 컨텍스트 위에서 도는 별도 실행파일 `gpu_tests`.
# CPU `unit_tests`(Catch2WithMain) 와 분리: GL 컨텍스트 세션 1회 init(글로벌 리스너)이 필요.
#
# 책임:
#   C-0  GL 컨텍스트 픽스처 (gl_context_fixture.{h,cpp}) - 숨김 GLFW 창 + GL 4.1 Core + gl3w.
#   C-1  셰이더 링크검증 (test_shader_link.cpp) - 독립 재컴파일 GLSL410 .vert/.frag 를 raw GL 로 검증.
#
# 셰이더 재컴파일(TX-C1)은 프로젝트 SSOT 파이프라인(cmake/Slang.cmake 의 sjh_compile_slang)을
# 재사용한다 - post-process(varying/sampler _N/배열 brace 함정 교정)는 scripts/slang_compile.py 가 SSOT.
# 절대 재구현하지 않는다.

include(Catch) # catch_discover_tests

# ---------------------------------------------------------------------------
# TX-C1. Slang -> GLSL410 독립 재컴파일 (테스트 전용 출력 dir)
# ---------------------------------------------------------------------------
# slangc 미발견 환경(예: Windows CI)에서는 gpu_tests 를 건너뛴다 (Slang.cmake graceful 패턴 일관).
if(NOT SLANGC_EXECUTABLE)
    message(WARNING "slangc 미발견 - gpu_tests(C-0/C-1) 를 건너뜁니다 (셰이더 재컴파일 불가).")
    return()
endif()

set(_gpu_shader_src_dir ${CMAKE_SOURCE_DIR}/apps/_MyApp_/shaders_slang)
set(_gpu_shader_out_dir ${CMAKE_BINARY_DIR}/test/gpu/shaders)

# 컴파일 가능한 entry(vsMain/fsMain)를 가진 셰이더 목록.
#   phong_lighting 은 import 전용 모듈(entry 없음)이라 제외 - 프로덕션 CMakeLists 도 직접 컴파일 안 함.
set(_gpu_shaders
    billboard_atlas
    bloom
    blurring
    fog
    gamma
    grayscale_vignetting
    healthbar
    invert
    matrix_skybox
    passthrough
    phong
    sharpening
    simple
    simple_texture
    sobel
    transparent)

set(_gpu_shader_outputs "")
foreach(_name ${_gpu_shaders})
    set(_slang ${_gpu_shader_src_dir}/${_name}.slang)
    # 테스트 본문이 basename + .vert/.frag 로 페어링하도록 확장자 통일.
    sjh_compile_slang(_VS_${_name} ${_slang} vsMain vertex   glsl glsl_410 ${_gpu_shader_out_dir}/${_name}.vert)
    sjh_compile_slang(_FS_${_name} ${_slang} fsMain fragment glsl glsl_410 ${_gpu_shader_out_dir}/${_name}.frag)
    list(APPEND _gpu_shader_outputs ${_VS_${_name}} ${_FS_${_name}})
endforeach()

# 산출 셰이더를 묶는 커스텀 타겟 - gpu_tests 가 이에 의존.
add_custom_target(gpu_tests_shaders DEPENDS ${_gpu_shader_outputs}
    COMMENT "gpu_tests 용 Slang -> GLSL410 독립 재컴파일")

# ---------------------------------------------------------------------------
# C-0 + C-1. gpu_tests 실행파일
# ---------------------------------------------------------------------------
add_executable(gpu_tests
    gl_context_fixture.cpp
    test_shader_link.cpp        # C-1 셰이더 링크검증
    test_device_context_state.cpp  # C-2 DeviceContext 상태캐시 (blend 캐시 기본값 불일치 가드)
    test_roundtrip.cpp          # C-3 buffer/texture/program roundtrip
    test_state_leak.cpp         # C-4 GL 상태누수 (CaptureGLState + DiffStates)
    test_bitmap_font.cpp)       # C-5 text BitmapFont GPU (minogram 실제 로드)

# 링크: project_deps(GL/glfw) + 각 C 항목이 소비하는 SJH 모듈.
#   C-0/C-1: shader/program/diagnostics  C-2: render(+material/buffer transitive)
#   C-3: buffer/texture/program/shader   C-4: render/buffer/diagnostics
#   C-5: text(+resource_registry -> game_deps: FMOD/Effekseer 자동 합류).
target_link_libraries(gpu_tests PRIVATE
    Catch2::Catch2WithMain
    project_deps
    SJH::shader
    SJH::program
    SJH::diagnostics
    SJH::render        # C-2/C-4 DeviceContext
    SJH::buffer        # C-3 Buffer roundtrip / C-4 작업 버퍼
    SJH::texture       # C-3 Image/Texture roundtrip
    SJH::material      # C-2 Pass::RenderStateBlock
    SJH::text)         # C-5 BitmapFont (resource_registry/game_deps 자동 합류)

target_compile_features(gpu_tests PRIVATE cxx_std_17)

# 셰이더 출력 dir 주입 + basename 목록은 생성 헤더로 전달
#   (CMake 리스트의 ';' 가 compile-def 안에서 깨지는 문제를 회피 - generated 헤더가 견고).
set(_gpu_names_cpp "")
foreach(_name ${_gpu_shaders})
    string(APPEND _gpu_names_cpp "    \"${_name}\",\n")
endforeach()
set(_gpu_names_header ${CMAKE_CURRENT_BINARY_DIR}/gpu_shader_names.generated.h)
file(WRITE ${_gpu_names_header}
    "// 자동 생성 (test/gpu/CMakeLists.txt). 직접 수정 금지.\n"
    "#pragma once\n"
    "static const char *const kGpuShaderNames[] = {\n"
    "${_gpu_names_cpp}"
    "};\n")

target_include_directories(gpu_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(gpu_tests PRIVATE
    SJH_GPU_SHADER_DIR="${_gpu_shader_out_dir}"
    # C-5 - minogram BMFont 에셋 절대경로 (끝에 '/'). CWD 무관 로드.
    SJH_GPU_FONT_DIR="${CMAKE_SOURCE_DIR}/apps/_MyApp_/resources/font/")

# C-5 - SJH::text 가 resource_registry -> game_deps 로 FMOD(dynamic dylib/dll) 를 끌어온다.
#   macOS rpath = @loader_path (Dependency.cmake) 이므로 실행파일 옆에 fmod 동적 라이브러리 복사 필수.
#   (Effekseer 는 macOS STATIC 이라 복사 불필요. fmod 미설치 환경은 타겟 부재 -> 가드로 skip.)
if(TARGET fmod)
    add_custom_command(TARGET gpu_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:fmod> $<TARGET_FILE_DIR:gpu_tests>
        COMMENT "gpu_tests 옆으로 FMOD 동적 라이브러리 복사 (C-5 런타임 의존)")
endif()
if(TARGET fmodstudio)
    add_custom_command(TARGET gpu_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:fmodstudio> $<TARGET_FILE_DIR:gpu_tests>
        COMMENT "gpu_tests 옆으로 FMOD Studio 동적 라이브러리 복사")
endif()

# 셰이더 재컴파일이 테스트 빌드보다 먼저 끝나도록 결속.
add_dependencies(gpu_tests gpu_tests_shaders)

catch_discover_tests(gpu_tests)

# 부모(test/CMakeLists.txt)의 tests 우산에 합류.
if(TARGET tests)
    add_dependencies(tests gpu_tests)
endif()
