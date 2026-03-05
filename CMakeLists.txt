# ______ 최소 CMake 버젼 요구사항 명시 ______
cmake_minimum_required(VERSION 3.14.0)

# ______ 프로젝트 이름 지정 ______
project(OpenGL-ComputerGraphics VERSION 0.1.0 LANGUAGES C CXX)

# ______ CMake 헬퍼 모듈 ______
include(cmake/CXXStandard.cmake)

# ______ 의존성 수집 → project_deps INTERFACE 타겟 ______
# lib/, include/ 는 build_extern.sh (또는 .bat) 로 사전 빌드
include(cmake/Dependency.cmake)

# ______ Source modules (내부 라이브러리) ______
add_subdirectory(src)
add_subdirectory(app)