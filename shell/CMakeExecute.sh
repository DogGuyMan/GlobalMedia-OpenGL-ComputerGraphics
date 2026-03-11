ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${1:-debug}"
TARGET_NAME="$2"

if [ -z "$TARGET_NAME" ]; then
    echo "실행할 타겟 이름을 두 번째 인자로 입력해야 합니다. (예: chapter1, chapter2)"
    exit 1
fi

if [ "$BUILD_TYPE" = "debug" ]; then
    EXEC_DIR="$ROOT_DIR/build_ninja/apps/$TARGET_NAME"
elif [ "$BUILD_TYPE" = "release" ]; then
    EXEC_DIR="$ROOT_DIR/build_ninja-release/apps/$TARGET_NAME"
else
    echo "사용법: $0 [debug|release] <타겟명>"
    exit 1
fi

# 실행 파일 디렉토리로 이동 후 실행 (리소스 상대경로 해결)
pushd "$EXEC_DIR" > /dev/null
"./$TARGET_NAME"
popd > /dev/null
