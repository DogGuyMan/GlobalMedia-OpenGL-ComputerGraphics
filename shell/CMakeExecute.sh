PROJECT_NAME="OpenGL-ComputerGraphics"
BUILD_TYPE="${1:-debug}"
MEM_CHECK="$2"

if [ "$BUILD_TYPE" = "debug" ]; then
    BUILD_DIR="build_ninja"
elif [ "$BUILD_TYPE" = "release" ]; then
    BUILD_DIR="build_ninja-release"
else
    echo "사용법: $0 [debug|release] [leaks]"
    exit 1
fi

EXECUTABLE="./$BUILD_DIR/app/$PROJECT_NAME"

if [ "$MEM_CHECK" = "leaks" ]; then
    MallocStackLogging=1 leaks --atExit --list -- "$EXECUTABLE"
else
    "$EXECUTABLE"
fi
