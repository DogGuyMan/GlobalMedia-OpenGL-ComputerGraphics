#!/usr/bin/env python3
# cmake/slang_compile.py
# 단일 .slang -> 산출(.vs/.fs/.wgsl/refl.json) 파이프라인.
#
# 책임 (CMake -> Python 이관 2026-06-21):
#   1) slangc 명령줄 조립 + 호출 (subprocess.run)
#   2) GLSL 산출 시 410 post-process (version/binding/major-buffer/varying 정규화)
#   3) reflection JSON 산출 분기 (-reflection-json flag)
#   4) DEPFILE (.d) 생성 - sibling .slang glob 으로 ninja 동적 의존성 추적
#   5) 출력 디렉토리 자동 생성 (os.makedirs exist_ok)
#
# CMake 의 sjh_compile_slang / sjh_reflect_slang 본문이 본 스크립트의 단일 호출로 축소된다.
# 책임 단일화로 Slang.cmake 109 -> ~32 줄 (71% 축소).
#
# --- post-process 변환 4종 (구 slang_postprocess_410.py 흡수, Q2-3 사용자 결정) ---
#   1) #version 450            -> #version 410 core
#   2) layout(binding = N)     단독 라인 제거 (UBO/텍스처 explicit binding 은 GLSL 420+)
#   3) layout((row|column)_major) buffer; 라인 제거 (SSBO 기본 레이아웃, GLSL 430+ - 410 거부)
#   4) inter-stage varying 이름 정규화 (--stage 필요)
#
# --- (4) varying 이름 정규화: 왜 필요한가 ---
# Slang 은 stage 별로 varying 을 다른 규약으로 명명한다:
#   vertex out  = entryPointParam_<entry>_<field>_N
#   fragment in = input_<field>_N
# Slang 은 location 매칭(Vulkan/SPIR-V 정통)을 가정하지만, macOS OpenGL 4.1 은
# non-separable program 의 varying 을 location 이 아니라 *이름* 으로 매칭한다.
# -> VS-out 과 FS-in 이름 불일치 -> 링크 실패 -> program=null -> 렌더 누락.
# 해결: vertex 의 out / fragment 의 in 을 같은 location 의 _slangVaryN 으로 치환.
#
# 사용:
#   slang_compile.py --slangc PATH --in foo.slang --entry vsMain --stage vertex
#                    --target glsl --profile glsl_410 --out foo.vs [--depfile foo.vs.d]
#   slang_compile.py --slangc PATH --in foo.slang --entry fsMain --stage fragment
#                    --reflection-out foo.refl.json

import argparse
import glob
import os
import re
import subprocess
import sys


# ===== post-process (GLSL 410 호환) ==========================================

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
        r"layout\(location\s*=\s*(\d+)\)\s+"
        + direction
        + r"\s+[A-Za-z0-9_]+\s+([A-Za-z0-9_]+)\s*;"
    )

    renames = {}  # old name -> _slangVaryN
    for match in decl_re.finditer(text):
        loc, name = match.group(1), match.group(2)
        renames[name] = "_slangVary{0}".format(loc)

    # 선언 + 본문 사용처 전부 word-boundary 치환 (부분 토큰 오손 방지).
    for old, new in renames.items():
        text = re.sub(r"\b" + re.escape(old) + r"\b", new, text)

    return text


