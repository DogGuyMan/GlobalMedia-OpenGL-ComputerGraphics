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
    # 중첩 경로(box2d_demo/demo1, effekseer_demo/demo1)는 '/' -> '_' 로 변환해 cmake 고유 타겟명 생성
    # 단순 경로(migrate_demo)는 변환 없이 그대로 사용
    TARGET_NAME="$(echo "$TARGET" | tr '/' '_')"
    cmake --build "$BUILD_DIR" --target "$TARGET_NAME"
else
    cmake --build "$BUILD_DIR"
fi
