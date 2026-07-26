# ______ 최소 CMake 버젼 요구사항 명시 ______
cmake_minimum_required(VERSION 3.14.0)

# ______ 프로젝트 이름 지정 ______
project(OpenGL-ComputerGraphics VERSION 0.1.0 LANGUAGES C CXX)

# ______ 빌드 옵션 ______
# Doxygen 문서 타겟 등록 여부. ON 이라도 Doxygen 미설치 환경은
# cmake/Doxygen.cmake 의 find_package(Doxygen QUIET) 가 조용히 스킵.
option(SJH_OPENGL_BUILD_DOCS "Enable Doxygen doc target" ON)

# 골든 이미지 캡처 빌드 모드. ON 이면 _MyApp_ 의 진입점이 capture_application 으로 바뀐다
# (고정-dt 1/60 가상 클럭 + 180 프레임 후 PNG 14장 저장 후 종료). 씬/파이프라인은 게임과 동일 -
# 진입점 인스턴스화만 다르므로 골든은 항상 *실제 게임 경로* 를 찍는다.
#   활성화 = 전용 프리셋: cmake --preset ninja-golden   (별도 build dir 라 게임 빌드와 공존)
#   OFF 인 빌드에는 골든 ctest(golden_capture / 골든 전수 비교) 가 등록되지 않는다 - test/CMakeLists.txt.
option(SJH_GOLDEN_CAPTURE "Build _MyApp_ as the golden-image capture app" OFF)

# ______ Python 보조 스크립트 경로 등록 (Slang.cmake 등이 사용) ______
# scripts/CMakeLists.txt 가 SJH_<NAME>_SCRIPT 변수들을 부모 scope 로 export.
# Slang.cmake 보다 *먼저* 호출되어야 SJH_SLANG_COMPILE_SCRIPT 가 정의됨.
add_subdirectory(scripts)

# ______ CMake 헬퍼 모듈 ______
include(cmake/CXXStandard.cmake)

# ______ 의존성 수집 -> project_deps INTERFACE 타겟 ______
# lib/, include/ 는 build_extern.sh (또는 .bat) 로 사전 빌드
include(cmake/Dependency.cmake)


include(cmake/Slang.cmake)   # Slang tool-only 셰이더 컴파일러 (find_program + sjh_compile_slang)

# ______ Doxygen 툴체인 -> sjhopengl_setup_doxygen() 정의 ______
include(cmake/Doxygen.cmake)

# ______ 공용 라이브러리 (모든 챕터에서 사용) ______
add_subdirectory(src)

# ______ 챕터별 실행 파일 ______
add_subdirectory(apps)

# ______ 테스트 (Catch2 v3 — vcpkg manifest) ______
# 기본 OFF — 챕터 빌드 시 Catch2 컴파일 오버헤드 제외.
# 활성화: cmake --preset ninja -DENABLE_TESTING=ON
option(ENABLE_TESTING "Build unit tests (Catch2 v3)" OFF)
if(ENABLE_TESTING)
    # Catch2::Catch2 / Catch2::Catch2WithMain 타겟 + Catch.cmake(catch_discover_tests)
    # 를 vcpkg 가 노출. (과거 extern/Catch2 add_subdirectory 대체.) vcpkg 의 Catch2Config 가
    # Catch.cmake 디렉토리를 CMAKE_MODULE_PATH 에 등록하므로 test/ 의 include(Catch) 가 동작.
    find_package(Catch2 3 CONFIG REQUIRED)
    enable_testing()
    if(EXISTS ${CMAKE_SOURCE_DIR}/test/CMakeLists.txt)
        add_subdirectory(test)
    endif()
endif()

# ______ 문서 (Doxygen) ______
# `doxygen` 커스텀 타겟 등록. 빌드: cmake --build build_ninja --target doxygen
# 출력: doc/html/index.html (메인페이지 = doxygen/pages/00-mainpage.md)
if(SJH_OPENGL_BUILD_DOCS)
    sjhopengl_setup_doxygen()
endif()
