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
    cmake --build "$BUILD_DIR" --target "$TARGET"
else
    cmake --build "$BUILD_DIR"
fi
