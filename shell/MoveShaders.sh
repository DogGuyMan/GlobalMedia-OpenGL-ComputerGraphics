#!/bin/sh
# apps/ 하위의 모든 shaders/ 디렉토리를 같은 계층의 resources/shaders/로 이동
# macOS GLFW가 glfwInit() 시 cwd를 resources/로 변경하므로
# 셰이더를 resources/shaders/에 배치해야 ./shaders/ 상대경로로 접근 가능

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APPS_DIR="$ROOT_DIR/apps"

find "$APPS_DIR" -maxdepth 2 -type d -name "shaders" | while read SHADER_DIR; do
    PARENT_DIR="$(dirname "$SHADER_DIR")"
    RESOURCE_DIR="$PARENT_DIR/resources"
    TARGET_DIR="$RESOURCE_DIR/shaders"
    APP_NAME="$(basename "$PARENT_DIR")"

    # resources 디렉토리가 없으면 생성
    if [ ! -d "$RESOURCE_DIR" ]; then
        mkdir -p "$RESOURCE_DIR"
        echo "[$APP_NAME] resources/ 디렉토리 생성"
    fi

    # resources/shaders/가 이미 존재하면 기존 내용 삭제
    if [ -d "$TARGET_DIR" ]; then
        rm -rf "$TARGET_DIR"
    fi

    # shaders/ -> resources/shaders/ 이동
    mv "$SHADER_DIR" "$TARGET_DIR"
    echo "[$APP_NAME] shaders/ -> resources/shaders/ 이동 완료"
done

echo "완료: 모든 셰이더가 resources/shaders/로 이동되었습니다."
