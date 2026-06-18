#!/usr/bin/env bash
# 전역(~/.claude/skills/)에 설치된 범용 Skill 10종을 현 프로젝트(.claude/skills/)로 파일째 복사.
# CLAUDE.md 통합 작업에서 추출한 프로젝트 독립 가치관 Skill 묶음. (무관한 MCP 헬퍼 skill 은 제외)
set -euo pipefail

# 스크립트 위치 기준으로 프로젝트 루트 산출 (shell/ 의 부모)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SRC="${HOME}/.claude/skills"
DST="${PROJECT_ROOT}/.claude/skills"

# 우리가 만든 10종만 명시 (context7-mcp / google-docs-sheets-mcp 등 무관 skill 제외)
SKILLS=(
    architecture-design-workflow
    agent-orchestration-anti-gaming
    benchmark-research-method
    code-design-review-lenses
    confidence-and-sourcing
    design-decision-discipline
    response-quality-calibration
    modular-build-discipline
    socratic-tutor
    personal-naming-conventions
)

mkdir -p "${DST}"
echo "SRC = ${SRC}"
echo "DST = ${DST}"
echo "----------------------------------------"

copied=0
for s in "${SKILLS[@]}"; do
    if [ -d "${SRC}/${s}" ]; then
        rm -rf "${DST:?}/${s}"          # 기존 동명 디렉토리 정리 후 깨끗이 복사
        cp -R "${SRC}/${s}" "${DST}/${s}"
        echo "  [OK]   ${s}"
        copied=$((copied + 1))
    else
        echo "  [SKIP] ${s} (소스 없음)"
    fi
done

echo "----------------------------------------"
echo "복사 완료: ${copied}/${#SKILLS[@]} 개 -> ${DST}"
