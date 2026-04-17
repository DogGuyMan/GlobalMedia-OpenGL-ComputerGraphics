#!/usr/bin/env bash
# extern 라이브러리(glfw3, sb7) 빌드 스크립트 (macOS)
# 프로젝트 루트에서 실행: sh shell/BuildExternLibs.sh
# 출력: build_extern/output/macos/ (libglfw3.a, libglfw3_d.a, libsb7.a, libsb7_d.a)
#       build_extern/output/include/
# 필요한 파일을 직접 lib/macos/, include/ 로 복사하여 사용

set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SB7CODE_DIR="$ROOT_DIR/extern/sb7code"
GLFW_DIR="$SB7CODE_DIR/extern/glfw-3.0.4"
BUILD_DIR="$ROOT_DIR/build_extern"
OUTPUT_DIR="$BUILD_DIR/output"
LIB_DIR="$OUTPUT_DIR/macos"
INCLUDE_DIR="$OUTPUT_DIR/include"

echo "========================================="
echo " extern 라이브러리 빌드 (macOS)"
echo " 출력: build_extern/output/macos/"
echo "       build_extern/output/include/"
echo "========================================="

# ====== 출력 디렉토리 준비 ======
mkdir -p "$LIB_DIR"
mkdir -p "$INCLUDE_DIR"
mkdir -p "$BUILD_DIR"

# ====== 헤더 복사 ======
echo "[1/3] 헤더 복사..."

cp -r "$SB7CODE_DIR/include/"* "$INCLUDE_DIR/"

mkdir -p "$INCLUDE_DIR/GLFW"
cp "$GLFW_DIR/include/GLFW/"* "$INCLUDE_DIR/GLFW/"

# macOS: gl3w.h가 이미 OpenGL을 로드하므로 GLFW가 gl3.h를 중복 include하지 않도록 패치
sed -i '' 's/#define GLFW_INCLUDE_GLCOREARB 1/#define GLFW_INCLUDE_NONE 1/' "$INCLUDE_DIR/sb7.h"

echo "  -> $INCLUDE_DIR 에 복사 완료"

# ====== glfw3 빌드 (Release + Debug) ======
echo "[2/3] glfw3 빌드..."

build_glfw() {
    local BUILD_TYPE=$1  # Release or Debug
    local SUFFIX=$2      # "" or "_d"

    local GLFW_BUILD="$BUILD_DIR/glfw_${BUILD_TYPE}"
    cmake -S "$GLFW_DIR" -B "$GLFW_BUILD" \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DGLFW_BUILD_EXAMPLES=OFF \
        -DGLFW_BUILD_TESTS=OFF \
        -DGLFW_BUILD_DOCS=OFF \
        -DGLFW_INSTALL=OFF

    cmake --build "$GLFW_BUILD"
    cp "$GLFW_BUILD/src/libglfw3.a" "$LIB_DIR/libglfw3${SUFFIX}.a"
}

build_glfw Release ""
build_glfw Debug "_d"

echo "  -> glfw3 빌드 완료 (Release + Debug)"

# ====== sb7 빌드 (Release + Debug) ======
echo "[3/3] sb7 빌드..."

SB7_CMAKE_DIR="$BUILD_DIR/sb7_src"
mkdir -p "$SB7_CMAKE_DIR"

cat > "$SB7_CMAKE_DIR/CMakeLists.txt" << 'CMAKEOF'
cmake_minimum_required(VERSION 3.14)
project(sb7_build LANGUAGES C CXX)

set(SB7CODE_DIR "" CACHE PATH "sb7code 소스 경로")
set(GLFW_DIR "" CACHE PATH "GLFW 소스 경로")

file(GLOB SB7_SOURCES ${SB7CODE_DIR}/src/sb7/*.cpp ${SB7CODE_DIR}/src/sb7/*.c)
add_library(sb7 STATIC ${SB7_SOURCES})
target_include_directories(sb7 PRIVATE
    ${SB7CODE_DIR}/include
    ${GLFW_DIR}/include)
target_compile_options(sb7 PRIVATE -w)

if(APPLE)
    target_compile_definitions(sb7 PRIVATE __glext_h_)
endif()
CMAKEOF

build_sb7() {
    local BUILD_TYPE=$1
    local SUFFIX=$2

    local SB7_BUILD="$BUILD_DIR/sb7_${BUILD_TYPE}"
    cmake -S "$SB7_CMAKE_DIR" -B "$SB7_BUILD" \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DSB7CODE_DIR="$SB7CODE_DIR" \
        -DGLFW_DIR="$GLFW_DIR"

    cmake --build "$SB7_BUILD"
    cp "$SB7_BUILD/libsb7.a" "$LIB_DIR/libsb7${SUFFIX}.a"
}

build_sb7 Release ""
build_sb7 Debug "_d"

echo "  -> sb7 빌드 완료 (Release + Debug)"

# ====== 완료 ======
echo ""
echo "========================================="
echo " 빌드 완료!"
echo " 라이브러리: $LIB_DIR"
echo " 헤더:      $INCLUDE_DIR"
echo ""
echo " lib/macos/, include/ 로 필요한 파일을 직접 복사하세요:"
echo "   cp $LIB_DIR/* $ROOT_DIR/lib/macos/"
echo "   cp -r $INCLUDE_DIR/* $ROOT_DIR/include/"
echo "========================================="
ls -la "$LIB_DIR"
