#!/usr/bin/env bash
# Doxygen.sh — cmake 로 doxygen 문서를 빌드한 뒤 localhost:<port> 로 서빙 + Chrome 자동 오픈.
#
# 사용법:
#   sh shell/Doxygen.sh            # 포트 8000 (기본)
#   sh shell/Doxygen.sh 9000       # 포트 지정
#
# 동작:
#   1) build_ninja 미구성 시 cmake --preset ninja 로 configure
#   2) doxygen 타겟 빌드 (C++ 컴파일 없음 — doxygen 실행만, 매번 재생성)
#   3) doc/html 을 python http.server 로 백그라운드 서빙
#   4) Chrome 으로 http://localhost:<port> 오픈 (Ctrl-C 로 서버 종료)
set -euo pipefail

PORT="${1:-8000}"

# 경로 — 이 스크립트는 shell/ 에 있고, 프로젝트 루트는 그 부모.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build_ninja"
HTML_DIR="$ROOT_DIR/doc/html"
URL="http://localhost:$PORT"

cd "$ROOT_DIR"

# 1) configure (build_ninja 가 없을 때만)
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "[Doxygen.sh] build_ninja 미구성 — cmake --preset ninja"
    cmake --preset ninja
fi

# 2) doxygen 타겟 빌드 (SJH_OPENGL_BUILD_DOCS=ON + cmake/Doxygen.cmake 가 등록)
echo "[Doxygen.sh] doxygen 문서 생성..."
cmake --build "$BUILD_DIR" --target doxygen

# 3) 산출물 확인
if [ ! -f "$HTML_DIR/index.html" ]; then
    echo "[Doxygen.sh] ERROR: $HTML_DIR/index.html 없음 — doxygen 빌드 실패?" >&2
    exit 1
fi

# 4) 로컬 서버 백그라운드 기동 (doc/html 루트)
echo "[Doxygen.sh] Serving $HTML_DIR  ->  $URL"
( cd "$HTML_DIR" && exec python3 -m http.server "$PORT" >/dev/null 2>&1 ) &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT INT TERM

# 서버가 떴는지 잠깐 확인 (포트 충돌 시 조기 종료)
sleep 1
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "[Doxygen.sh] ERROR: 서버 기동 실패 — 포트 $PORT 가 이미 사용 중일 수 있음." >&2
    echo "             다른 포트로: sh shell/Doxygen.sh 9000" >&2
    exit 1
fi

# 5) Chrome 오픈 (macOS / Linux)
case "$(uname -s)" in
    Darwin)
        open -a "Google Chrome" "$URL" 2>/dev/null || open "$URL"
        ;;
    Linux)
        ( google-chrome "$URL" || google-chrome-stable "$URL" || xdg-open "$URL" ) >/dev/null 2>&1 &
        ;;
    *)
        echo "[Doxygen.sh] 브라우저 자동 오픈 미지원 OS — 수동 접속: $URL"
        ;;
esac

echo "[Doxygen.sh] 서버 실행 중. 종료하려면 Ctrl-C."
wait "$SERVER_PID"
