#!/usr/bin/env python3
# cmake/slang_postprocess_410.py
# slangc 의 GLSL 출력을 macOS OpenGL 4.1 (GLSL 410) 호환으로 변환한다.
#
# CMake string(REGEX) 의 클렁키함/부분토큰 오손 위험 대신 정밀한 Python 처리로 이관 (2026-06-21).
# 책임 분리: CMake(Slang.cmake) 는 오케스트레이션(slangc 호출/의존)만, 텍스트 변환은 본 스크립트가 전담.
#
# 변환 4종:
#   1) #version 450            -> #version 410 core
#   2) layout(binding = N)     단독 라인 제거 (UBO/텍스처 explicit binding 은 GLSL 420+)
#   3) layout((row|column)_major) buffer; 라인 제거 (SSBO 기본 레이아웃, GLSL 430+ — 410 거부)
#   4) inter-stage varying 이름 정규화 (--stage 필요) — 아래 상세
#
# --- (4) varying 이름 정규화: 왜 필요한가 ---
# Slang 은 stage 별로 varying 을 다른 규약으로 명명한다: vertex out = entryPointParam_<entry>_<field>_N,
# fragment in = input_<field>_N. Slang 은 location 매칭(Vulkan/SPIR-V 정통)을 가정하지만,
# macOS OpenGL 4.1 은 non-separable program 의 varying 을 location 이 아니라 *이름* 으로 매칭한다.
# -> VS-out 과 FS-in 이름이 안 맞아 "Input of fragment shader 'input_X' not written by vertex shader"
#    링크 실패 -> program=null -> 렌더 누락 (2026-06-21 PCB 투명 버그).
# 해결: vertex 의 out / fragment 의 in 을 동일한 location 기반 정규명 _slangVaryN 으로 치환.
#   -> VS-out 의 location N 과 FS-in 의 location N 이 같은 이름이 되어 macOS 이름매칭 성립.
#   (vertex 의 in=attribute / fragment 의 out=color 는 location 으로 매칭되므로 건드리지 않는다.)
#
# 사용: slang_postprocess_410.py --in raw.glsl --out final.fs --stage fragment

import argparse
import re
import sys


def normalize_varyings(text, stage):
    """stage 의 inter-stage varying 을 location 기반 _slangVaryN 으로 통일.

    vertex 는 out varying 만, fragment 는 in varying 만 치환한다
    (vertex 의 in=attribute / fragment 의 out=color 는 location 으로 매칭되므로 제외).
    """
    direction = {"vertex": "out", "fragment": "in"}.get(stage)
    if direction is None:
        return text

    # layout(location = N) <개행/공백> <dir> <type> <name>;
    decl_re = re.compile(
        r"layout\(location\s*=\s*(\d+)\)\s+" + direction + r"\s+[A-Za-z0-9_]+\s+([A-Za-z0-9_]+)\s*;"
    )

    renames = {}  # old name -> _slangVaryN
    for match in decl_re.finditer(text):
        loc, name = match.group(1), match.group(2)
        renames[name] = "_slangVary{0}".format(loc)

    # 선언 + 본문 사용처 전부 word-boundary 치환 (부분 토큰 오손 방지 - CMake REPLACE 대비 정밀).
    for old, new in renames.items():
        text = re.sub(r"\b" + re.escape(old) + r"\b", new, text)

    return text


def post_process(text, stage):
    # 1) version
    text = text.replace("#version 450", "#version 410 core")
    # 2) explicit binding 단독 라인 제거 (뒤따르는 개행까지)
    text = re.sub(r"layout\(binding\s*=\s*\d+\)\n", "", text)
    # 3) SSBO (row|column)_major buffer; 단독 라인 제거
    text = re.sub(r"layout\((?:row|column)_major\) buffer;\n", "", text)
    # 4) inter-stage varying 이름 정규화
    text = normalize_varyings(text, stage)
    return text


def main():
    parser = argparse.ArgumentParser(description="slangc GLSL 출력 -> GLSL 410 호환 post-process")
    parser.add_argument("--in", dest="infile", required=True, help="slangc raw GLSL 입력")
    parser.add_argument("--out", dest="outfile", required=True, help="410 호환 최종 출력")
    parser.add_argument("--stage", dest="stage", default="", help="vertex | fragment (varying 정규화용)")
    args = parser.parse_args()

    try:
        with open(args.infile, "r", encoding="utf-8") as handle:
            text = handle.read()
    except OSError as err:
        sys.stderr.write("[slang:410] 입력 읽기 실패: {0}\n".format(err))
        return 1

    text = post_process(text, args.stage)

    try:
        with open(args.outfile, "w", encoding="utf-8") as handle:
            handle.write(text)
    except OSError as err:
        sys.stderr.write("[slang:410] 출력 쓰기 실패: {0}\n".format(err))
        return 1

    print("[slang:410] {0}".format(args.outfile))
    return 0


if __name__ == "__main__":
    sys.exit(main())
