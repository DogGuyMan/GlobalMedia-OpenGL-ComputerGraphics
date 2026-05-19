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

# ====== Box2D 빌드 (Release + Debug) ======
echo "[+] Box2D v2.4.1 빌드..."

BOX2D_DIR="$ROOT_DIR/extern/box2d"

build_box2d() {
    local BUILD_TYPE=$1   # Release or Debug
    local SUFFIX=$2       # "" or "_d"

    local BOX2D_BUILD="$BUILD_DIR/box2d_${BUILD_TYPE}"
    cmake -S "$BOX2D_DIR" -B "$BOX2D_BUILD" \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DBOX2D_BUILD_TESTBED=OFF \
        -DBOX2D_BUILD_UNIT_TESTS=OFF \
        -DBOX2D_BUILD_DOCS=OFF

    cmake --build "$BOX2D_BUILD"

    local ARTIFACT
    ARTIFACT=$(find "$BOX2D_BUILD" -name 'libbox2d.a' | head -1)
    if [ -z "$ARTIFACT" ]; then
        echo "ERROR: libbox2d.a 를 찾을 수 없음" >&2
        exit 1
    fi
    cp "$ARTIFACT" "$LIB_DIR/libbox2d${SUFFIX}.a"
}

build_box2d Release ""
build_box2d Debug "_d"

# Box2D 공개 헤더 복사
cp -r "$BOX2D_DIR/include/box2d" "$INCLUDE_DIR/box2d"

echo "  -> Box2D 빌드 완료 (Release + Debug)"

# ====== Effekseer 빌드 (Release + Debug) ======
echo "[+] Effekseer 빌드..."

EFK_DIR="$ROOT_DIR/extern/Effekseer"
EFK_HEADER_SRC="$EFK_DIR/Dev/Cpp"   # 헤더 루트 (Effekseer/, EffekseerRendererGL/ 하위)

build_effekseer() {
    local BUILD_TYPE=$1   # Release or Debug
    local SUFFIX=$2       # "" or "_d"

    local EFK_BUILD="$BUILD_DIR/effekseer_${BUILD_TYPE}"
    # Effekseer 1.7.3.0 옵션명: BUILD_TEST(단수), BUILD_VIEWER, BUILD_EDITOR,
    # BUILD_EXAMPLES, BUILD_GL, BUILD_VULKAN, BUILD_METAL.
    # 주의 — macOS 에서 BUILD_METAL 기본값이 ON 이라 명시적으로 끄지 않으면
    # 미초기화 nested 서브모듈(3rdParty/LLGI) 을 add_subdirectory 하다 실패함.
    # -include cstddef — 신형 AppleClang/macOS SDK 의 libc++ 가 size_t 를
    # 더 이상 암묵 전파하지 않아 SIMD/Float4_NEON.h 가 bare size_t 에서 깨짐.
    # 벤더 소스를 패치하는 대신 강제 인클루드로 우회.
    cmake -S "$EFK_DIR" -B "$EFK_BUILD" \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_CXX_FLAGS="-include cstddef" \
        -DBUILD_GL=ON \
        -DBUILD_VULKAN=OFF \
        -DBUILD_METAL=OFF \
        -DBUILD_DX9=OFF -DBUILD_DX11=OFF -DBUILD_DX12=OFF \
        -DBUILD_VIEWER=OFF \
        -DBUILD_EDITOR=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_TEST=OFF \
        -DBUILD_UNITYPLUGIN=OFF \
        -DUSE_LIBPNG_LOADER=OFF \
        -DNETWORK_ENABLED=OFF

    cmake --build "$EFK_BUILD" --target Effekseer EffekseerRendererGL

    local EFK_LIB EFK_GL_LIB
    EFK_LIB=$(find "$EFK_BUILD" -name 'libEffekseer.a' | head -1)
    EFK_GL_LIB=$(find "$EFK_BUILD" -name 'libEffekseerRendererGL.a' | head -1)
    if [ -z "$EFK_LIB" ] || [ -z "$EFK_GL_LIB" ]; then
        echo "ERROR: Effekseer 정적 라이브러리를 찾을 수 없음" >&2
        exit 1
    fi
    cp "$EFK_LIB"    "$LIB_DIR/libEffekseer${SUFFIX}.a"
    cp "$EFK_GL_LIB" "$LIB_DIR/libEffekseerRendererGL${SUFFIX}.a"
}

