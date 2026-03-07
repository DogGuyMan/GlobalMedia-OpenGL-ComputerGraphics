BASE_DIR="$(dirname "$0")"
BUILD_TYPE="${1:-debug}"
TARGET="$2"

if [ -z "$TARGET" ]; then
    echo "사용법: $0 [debug|release] <타겟명>"
    echo "예시:  $0 debug chapter1"
    exit 1
fi

sh "$BASE_DIR/CMakePrepare.sh"
sh "$BASE_DIR/CMakeConfigureAndGenerate.sh" "$BUILD_TYPE"
sh "$BASE_DIR/CMakeBuild.sh"               "$BUILD_TYPE" "$TARGET"
sh "$BASE_DIR/CMakeExecute.sh"             "$BUILD_TYPE" "$TARGET"
