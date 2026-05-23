ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${1:-debug}"
TARGET="$2"

if [ "$BUILD_TYPE" = "debug" ]; then
    PRESET="ninja"
    BUILD_DIR="$ROOT_DIR/build_ninja"
elif [ "$BUILD_TYPE" = "release" ]; then
    PRESET="ninja-release"
    BUILD_DIR="$ROOT_DIR/build_ninja-release"
else
    echo "사용법: $0 [debug|release] [타겟명] (기본값: debug, 전체 빌드)"
    exit 1
fi

if [ -n "$TARGET" ]; then
    # CMake 타겟명은 디렉토리 경로가 아닌 leaf 이름 (box2d_demo/box2d_demo1 -> box2d_demo1)
    TARGET_NAME="$(basename "$TARGET")"
    cmake --build "$BUILD_DIR" --target "$TARGET_NAME"
else
    cmake --build "$BUILD_DIR"
fi
