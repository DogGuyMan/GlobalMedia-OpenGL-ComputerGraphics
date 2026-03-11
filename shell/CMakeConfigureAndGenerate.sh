ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${1:-debug}"

if [ "$BUILD_TYPE" = "debug" ]; then
    cmake --preset ninja -S "$ROOT_DIR"
elif [ "$BUILD_TYPE" = "release" ]; then
    cmake --preset ninja-release -S "$ROOT_DIR"
else
    echo "사용법: $0 [debug|release] (기본값: debug)"
    exit 1
fi