build_effekseer Release ""
build_effekseer Debug "_d"

# Effekseer 공개 헤더만 복사 (.h — .cpp/.fx/.py/CMakeLists.txt 제외)
mkdir -p "$INCLUDE_DIR/Effekseer"
rsync -am --include='*/' --include='*.h' --exclude='*' \
    "$EFK_HEADER_SRC/Effekseer/" "$INCLUDE_DIR/Effekseer/"
rsync -am --include='*/' --include='*.h' --exclude='*' \
    "$EFK_HEADER_SRC/EffekseerRendererGL/" "$INCLUDE_DIR/Effekseer/"

echo "  -> Effekseer 빌드 완료 (Release + Debug)"

# ====== assimp 빌드 (Release + Debug) ======
echo "[+] assimp v5.4.3 빌드..."

ASSIMP_DIR="$ROOT_DIR/extern/assimp"

# 번들 zlib(zutil.h) 패치 — 신형 macOS SDK 에서 TARGET_OS_MAC 가 정의되어
# 레거시 클래식 Mac OS 블록의 '#define fdopen(fd,mode) NULL' 가 활성화되고,
# 이것이 <stdio.h> 의 진짜 fdopen 선언을 매크로 치환해 컴파일이 깨진다.
# sb7.h 패치와 동일한 방식으로 벤더 헤더를 in-place 로 보정한다 (멱등).
ZUTIL_H="$ASSIMP_DIR/contrib/zlib/zutil.h"
if grep -q 'defined(MACOS) || defined(TARGET_OS_MAC)' "$ZUTIL_H"; then
    sed -i '' 's/#if defined(MACOS) || defined(TARGET_OS_MAC)/#if defined(MACOS)/' "$ZUTIL_H"
    echo "  -> zutil.h 패치 적용 (TARGET_OS_MAC fdopen 매크로 우회)"
fi

build_assimp() {
    local BUILD_TYPE=$1   # Release or Debug
    local SUFFIX=$2       # "" or "_d"

    local ASSIMP_BUILD="$BUILD_DIR/assimp_${BUILD_TYPE}"
    # assimp v5.4.3 옵션:
    #  - ASSIMP_INJECT_DEBUG_POSTFIX 가 기본 ON → Debug 빌드는 CMAKE_DEBUG_POSTFIX=d
    #    가 적용되어 libassimpd.a / libzlibstaticd.a 가 생성됨 (find 로 탐색).
    #  - ASSIMP_BUILD_ZLIB=ON → 번들 zlib(zlibstatic) 을 함께 빌드.
    cmake -S "$ASSIMP_DIR" -B "$ASSIMP_BUILD" \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DBUILD_SHARED_LIBS=OFF \
        -DASSIMP_BUILD_TESTS=OFF \
        -DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
        -DASSIMP_BUILD_SAMPLES=OFF \
        -DASSIMP_INSTALL=OFF \
        -DASSIMP_WARNINGS_AS_ERRORS=OFF \
        -DASSIMP_BUILD_ZLIB=ON

    cmake --build "$ASSIMP_BUILD"

    # assimp 정적 라이브러리 (Release: libassimp.a, Debug: libassimpd.a)
    local ASSIMP_LIB
    ASSIMP_LIB=$(find "$ASSIMP_BUILD" -name 'libassimp*.a' | head -1)
    if [ -z "$ASSIMP_LIB" ]; then
        echo "ERROR: libassimp 정적 라이브러리를 찾을 수 없음" >&2
        exit 1
    fi
    cp "$ASSIMP_LIB" "$LIB_DIR/libassimp${SUFFIX}.a"

    # 번들 zlib 정적 라이브러리 (Release: libzlibstatic.a, Debug: libzlibstaticd.a)
    local ZLIB_LIB
    ZLIB_LIB=$(find "$ASSIMP_BUILD" -name 'libzlibstatic*.a' | head -1)
    if [ -z "$ZLIB_LIB" ]; then
        echo "  주의: 번들 zlib 정적 라이브러리를 찾지 못함 (zlib 가 다르게 링크되었을 수 있음)"
    else
        cp "$ZLIB_LIB" "$LIB_DIR/libzlibstatic${SUFFIX}.a"
    fi
}