def normalize_sampler_names(text):
    """Slang 이 sampler global 에 붙인 _N 접미사를 제거 (author 이름 복원).

    Slang 은 sampler global 을 'uniform sampler2D uTex_0;' 처럼 _N 접미사로 낸다.
    엔진(PropertyBlockSetter)은 sampler 를 *author 이름*('uTex' = Material.Properties.Textures 키)
    으로 바인딩하므로, _N 이 붙으면 매칭 실패 -> 텍스처 미바인딩 -> 직전 unit 잔재 샘플(오염).
    (UBO 블록은 'block_<T>_N' 을 엔진이 별도 정규화하지만 sampler 는 그 경로 밖이라 여기서 처리.)
    varying(_slangVaryN) / in / out 은 'uniform sampler' 패턴이 아니므로 무영향.
    """
    sampler_re = re.compile(r"uniform\s+sampler\w+\s+([A-Za-z_][A-Za-z0-9_]*)_(\d+)\s*;")

    renames = {}  # uTex_0 -> uTex
    for match in sampler_re.finditer(text):
        base, suffix = match.group(1), match.group(2)
        renames[base + "_" + suffix] = base

    # 선언 + 본문 사용처(texture(uTex_0, ...)) 전부 word-boundary 치환.
    for old, new in renames.items():
        text = re.sub(r"\b" + re.escape(old) + r"\b", new, text)

    return text


def post_process_glsl_410(text, stage):
    """slangc 의 GLSL 출력을 macOS OpenGL 4.1 (GLSL 410) 호환으로 변환."""
    # 1) version
    text = text.replace("#version 450", "#version 410 core")
    # 2) explicit binding 단독 라인 제거 (뒤따르는 개행까지)
    text = re.sub(r"layout\(binding\s*=\s*\d+\)\n", "", text)
    # 3) SSBO (row|column)_major buffer; 단독 라인 제거
    text = re.sub(r"layout\((?:row|column)_major\) buffer;\n", "", text)
    # 4) inter-stage varying 이름 정규화
    text = normalize_varyings(text, stage)
    # 5) sampler global _N 접미사 제거 (author 이름 복원 - 텍스처 바인딩 키 일치)
    text = normalize_sampler_names(text)
    return text


# ===== slangc 호출 =============================================================

def run_slangc(slangc, args, error_prefix):
    """slangc subprocess 호출. stderr 은 그대로 통과 (ninja 가 capture)."""
    try:
        result = subprocess.run([slangc] + args, check=False)
    except OSError as err:
        sys.stderr.write("[{0}] slangc 실행 실패: {1}\n".format(error_prefix, err))
        return 1
    if result.returncode != 0:
        sys.stderr.write(
            "[{0}] slangc 비-0 종료 ({1}). 인자: {2}\n".format(
                error_prefix, result.returncode, " ".join(args)
            )
        )
        return result.returncode
    return 0


# ===== depfile (.d) 생성 ======================================================

def write_depfile(depfile_path, target_path, dependency_paths):
    """Make 형식 .d 파일 - ninja 가 DEPFILE 옵션으로 읽어 sibling .slang 의존 동적 추적.

    형식 (gcc -MMD 호환):
        target_path: dep1.slang dep2.slang ...
    경로 공백 escape 처리 (보수적).
    """
    def esc(p):
        return p.replace(" ", "\\ ")

    body = "{0}: {1}\n".format(
        esc(target_path), " ".join(esc(d) for d in dependency_paths)
    )
    with open(depfile_path, "w", encoding="utf-8") as handle:
        handle.write(body)


def collect_sibling_slang_files(input_path):
    """입력 .slang 의 형제 .slang 들 (import 후보) - depfile/DEPENDS 용."""
    parent = os.path.dirname(os.path.abspath(input_path))
    return sorted(glob.glob(os.path.join(parent, "*.slang")))


# ===== main 파이프라인 =======================================================

def ensure_out_dir(out_path):
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)


def compile_glsl(args):
    """GLSL 산출 - 2단계: slangc -> raw -> post-process -> 최종."""
    ensure_out_dir(args.out)
    parent = os.path.dirname(os.path.abspath(args.infile))
    raw_path = args.out + ".raw"

    slangc_args = [
        args.infile,
        "-I", parent,
        "-target", "glsl",
        "-entry", args.entry,
        "-stage", args.stage,
        "-o", raw_path,
    ]
    if args.profile:
        slangc_args += ["-profile", args.profile]

    rc = run_slangc(args.slangc, slangc_args, "slang:glsl410")
    if rc != 0:
        return rc

    try:
        with open(raw_path, "r", encoding="utf-8") as handle:
            text = handle.read()
    except OSError as err:
        sys.stderr.write("[slang:glsl410] raw 읽기 실패: {0}\n".format(err))
        return 1

    text = post_process_glsl_410(text, args.stage)

    try:
        with open(args.out, "w", encoding="utf-8") as handle:
            handle.write(text)
    except OSError as err:
        sys.stderr.write("[slang:glsl410] 최종 쓰기 실패: {0}\n".format(err))
        return 1

    print("[slang:glsl410] {0}".format(args.out))
    return 0


