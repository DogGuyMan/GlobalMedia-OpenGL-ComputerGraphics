# 모듈별 characterization 단위 테스트. 각 exe 는 공개 헤더(계약)에만 바인딩(contract-anchoring).
# 루트 CMakeLists 의 ENABLE_TESTING 블록이 find_package(Catch2 3)+enable_testing() 후
# EXISTS 가드로 이 디렉토리를 add_subdirectory 한다.
include(Catch) # vcpkg Catch2Config -> catch_discover_tests 제공

# --- 헬퍼: test_<name>.cpp + 모듈 link + discover 를 한 줄로 ---
function(sjh_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain ${ARGN})
    target_compile_features(${name} PRIVATE cxx_std_17)
    catch_discover_tests(${name})
endfunction()

sjh_add_test(test_timer SJH::timer)
sjh_add_test(test_transform SJH::object)
sjh_add_test(test_light SJH::object)
sjh_add_test(test_sprite_uvrect SJH::sprite)
sjh_add_test(test_fsm SJH::fsm)
sjh_add_test(test_scene SJH::scene)
sjh_add_test(test_playable SJH::playable)
sjh_add_test(test_input SJH::input)
sjh_add_test(test_multiple_timer SJH::timer)
sjh_add_test(test_material SJH::material)
sjh_add_test(test_text SJH::text)
sjh_add_test(test_diagnostics_cpu SJH::diagnostics)

# 우산 타겟 - `cmake --build --preset ninja --target tests` 로 전체 일괄 빌드.
add_custom_target(tests DEPENDS
    test_timer test_transform test_light test_sprite_uvrect test_fsm
    test_scene test_playable test_input test_multiple_timer
    test_material test_text test_diagnostics_cpu)

# ============================================================================
# 골든 이미지 비교 게이트 (Phase B) -- capture(GPU) -> compare(CPU) ctest 체인
# ============================================================================
# 비교는 순수 CPU(OpenCV imgcodecs PNG 로드/쓰기 + absdiff). GL/SJH 모듈 미링크.
# 캡처(GPU)는 _MyApp_ 골든 캡처 빌드(-DSJH_GOLDEN_CAPTURE=ON, 프리셋 ninja-golden) 를 실행.
#   -> 캡처는 이제 *빌드 모드* 다. 게임 빌드의 _MyApp_ 는 캡처하지 않으므로(그냥 창이 떠서
#      ctest 가 영원히 대기) 골든 ctest 등록 자체를 SJH_GOLDEN_CAPTURE 로 게이트한다.
find_package(OpenCV REQUIRED) # OpenCV_LIBS / OpenCV_INCLUDE_DIRS (core + imgcodecs)

# 캡처 산출 dir(cwd=build/apps/_MyApp_ 기준 test/golden) / 커밋 골든 dir / diff 아티팩트 dir.
set(SJH_GOLDEN_CAPTURED_DIR "${CMAKE_BINARY_DIR}/apps/_MyApp_/test/golden")
set(SJH_GOLDEN_REF_DIR      "${CMAKE_SOURCE_DIR}/test/golden")
# 주의: golden_compare 는 실행파일명이므로 같은 이름 디렉토리를 artifact 경로로 쓰면 안 됨.
set(SJH_GOLDEN_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/test/golden_artifacts")

add_executable(golden_compare golden_compare/golden_compare.cpp)
target_link_libraries(golden_compare PRIVATE Catch2::Catch2WithMain ${OpenCV_LIBS})
target_include_directories(golden_compare PRIVATE ${OpenCV_INCLUDE_DIRS})
target_compile_features(golden_compare PRIVATE cxx_std_17)
target_compile_definitions(golden_compare PRIVATE
    SJH_GOLDEN_CAPTURED_DIR="${SJH_GOLDEN_CAPTURED_DIR}"
    SJH_GOLDEN_REF_DIR="${SJH_GOLDEN_REF_DIR}"
    SJH_GOLDEN_ARTIFACT_DIR="${SJH_GOLDEN_ARTIFACT_DIR}")

# B-2. ctest 픽스처 체인 ------------------------------------------------------
# 골든 캡처 빌드에서만 등록. 게임 빌드(-DSJH_GOLDEN_CAPTURE=OFF)에서는 _MyApp_ 가 캡처하지 않아
# 체인이 성립하지 않는다 -> 조용히 통과시키는 대신 아예 등록하지 않는다(위양성 GREEN 방지).
if(SJH_GOLDEN_CAPTURE)
    # 1) golden_capture: 캡처 빌드 _MyApp_ 실행 -> 180 프레임 후 PNG 14장 생성 후 자동 종료.
    #    FIXTURES_SETUP GOLDEN 으로 비교 케이스보다 먼저 실행 보장.
    add_test(NAME golden_capture COMMAND $<TARGET_FILE:_MyApp_>)
    set_tests_properties(golden_capture PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/apps/_MyApp_"
        FIXTURES_SETUP GOLDEN)

    # 2) golden_compare 의 케이스([golden]) -> FIXTURES_REQUIRED GOLDEN (캡처 후 비교).
    catch_discover_tests(golden_compare PROPERTIES FIXTURES_REQUIRED GOLDEN)
else()
    message(STATUS "골든 ctest 미등록 (게임 빌드) -- 게이트 실행은 `cmake --preset ninja-golden`")
endif()

# ENABLE_TESTING 시 _MyApp_(capture) + golden_compare 빌드 의존을 tests 우산에 합류.
add_dependencies(tests golden_compare _MyApp_)

# test_smoke 는 소스 정착 시에만 (현재 디렉토리 미존재 — 부재해도 빌드 진행)
if(EXISTS ${CMAKE_SOURCE_DIR}/smoke/CMakeLists.txt)
    add_subdirectory(smoke)
endif()

# ============================================================================
# Phase C — GPU 테스트 (gpu_tests: 숨김 GL 컨텍스트 + 셰이더 링크검증)
# ============================================================================
# 별도 실행파일. slangc 미발견 시 내부에서 graceful return (Slang.cmake 패턴 일관).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gpu/CMakeLists.txt)
    add_subdirectory(gpu)
endif()