build_assimp Release ""
build_assimp Debug "_d"

# 빌드 완료 후 zutil.h 패치를 원복해 extern/assimp 서브모듈을 깨끗한 상태로 되돌린다.
# (패치는 멱등이라 다음 실행 시 자동 재적용 — 'm extern/assimp' 오염 방지.)
git -C "$ASSIMP_DIR" checkout -- contrib/zlib/zutil.h 2>/dev/null || true

# assimp 헤더 복사: 소스 트리의 공개 헤더 → 그 위에 생성 헤더(config.h/revision.h) 덮어쓰기
mkdir -p "$INCLUDE_DIR/assimp"
cp -r "$ASSIMP_DIR/include/assimp/." "$INCLUDE_DIR/assimp/"
# config.h / revision.h 는 소스 트리에 없고 Release 빌드 트리에서 생성됨
cp "$BUILD_DIR/assimp_Release/include/assimp/config.h"   "$INCLUDE_DIR/assimp/"
cp "$BUILD_DIR/assimp_Release/include/assimp/revision.h" "$INCLUDE_DIR/assimp/"

echo "  -> assimp v5.4.3 빌드 완료 (Release + Debug)"

# ====== spdlog 빌드 (Release + Debug) ======
echo "[+] spdlog v1.17.0 빌드..."

SPDLOG_DIR="$ROOT_DIR/extern/spdlog"

build_spdlog() {
    local BUILD_TYPE=$1   # Release or Debug
    local SUFFIX=$2       # "" or "_d"

    local SPDLOG_BUILD="$BUILD_DIR/spdlog_${BUILD_TYPE}"
    # spdlog v1.17.0 옵션:
    #  - SPDLOG_DEBUG_POSTFIX "d" (기본값) → Debug 빌드는 libspdlogd.a 생성
    #  - 헤더 온리 모드가 아닌 컴파일 정적 라이브러리로 빌드 (기본 동작)
    cmake -S "$SPDLOG_DIR" -B "$SPDLOG_BUILD" \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DSPDLOG_BUILD_EXAMPLE=OFF \
        -DSPDLOG_BUILD_TESTS=OFF \
        -DSPDLOG_BUILD_BENCH=OFF

    cmake --build "$SPDLOG_BUILD"

    # spdlog 정적 라이브러리 (Release: libspdlog.a, Debug: libspdlogd.a)
    local SPDLOG_LIB
    SPDLOG_LIB=$(find "$SPDLOG_BUILD" -name 'libspdlog*.a' | head -1)
    if [ -z "$SPDLOG_LIB" ]; then
        echo "ERROR: libspdlog 정적 라이브러리를 찾을 수 없음" >&2
        exit 1
    fi
    cp "$SPDLOG_LIB" "$LIB_DIR/libspdlog${SUFFIX}.a"
}

build_spdlog Release ""
build_spdlog Debug "_d"

# spdlog 공개 헤더 복사
cp -r "$SPDLOG_DIR/include/spdlog" "$INCLUDE_DIR/spdlog"

echo "  -> spdlog v1.17.0 빌드 완료 (Release + Debug)"

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