def compile_wgsl(args):
    """WGSL 산출 - 1단계: slangc 직접."""
    ensure_out_dir(args.out)
    parent = os.path.dirname(os.path.abspath(args.infile))

    slangc_args = [
        args.infile,
        "-I", parent,
        "-target", "wgsl",
        "-entry", args.entry,
        "-stage", args.stage,
        "-o", args.out,
    ]
    if args.profile:
        slangc_args += ["-profile", args.profile]

    rc = run_slangc(args.slangc, slangc_args, "slang:wgsl")
    if rc != 0:
        return rc

    print("[slang:wgsl] {0}".format(args.out))
    return 0


def emit_reflection(args):
    """reflection JSON 산출 - slangc 의 -reflection-json + dummy .glsl 출력."""
    ensure_out_dir(args.reflection_out)
    parent = os.path.dirname(os.path.abspath(args.infile))

    slangc_args = [
        args.infile,
        "-I", parent,
        "-target", "glsl",
        "-profile", "glsl_410",
        "-entry", args.entry,
        "-stage", args.stage,
        "-reflection-json", args.reflection_out,
        "-o", args.reflection_out + ".ignore.glsl",
    ]
    rc = run_slangc(args.slangc, slangc_args, "slang:refl")
    if rc != 0:
        return rc

    print("[slang:refl] {0}".format(args.reflection_out))
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="slangc 호출 + GLSL 410 post-process + reflection + depfile 통합 파이프라인"
    )
    parser.add_argument("--slangc", required=True, help="slangc 실행파일 경로")
    parser.add_argument("--in", dest="infile", required=True, help="입력 .slang 파일")
    parser.add_argument("--entry", required=True, help="진입 함수명 (vsMain 등)")
    parser.add_argument("--stage", required=True, choices=["vertex", "fragment"], help="셰이더 스테이지")
    # compile mode 인자
    parser.add_argument("--target", choices=["glsl", "wgsl"], help="컴파일 모드 산출 타겟")
    parser.add_argument("--profile", default="", help="slangc -profile (예: glsl_410, wgsl 은 빈 문자열)")
    parser.add_argument("--out", help="컴파일 모드 출력 파일")
    parser.add_argument("--depfile", default="", help="ninja DEPFILE (.d) 출력 경로")
    # reflection mode 인자
    parser.add_argument("--reflection-out", dest="reflection_out", default="", help="reflection JSON 출력 경로 (지정 시 reflection 모드)")
    args = parser.parse_args()

    # 모드 결정
    if args.reflection_out:
        rc = emit_reflection(args)
        # reflection 모드도 depfile 생성 가능 (선택)
        target_path = args.reflection_out
    elif args.target and args.out:
        if args.target == "glsl":
            rc = compile_glsl(args)
        else:
            rc = compile_wgsl(args)
        target_path = args.out
    else:
        sys.stderr.write(
            "[slang:compile] 모드 인자 부족 - --target+--out (compile) 또는 --reflection-out (reflection) 필요\n"
        )
        return 2

    if rc != 0:
        return rc

    # depfile 생성 (성공 시점) - sibling glob 동적 추적.
    if args.depfile:
        siblings = collect_sibling_slang_files(args.infile)
        write_depfile(args.depfile, target_path, siblings)

    return 0


if __name__ == "__main__":
    sys.exit(main())
