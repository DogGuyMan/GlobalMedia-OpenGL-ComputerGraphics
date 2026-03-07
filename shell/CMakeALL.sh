BASE_DIR="$(dirname "$0")"
BUILD_TYPE="${1:-debug}"

sh "$BASE_DIR/CMakePrepare.sh"
sh "$BASE_DIR/CMakeConfigureAndGenerate.sh" "$BUILD_TYPE"
sh "$BASE_DIR/CMakeBuild.sh"               "$BUILD_TYPE"
sh "$BASE_DIR/CMakeExecute.sh"             "$BUILD_TYPE"
