# ______ 최소 CMake 버젼 요구사항 명시 ______
cmake_minimum_required(VERSION 3.14.0)

# ______ 프로젝트 이름 지정 ______
project(OpenGL-ComputerGraphics VERSION 0.1.0 LANGUAGES C CXX)

# ______ CMake 헬퍼 모듈 ______
include(cmake/CXXStandard.cmake)

# ______ 의존성 수집 -> project_deps INTERFACE 타겟 ______
# lib/, include/ 는 build_extern.sh (또는 .bat) 로 사전 빌드
include(cmake/Dependency.cmake)

# ______ 공용 라이브러리 (모든 챕터에서 사용) ______
if(EXISTS ${CMAKE_SOURCE_DIR}/src/CMakeLists.txt)
    add_subdirectory(src)
endif()

# ______ 챕터별 실행 파일 ______
add_subdirectory(apps)

# ______ 테스트 (Catch2 v3 — extern/Catch2 서브모듈로 통합) ______
# 기본 OFF — 챕터 빌드 시 Catch2 컴파일 오버헤드 제외.
# 활성화: cmake --preset ninja -DENABLE_TESTING=ON
option(ENABLE_TESTING "Build unit tests (Catch2 v3)" OFF)
if(ENABLE_TESTING)
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/extern/Catch2/CMakeLists.txt)
        message(FATAL_ERROR
            "extern/Catch2 가 비어 있습니다. 'git submodule update --init --recursive' 실행 후 다시 시도하세요.")
    endif()
    # Catch2 자체 CMake — Catch2::Catch2 / Catch2::Catch2WithMain 타겟 노출
    add_subdirectory(extern/Catch2)
    # extras/Catch.cmake — include(Catch) + catch_discover_tests(...) 매크로
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/extern/Catch2/extras")
    enable_testing()
    add_subdirectory(test_smoke)
    if(EXISTS ${CMAKE_SOURCE_DIR}/test/CMakeLists.txt)
        add_subdirectory(test)
    endif()
endif()