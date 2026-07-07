ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${1:-debug}"
TARGET_NAME="$2"
LOGGING_TYPE="$3"

if [ -z "$TARGET_NAME" ]; then
    echo "사용법: $0 <debug|release> <타겟명> [leaks]"
    echo "  예: $0 debug migrate_demo"
    echo "  예: $0 debug box2d_demo/box2d_demo1"
    echo "  예: $0 debug migrate_demo leaks   # 메모리 누수 검사 (macOS leaks)"
    exit 1
fi

# 중첩 경로 지원: box2d_demo/box2d_demo1 -> EXEC_BASENAME=box2d_demo1
# 단순 경로:     migrate_demo            -> EXEC_BASENAME=migrate_demo
EXEC_BASENAME="$(basename "$TARGET_NAME")"

if [ "$BUILD_TYPE" = "debug" ]; then
    EXEC_DIR="$ROOT_DIR/build_ninja/apps/$TARGET_NAME"
elif [ "$BUILD_TYPE" = "release" ]; then
    EXEC_DIR="$ROOT_DIR/build_ninja-release/apps/$TARGET_NAME"
else
    echo "사용법: $0 [debug|release] <타겟명> [leaks]"
    exit 1
fi

# 빌드 산출물 존재 확인
if [ ! -d "$EXEC_DIR" ]; then
    echo "❌ 빌드 디렉토리가 없습니다: $EXEC_DIR"
    echo "먼저 다음 명령으로 빌드하세요:"
    echo "  cmake --build --preset ninja${BUILD_TYPE:+$([ "$BUILD_TYPE" = "release" ] && echo "-release")} --target $EXEC_BASENAME"
    exit 1
fi

if [ ! -x "$EXEC_DIR/$EXEC_BASENAME" ]; then
    echo "❌ 실행 파일이 없습니다: $EXEC_DIR/$EXEC_BASENAME"
    echo "타겟이 apps/CMakeLists.txt 에서 활성화(주석 해제)되어 있는지 확인하세요."
    exit 1
fi

# 실행 파일 디렉토리로 이동 후 실행 (리소스 상대경로 해결)
pushd "$EXEC_DIR" > /dev/null

if [ "$LOGGING_TYPE" = "leaks" ]; then
    if [ "$(uname)" != "Darwin" ]; then
        echo "!!️  leaks 옵션은 macOS 에서만 지원됩니다. (현재: $(uname))"
        popd > /dev/null
        exit 1
    fi
    echo "---------메모리 누수 검사 실행 ($EXEC_BASENAME)---------"
    MallocStackLogging=1 MallocStackLoggingNoCompact=1 leaks --atExit --list -- "./$EXEC_BASENAME" > "$ROOT_DIR/leaklog.txt"
    echo "✓ 누수 검사 결과: $ROOT_DIR/leaklog.txt"
    if command -v python3 &>/dev/null && [ -f "$ROOT_DIR/shell/leakloghandler.py" ]; then
        echo ""
        python3 "$ROOT_DIR/shell/leakloghandler.py" "$ROOT_DIR/leaklog.txt"
    fi
else
    echo "---------일반 실행 ($EXEC_BASENAME)---------"
    "./$EXEC_BASENAME" 2>&1 | tee "$ROOT_DIR/log.txt"
    echo "✓ 실행 로그 저장: $ROOT_DIR/log.txt"
fi

popd > /dev/null
