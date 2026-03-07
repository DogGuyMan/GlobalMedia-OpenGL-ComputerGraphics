BUILD_TYPE="${1:-debug}"
TARGET="$2"
MEM_CHECK="$3"

if [ -z "$TARGET" ]; then
    echo "사용법: $0 [debug|release] <타겟명> [leaks]"
    echo "예시:  $0 debug chapter1"
    exit 1
fi

if [ "$BUILD_TYPE" = "debug" ]; then
    BUILD_DIR="build_ninja"
elif [ "$BUILD_TYPE" = "release" ]; then
    BUILD_DIR="build_ninja-release"
else
    echo "사용법: $0 [debug|release] <타겟명> [leaks]"
    exit 1
fi

EXECUTABLE="./$BUILD_DIR/apps/$TARGET/$TARGET"

if [ "$MEM_CHECK" = "leaks" ]; then
    MallocStackLogging=1 leaks --atExit --list -- "$EXECUTABLE"
else
    "$EXECUTABLE"
fi
